#include "Components/RewindBenchmarkComponent.h"
#include "CombatCharacter.h"
#include "CombatGameState.h"
#include "CombatPlayerController.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/CombatTimeSyncComponent.h"
#include "Components/HealthComponent.h"
#include "Components/RewindHistoryComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Components/WeaponHitValidationComponent.h"
#include "Data/CombatRules.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HighResScreenshot.h"
#include "Net/UnrealNetwork.h"
#include "DynamicRHI.h"
#include "RHI.h"

URewindBenchmarkComponent::URewindBenchmarkComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URewindBenchmarkComponent::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("FPSRewindBenchmark")))
	{
		FParse::Value(FCommandLine::Get(), TEXT("FPSBenchmarkSamples="), DesiredSamples);
		DesiredSamples = FMath::Clamp(DesiredSamples, 8, 24);
		StartedAt = FPlatformTime::Seconds();
		SetComponentTickEnabled(true);
	}
#endif
}

void URewindBenchmarkComponent::EndPlay(const EEndPlayReason::Type Reason)
{
#if !UE_BUILD_SHIPPING
	if (BoundFire.IsValid())
	{
		BoundFire->OnShotResult.RemoveAll(this);
	}
#endif
	Super::EndPlay(Reason);
}

void URewindBenchmarkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URewindBenchmarkComponent, Epoch);
}

void URewindBenchmarkComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
#if !UE_BUILD_SHIPPING
	if (bFinished)
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		TickServer();
	}
	else
	{
		TickClient(DeltaTime);
	}
#endif
}

#if !UE_BUILD_SHIPPING
void URewindBenchmarkComponent::TickServer()
{
	ACombatGameState* GS = Cast<ACombatGameState>(GetOwner());
	if (!GS)
	{
		return;
	}
	if (!Target.IsValid())
	{
		ACombatCharacter* Red = nullptr;
		ACombatCharacter* Blue = nullptr;
		for (APlayerState* State : GS->PlayerArray)
		{
			ACombatPlayerState* PS = Cast<ACombatPlayerState>(State);
			if (PS && PS->Team == ETeamSide::Red)
			{
				Red = Cast<ACombatCharacter>(PS->GetPawn());
			}
			if (PS && PS->Team == ETeamSide::Blue)
			{
				Blue = Cast<ACombatCharacter>(PS->GetPawn());
			}
		}
		if (!Red || !Blue || !Red->GetWeapon() || !Blue->GetWeapon())
		{
			return;
		}
		Red->GetCharacterMovement()->DisableMovement();
		Blue->GetCharacterMovement()->DisableMovement();
		Red->SetActorLocation(FVector(-900, 0, 88));
		Blue->SetActorLocation(FVector(900, 0, 88));
		Red->RewindHistory->ResetHistory();
		Blue->RewindHistory->ResetHistory();
		FCharacterStats DurableTarget = Blue->GetCharacterStats();
		DurableTarget.MaxHealth = 10000.f;
		DurableTarget.MaxArmor = 0.f;
		Blue->GetPlayerState<ACombatPlayerState>()->Health->InitializeForSpawn(DurableTarget);
		Target = Blue;
		TargetAnchor = Blue->GetActorLocation();
		Epoch = GetWorld()->GetTimeSeconds() + 3.0;
		GS->ForceNetUpdate();
		UE_LOG(LogTemp, Display,
			TEXT("[FPSRewindBenchmark] SCENARIO Rewind=%d Epoch=%.6f AmplitudeCm=240 PeriodSeconds=2.4 "
				 "IntervalSeconds=0.65 Samples=%d"),
			GS->LagCompensation.bEnabled, Epoch, DesiredSamples);
	}
	const double Phase = FMath::Max(0.0, GetWorld()->GetTimeSeconds() - Epoch);
	FVector Location = TargetAnchor;
	Location.Y += 240.0 * FMath::Sin(Phase * 2.0 * PI / 2.4);
	Target->SetActorLocation(Location);
	Target->ForceNetUpdate();
}

