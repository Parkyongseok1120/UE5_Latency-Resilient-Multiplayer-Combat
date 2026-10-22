#include "Components/WeaponHitValidationComponent.h"
#include "CombatCharacter.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Components/RewindHistoryComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Data/CombatNetworkDataAsset.h"
#include "Data/CombatRules.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool UWeaponHitValidationComponent::IntersectCapsule(
	const FVector& Start, const FVector& Direction, double Range, const FRewindFrame& Frame, double& OutDistance)
{
	const FVector Relative = Start - Frame.Location;
	const double Radius = Frame.Radius;
	const double Axis = FMath::Max(0.0, static_cast<double>(Frame.HalfHeight) - Radius);
	const double ClosestZ = FMath::Clamp(Relative.Z, -Axis, Axis);
	if ((Relative - FVector(0, 0, ClosestZ)).SizeSquared() <= Radius * Radius)
	{
		OutDistance = 0.0;
		return true;
	}
	double Best = Range + 1.0;
	const double A = Direction.X * Direction.X + Direction.Y * Direction.Y;
	const double B = Relative.X * Direction.X + Relative.Y * Direction.Y;
	const double C = Relative.X * Relative.X + Relative.Y * Relative.Y - Radius * Radius;
	const double Discriminant = B * B - A * C;
	if (A > UE_DOUBLE_SMALL_NUMBER && Discriminant >= 0.0)
	{
		const double Distance = (-B - FMath::Sqrt(Discriminant)) / A;
		if (Distance >= 0.0 && FMath::Abs(Relative.Z + Distance * Direction.Z) <= Axis)
		{
			Best = Distance;
		}
	}
	for (int32 Sign = -1; Sign <= 1; Sign += 2)
	{
		const FVector SphereRelative = Relative - FVector(0, 0, Sign * Axis);
		const double Projection = FVector::DotProduct(SphereRelative, Direction);
		const double D = Projection * Projection - (SphereRelative.SizeSquared() - Radius * Radius);
		if (D >= 0.0)
		{
			const double Distance = -Projection - FMath::Sqrt(D);
			if (Distance >= 0.0)
			{
				Best = FMath::Min(Best, Distance);
			}
		}
	}
	OutDistance = Best;
	return Best <= Range;
}