void URewindBenchmarkComponent::TickClient(float DeltaTime)
{
	if (FPlatformTime::Seconds() - StartedAt > 100.0)
	{
		Finish(false, TEXT("timeout"));
		return;
	}
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetWorld()->GetFirstPlayerController());
	ACombatGameState* GS = Cast<ACombatGameState>(GetOwner());
	ACombatCharacter* Pawn = PC ? Cast<ACombatCharacter>(PC->GetPawn()) : nullptr;
	if (!PC || !Pawn || !GS || !Pawn->GetWeapon() || !PC->TimeSync->IsReady() || Epoch <= 0.0 ||
		!Pawn->GetLifeId().IsValid())
	{
		return;
	}
	ACombatPlayerState* Blue = nullptr;
	for (APlayerState* State : GS->PlayerArray)
	{
		ACombatPlayerState* PS = Cast<ACombatPlayerState>(State);
		if (PS && PS->Team == ETeamSide::Blue)
		{
			Blue = PS;
		}
	}
	ACombatCharacter* Peer = Blue ? Cast<ACombatCharacter>(Blue->GetPawn()) : nullptr;
	if (!Peer)
	{
		return;
	}
	bSawMovement |= FMath::Abs(Peer->GetActorLocation().Y) > 100.f;
	bSawDamage |= Blue->Health->GetMaxHealth() > 100.f && Blue->Health->GetHealth() < Blue->Health->GetMaxHealth();
	const double Phase = PC->TimeSync->GetServerTime() - Epoch;
	if (Phase < 3.0)
	{
		return;
	}
	++FrameCount;
	FrameSeconds += DeltaTime;
	if (!GUsingNullRHI)
	{
		const uint32 Cycles = RHIGetGPUFrameCycles();
		if (Cycles > 0)
		{
			GPUSeconds += FPlatformTime::ToSeconds(Cycles);
			++GPUSamples;
		}
	}
	const bool bShooter = FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineShooter"));
	if (!bCaptured && Phase > 4.0 && FParse::Param(FCommandLine::Get(), TEXT("FPSBenchmarkCapture")))
	{
		bCaptured = true;
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Week6") /
				(bShooter ? TEXT("RewindShooter.png") : TEXT("RewindObserver.png")),
			true, false);
	}
	if (!bShooter)
	{
		if (Phase > 5.0 + DesiredSamples * 0.65 + 3.0)
		{
			Finish(bSawMovement && bSawDamage, TEXT("observer movement and health replication"));
		}
		return;
	}
	UWeaponFireComponent* Fire = Pawn->GetWeapon()->Fire;
	if (BoundFire.Get() != Fire)
	{
		if (BoundFire.IsValid())
		{
			BoundFire->OnShotResult.RemoveAll(this);
		}
		BoundFire = Fire;
		Fire->OnShotResult.AddUObject(this, &URewindBenchmarkComponent::ShotResult);
		Fire->SetADS(true);
	}
	if (bPending)
	{
		if (FPlatformTime::Seconds() - SubmittedAt > 5.0)
		{
			Finish(false, TEXT("shot acknowledgement timeout"));
		}
		return;
	}
	if (Samples >= DesiredSamples)
	{
		Finish(Rejected == 0 && bSawMovement && ActualHits > 0, TEXT("moving target samples complete"));
		return;
	}
	if (Phase < 5.0 + Samples * 0.65)
	{
		return;
	}
	if (!Fire->CanFire())
	{
		Finish(false, TEXT("unexpected fire block"));
		return;
	}
	ObservedTarget = Peer->GetActorLocation();
	FVector AimPoint = ObservedTarget;
	if (Samples % 4 == 3)
	{
		AimPoint.Z += 300.f;
	}
	const FRotator Aim = (AimPoint - Pawn->GetPawnViewLocation()).Rotation();
	PC->SetControlRotation(Aim);
	// 자동 조준 직후라 카메라 캐시도 같이 갱신해줌.
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->UpdateCamera(0.f);
	}
	PendingShotId = Samples + 1;
	const FWeaponStats& Stats = Pawn->GetWeapon()->GetStats();
	FRandomStream Random(static_cast<int32>(PendingShotId));
	const FVector Direction = Random.VRandCone(Aim.Vector(), FMath::DegreesToRadians(Stats.BaseSpread));
	FRewindFrame Displayed;
	Displayed.Location = ObservedTarget;
	Displayed.Radius = Peer->GetCapsuleComponent()->GetScaledCapsuleRadius();
	Displayed.HalfHeight = Peer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	double Distance = 0;
	bExpectedHit = UWeaponHitValidationComponent::IntersectCapsule(Pawn->GetPawnViewLocation(), Direction,
		Stats.MaxRange * FCombatRules::CentimetersPerMeter, Displayed, Distance);
	RTT = PC->TimeSync->GetRoundTripSeconds();
	TargetSpeed = 0.f; // 정해둔 이동 식으로 화면에 보이는 시점의 속도를 구함.
	const double DisplayPhase = Phase - RTT * 0.5;
	TargetSpeed = FMath::Abs(240.0 * 2.0 * PI / 2.4 * FMath::Cos(DisplayPhase * 2.0 * PI / 2.4));
	SubmittedAt = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("[FPSRewindBenchmark] AIM Shot=%u Origin=%s Target=%s"), PendingShotId,
		*Pawn->GetPawnViewLocation().ToCompactString(), *ObservedTarget.ToCompactString());
	bPending = true;
	Fire->FireWeapon();
}