EShotRejection UWeaponHitValidationComponent::Resolve(const FCombatShotCommand& Command, FHitResult& Hit,
	FVector& Start, FVector& End, FVector& Direction, FCombatShotResponse& Response)
{
	ValidatedTarget.Reset();
	ValidatedLife.Invalidate();
	ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	ACombatCharacter* Shooter = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	if (!Shooter || !Shooter->HasAuthority())
	{
		return EShotRejection::InvalidOwner;
	}
	const FLagCompensationSettings Settings = FLagCompensationSettings::Get(GetWorld());
	const double Now = GetWorld()->GetTimeSeconds();
	const double ShotTime = FMath::Min(Command.ServerFireTime, Now);
	FRewindFrame ShooterFrame;
	if (Settings.bEnabled)
	{
		if (!Shooter->RewindHistory->Sample(ShotTime, ShooterFrame) || !ShooterFrame.bAlive ||
			ShooterFrame.LifeId != Command.LifeId)
		{
			return EShotRejection::MissingHistory;
		}
	}
	else
	{
		Shooter->RewindHistory->Capture(ShooterFrame);
	}
	Start = ShooterFrame.ViewLocation;
	const FWeaponStats& Stats = Weapon->GetStats();
	const FRotator Aim(FMath::Clamp(FRotator::NormalizeAxis(Command.AimRotation.Pitch), -89.f, 89.f),
		FRotator::NormalizeAxis(Command.AimRotation.Yaw), 0.f);
	float Spread = Stats.BaseSpread;
	if (!Weapon->Fire->IsADS())
	{
		Spread *= Shooter->GetCharacterStats().HipFireSpreadMultiplier;
	}
	if (ShooterFrame.Velocity.SizeSquared2D() > FCombatRules::MovingSpeedSquared)
	{
		Spread *= Stats.MovingSpreadMultiplier;
	}
	// 클라이언트 번호 대신 서버가 처리한 순서로 탄 퍼짐을 정함.
	FRandomStream Random(static_cast<int32>(++SpreadSequence));
	Direction = Random.VRandCone(Aim.Vector(), FMath::DegreesToRadians(FMath::Clamp(Spread, 0.f, 45.f)));
	const double Range = Stats.MaxRange * FCombatRules::CentimetersPerMeter;
	End = Start + Direction * Range;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponTrace), false, Shooter);
	Params.AddIgnoredActor(Weapon);
	Response.bUsedRewind = Settings.bEnabled;
	Response.ShotAgeSeconds = Now - ShotTime;
	if (!Settings.bEnabled)
	{
		GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	}
	else
	{
		const ACombatPlayerState* PS = Shooter->GetPlayerState<ACombatPlayerState>();
		const double ViewDelay = Shooter->IsLocallyControlled()
			? 0.0
			: FMath::Clamp((PS ? PS->GetPingInMilliseconds() * 0.0005 : 0.0) + Settings.DisplayDelaySeconds, 0.0,
				  static_cast<double>(Settings.MaxTargetViewDelay));
		const double TargetTime = ShotTime - ViewDelay;
		Response.TargetAgeSeconds = Now - TargetTime;
		// 캐릭터는 과거 캡슐로 검사하고, 엄폐물은 현재 서버 월드 기준으로 막아줌.
		for (TActorIterator<ACombatCharacter> It(GetWorld()); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		double Best = Hit.bBlockingHit ? Hit.Distance : Range;
		for (TActorIterator<ACombatCharacter> It(GetWorld()); It; ++It)
		{
			ACombatCharacter* Target = *It;
			const ACombatPlayerState* TargetPS = Target->GetPlayerState<ACombatPlayerState>();
			if (Target == Shooter || !IsValid(Target) || !Target->GetController() || !TargetPS ||
				TargetPS->Health->IsDead())
			{
				continue;
			}
			FRewindFrame Frame;
			double Distance = 0.0;
			if (Target->RewindHistory->Sample(TargetTime, Frame) && Frame.bAlive &&
				Frame.LifeId == Target->GetLifeId() && Frame.Team == TargetPS->Team &&
				IntersectCapsule(Start, Direction, Best, Frame, Distance) && Distance < Best)
			{
				Best = Distance;
				Hit = FHitResult(Target, Target->GetCapsuleComponent(), Start + Direction * Distance, -Direction);
				Hit.bBlockingHit = true;
				Hit.Distance = Distance;
				Hit.TraceStart = Start;
				Hit.TraceEnd = End;
			}
		}
	}
	if (ACombatCharacter* Target = Cast<ACombatCharacter>(Hit.GetActor()))
	{
		ValidatedTarget = Target;
		ValidatedLife = Target->GetLifeId();
	}
	if (Hit.bBlockingHit)
	{
		End = Hit.ImpactPoint;
	}
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("FPSRewindBenchmark")))
	{
		UE_LOG(LogTemp, Display, TEXT("[FPSRewindBenchmark] TRACE Shot=%u Start=%s End=%s Hit=%s"), Command.ShotId,
			*Start.ToCompactString(), *End.ToCompactString(), *GetNameSafe(Hit.GetActor()));
	}
#endif
	return EShotRejection::None;
}

float UWeaponHitValidationComponent::ApplyValidatedDamage(const FHitResult& Hit, const FVector& Direction) const
{
	ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	ACombatCharacter* Shooter = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	AActor* Target = Hit.GetActor();
	if (!Shooter || !Shooter->HasAuthority() || !IsValid(Target) || !FCombatRules::IsMatchActive(GetWorld()))
	{
		return 0.f;
	}
	if (ACombatCharacter* Character = Cast<ACombatCharacter>(Target))
	{
		const ACombatPlayerState* PS = Character->GetPlayerState<ACombatPlayerState>();
		if (ValidatedTarget.Get() != Target || ValidatedLife != Character->GetLifeId() || !Character->GetController() ||
			!PS || PS->Health->IsDead())
		{
			return 0.f;
		}
	}
	// 팀이나 사망 상태는 실제 피해를 넣는 쪽에서 한 번 더 확인함.
	return UGameplayStatics::ApplyPointDamage(Target, FCombatRules::DamageAtRange(Weapon->GetStats(), Hit.Distance),
		Direction, Hit, Shooter->GetController(), Weapon, nullptr);
}