void URewindBenchmarkComponent::ShotResult(const FCombatShotResponse& Response)
{
	if (!bPending || Response.ShotId != PendingShotId)
	{
		return;
	}
	const bool bActual = Response.bDamagedPlayer;
	++Samples;
	ExpectedHits += bExpectedHit ? 1 : 0;
	ActualHits += bActual ? 1 : 0;
	FalsePositive += bActual && !bExpectedHit ? 1 : 0;
	FalseNegative += !bActual && bExpectedHit ? 1 : 0;
	Rejected += Response.Rejection != EShotRejection::None ? 1 : 0;
	UE_LOG(LogTemp, Display,
		TEXT("[FPSRewindBenchmark] SAMPLE Shot=%u Rewind=%d Expected=%d Actual=%d Rejection=%d AckMs=%.3f RttMs=%.3f "
			 "ShotAgeMs=%.3f TargetAgeMs=%.3f TargetY=%.3f SpeedCmS=%.3f"),
		Response.ShotId, Response.bUsedRewind, bExpectedHit, bActual, static_cast<int32>(Response.Rejection),
		(FPlatformTime::Seconds() - SubmittedAt) * 1000.0, RTT * 1000.f, Response.ShotAgeSeconds * 1000.f,
		Response.TargetAgeSeconds * 1000.f, ObservedTarget.Y, TargetSpeed);
	bPending = false;
}

void URewindBenchmarkComponent::Finish(bool bSuccess, const TCHAR* Reason)
{
	bFinished = true;
	UE_LOG(LogTemp, Display,
		TEXT("[FPSRewindBenchmark] %s Reason=%s Samples=%d Expected=%d Hits=%d FalsePositive=%d FalseNegative=%d "
			 "Rejected=%d Movement=%d Damage=%d AverageFrameMs=%.3f Rendered=%d AverageGpuMs=%.3f GpuSamples=%d"),
		bSuccess ? TEXT("PASS") : TEXT("FAIL"), Reason, Samples, ExpectedHits, ActualHits, FalsePositive, FalseNegative,
		Rejected, bSawMovement, bSawDamage, FrameCount > 0 ? FrameSeconds / FrameCount * 1000.0 : 0.0,
		GUsingNullRHI ? 0 : 1, GPUSamples > 0 ? GPUSeconds / GPUSamples * 1000.0 : -1.0, GPUSamples);
	FPlatformMisc::RequestExit(false);
}
#endif
