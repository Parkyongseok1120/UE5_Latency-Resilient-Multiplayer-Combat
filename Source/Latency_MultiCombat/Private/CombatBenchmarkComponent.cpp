#include "CombatBenchmarkComponent.h"
#include "Components/WeaponAmmoComponent.h"
#include "Components/HealthComponent.h"
#include "CombatCharacter.h"
#include "CombatWeapon.h"
#include "Components/WeaponFireComponent.h"
#include "CombatPlayerController.h"
#include "CombatPlayerState.h"
#include "CombatGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"

UCombatBenchmarkComponent::UCombatBenchmarkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCombatBenchmarkComponent::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwner());

	if (PC && PC->IsLocalController() && GetNetMode() == NM_Client &&
		FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineBenchmark")))
	{
		bShooter = FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineShooter"));
		FParse::Value(FCommandLine::Get(), TEXT("FPSBenchmarkSamples="), DesiredSamples);
		DesiredSamples = FMath::Clamp(DesiredSamples, 8, 60);
		StartedAt = FPlatformTime::Seconds();
		SetComponentTickEnabled(true);
	}
#endif
}

void UCombatBenchmarkComponent::EndPlay(const EEndPlayReason::Type Reason)
{
#if !UE_BUILD_SHIPPING
	BindCharacter(nullptr);
#endif
	Super::EndPlay(Reason);
}

void UCombatBenchmarkComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
#if !UE_BUILD_SHIPPING
	if (bFinished)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - StartedAt > 150.0)
	{
		Finish(false, TEXT("timeout"));
		return;
	}
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwner());
	ACombatCharacter* Character = PC ? Cast<ACombatCharacter>(PC->GetPawn()) : nullptr;
	ACombatGameState* GS = GetWorld()->GetGameState<ACombatGameState>();
	if (!PC || !GS)
	{
		return;
	}
	BindCharacter(Character);
	ACombatPlayerState* Red = nullptr;
	ACombatPlayerState* Blue = nullptr;
	for (APlayerState* State : GS->PlayerArray)
	{
		ACombatPlayerState* PS = Cast<ACombatPlayerState>(State);
		if (!PS)
		{
			continue;
		}
		if (PS->Team == ETeamSide::Red)
		{
			Red = PS;
		}
		if (PS->Team == ETeamSide::Blue)
		{
			Blue = PS;
		}
	}
	// Logout and the last respawn update may arrive together. Refresh before completing.
	if (!bShooter && bReady && !Red)
	{
		if (!Blue)
		{
			return;
		}
		LastDeaths = Blue->DeathCount;
		bSawDeath |= LastDeaths > 0;
		bSawRespawn |= bSawDeath && !Blue->Health->IsDead() && Blue->Health->GetHealth() > 0.f && Blue->GetPawn() &&
			Blue->GetPawn() != InitialBluePawn.Get();
		if (!bSawRespawn || Blue->Health->IsDead())
		{
			return;
		}
		Finish(bRemoteMoved && bJumped && bSawDamage && bDeadFireBlocked && LastKills > 0 && LastKills == LastDeaths &&
				GS->RedTeamScore == LastDeaths,
			TEXT("observer complete"));
		return;
	}
	if (!Red || !Blue || !Character || !Character->IsLocallyControlled() ||
		Character->GetLocalRole() != ROLE_AutonomousProxy || !Character->GetWeapon() ||
		PC->AcknowledgedPawn != Character || !PC->IsInState(NAME_Playing) || PC->IsMoveInputIgnored())
	{
		return;
	}
	ACombatCharacter* Peer = Cast<ACombatCharacter>((bShooter ? Blue : Red)->GetPawn());
	if (!Peer)
	{
		return;
	}
	TargetState = Blue;
	if (!bReady)
	{
		if (!Character->GetCharacterMovement()->IsMovingOnGround())
		{
			return;
		}
		bReady = true;
		ReadyAt = Now;
		LocalStart = Character->GetActorLocation();
		PeerStart = Peer->GetActorLocation();
		InitialBluePawn = bShooter ? Peer : Character;
		if ((bShooter && LocalStart.X >= 0.f) || (!bShooter && LocalStart.X <= 0.f))
		{
			Finish(false, TEXT("initial team spawn on wrong side"));
			return;
		}
		MoveDirection = FVector(0, LocalStart.Y >= 0 ? -1.f : 1.f, 0);
		UE_LOG(LogTemp, Display, TEXT("[FPSBaselineBenchmark] READY Role=%s X=%.1f Y=%.1f"),
			bShooter ? TEXT("Shooter") : TEXT("Observer"), LocalStart.X, LocalStart.Y);
	}
	bRemoteMoved |= FVector::Dist2D(PeerStart, Peer->GetActorLocation()) > 75.f;
	bJumped |= Now - ReadyAt > 0.3 && Character->GetVelocity().Z > 50.f;
	if (Now - ReadyAt < 1.0)
	{
		Character->AddMovementInput(MoveDirection);
		if (Now - ReadyAt > 0.3 && !bJumped && Character->CanJump())
		{
			Character->Jump();
		}
		return;
	}
	Character->StopJumping();
	if (Now - ReadyAt < 3.0)
	{
		Character->AddMovementInput(
			FVector(0, -1, 0), FMath::Clamp(Character->GetActorLocation().Y / 200.f, -1.f, 1.f));
		return;
	}
	if (!bMovementLogged)
	{
		bMovementLogged = true;
		UE_LOG(LogTemp, Display, TEXT("[FPSBaselineBenchmark] MOVE LocalDistance=%.1f RemoteMoved=%d Jump=%d"),
			FVector::Dist2D(LocalStart, Character->GetActorLocation()), bRemoteMoved, bJumped);
	}
	PC->SetControlRotation(
		(Peer->GetActorLocation() + FVector(0, 0, 30) - Character->GetPawnViewLocation()).Rotation());
	if (!bCaptured && Now - ReadyAt > 4.0 && FParse::Param(FCommandLine::Get(), TEXT("FPSBenchmarkCapture")))
	{
		bCaptured = true;
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Codex/Week4Latest");
		IFileManager::Get().MakeDirectory(*Directory, true);
		FScreenshotRequest::RequestScreenshot(
			Directory / (bShooter ? TEXT("Shooter.png") : TEXT("Observer.png")), true, false);
	}
	if (Blue->Health->GetHealth() != LastHealth || Blue->DeathCount != LastDeaths || Red->KillCount != LastKills)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[FPSBaselineBenchmark] OBS Role=%s HP=%.1f Armor=%.1f Kills=%d Deaths=%d Score=%d"),
			bShooter ? TEXT("Shooter") : TEXT("Observer"), Blue->Health->GetHealth(), Blue->Health->GetArmor(),
			Red->KillCount, Blue->DeathCount, GS->RedTeamScore);
	}
	LastHealth = Blue->Health->GetHealth();
	LastDeaths = Blue->DeathCount;
	LastKills = Red->KillCount;
	bSawDamage |= Blue->Health->GetHealth() < Blue->Health->GetMaxHealth();
	bSawDeath |= Blue->Health->IsDead() || LastDeaths > 0;
	// A new pawn may already have received another hit before the MaxHealth snapshot arrives.
	bSawRespawn |= bSawDeath && !Blue->Health->IsDead() && Blue->Health->GetHealth() > 0.f && Blue->GetPawn() &&
		Blue->GetPawn() != InitialBluePawn.Get();
	if (!bShooter)
	{
		// Re-center after every respawn using normal movement, keeping range comparable.
		if (!Blue->Health->IsDead() && FMath::Abs(Character->GetActorLocation().Y) > 2.f)
		{
			Character->AddMovementInput(
				FVector(0, -1, 0), FMath::Clamp(Character->GetActorLocation().Y / 150.f, -1.f, 1.f));
		}
		if (Blue->Health->IsDead() && Character->GetCombatState() == ECombatState::Dead)
		{
			const int32 Ammo = Character->GetWeapon()->Ammo->GetMagazineAmmo();
			const bool bBlocked = !Character->GetWeapon()->Fire->CanFire();
			Character->GetWeapon()->Fire->FireWeapon();
			bDeadFireBlocked |= bBlocked && Character->GetWeapon()->Ammo->GetMagazineAmmo() == Ammo;
		}
		return;
	}
	if (bPending)
	{
		Sample.ObserveHealth(Blue->Health->GetHealth(), Now);
		if (Sample.IsComplete())
		{
			if (SampleCount < DesiredSamples)
			{
				++SampleCount;
				HitCount += Sample.bHit ? 1 : 0;
				UE_LOG(LogTemp, Display,
					TEXT("[FPSBaselineBenchmark] SAMPLE Shot=%u FeedbackMs=%.3f HitConfirmMs=%.3f HealthMs=%.3f "
						 "Hit=%d HealthBefore=%.3f HealthAfter=%.3f RangeCm=%.3f"),
					Sample.ShotId, Sample.FeedbackMs, Sample.HitConfirmMs, Sample.HealthMs, Sample.bHit,
					Sample.HealthBefore, Sample.HealthAfter, Sample.RangeCm);
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("[FPSBaselineBenchmark] VALIDATION Shot=%u Hit=%d"), Sample.ShotId,
					Sample.bHit);
			}
			bPending = false;
			LastCompletedAt = Now;
		}
		else if (Now - Sample.StartedAt > 5.0)
		{
			Finish(false, TEXT("shot response timeout"));
			return;
		}
		else
		{
			return;
		}
	}
	if (SampleCount >= 2 && !bReloadRequested)
	{
		bReloadRequested = true;
		ReloadTotalAmmo =
			Character->GetWeapon()->Ammo->GetMagazineAmmo() + Character->GetWeapon()->Ammo->GetReserveAmmo();
		ReloadMagazineBefore = Character->GetWeapon()->Ammo->GetMagazineAmmo();
		ReloadRequestedAt = Now;
		Character->GetWeapon()->Ammo->StartReload();
	}
	if (bReloadRequested && !bReloadVerified)
	{
		if (Character->GetCombatState() == ECombatState::Reloading)
		{
			const int32 Ammo = Character->GetWeapon()->Ammo->GetMagazineAmmo();
			const bool bBlocked = !Character->GetWeapon()->Fire->CanFire();
			Character->GetWeapon()->Fire->FireWeapon();
			bReloadStarted |= bBlocked && Ammo == Character->GetWeapon()->Ammo->GetMagazineAmmo();
		}
		else if (bReloadStarted && Character->GetWeapon()->Ammo->GetMagazineAmmo() > ReloadMagazineBefore)
		{
			bReloadVerified =
				Character->GetWeapon()->Ammo->GetMagazineAmmo() + Character->GetWeapon()->Ammo->GetReserveAmmo() ==
				ReloadTotalAmmo;
			if (!bReloadVerified)
			{
				Finish(false, TEXT("reload ammo conservation"));
				return;
			}
			UE_LOG(LogTemp, Display, TEXT("[FPSBaselineBenchmark] RELOAD PASS InputToCompleteMs=%.3f"),
				(Now - ReloadRequestedAt) * 1000.0);
		}
		if (Now - ReloadRequestedAt > 8.0 && !bReloadVerified)
		{
			Finish(false, TEXT("reload timeout"));
			return;
		}
		if (!bReloadVerified)
		{
			return;
		}
	}
	if (SampleCount >= DesiredSamples && bSawDeath && bSawRespawn && !Blue->Health->IsDead() &&
		Blue->Health->GetHealth() > 0 && LastKills == LastDeaths && GS->RedTeamScore == LastDeaths)
	{
		Finish(bRemoteMoved && bJumped && bSawDamage && bReloadVerified, TEXT("shooter complete"));
		return;
	}
	if (Now - ReadyAt >= 5.0 && Now - LastCompletedAt > 0.3 && !Blue->Health->IsDead() &&
		FMath::Abs(Peer->GetActorLocation().Y) < 5.f && Peer->GetVelocity().SizeSquared2D() < 25.f &&
		Character->GetWeapon()->Fire->CanFire())
	{
		Character->GetWeapon()->Fire->FireWeapon();
	}
#endif
}

#if !UE_BUILD_SHIPPING
void UCombatBenchmarkComponent::BindCharacter(ACombatCharacter* Character)
{
	UWeaponFireComponent* Fire = Character && Character->GetWeapon() ? Character->GetWeapon()->Fire.Get() : nullptr;
	if (BoundCharacter.Get() == Character && BoundFire.Get() == Fire)
	{
		return;
	}
	if (UWeaponFireComponent* Previous = BoundFire.Get())
	{
		Previous->OnShotSubmitted.RemoveAll(this);
		Previous->OnShotFeedback.RemoveAll(this);
		Previous->OnHitConfirmed.RemoveAll(this);
	}
	BoundCharacter = Character;
	BoundFire = Fire;
	if (Fire && bShooter)
	{
		Fire->OnShotSubmitted.AddUObject(this, &UCombatBenchmarkComponent::Submitted);
		Fire->OnShotFeedback.AddUObject(this, &UCombatBenchmarkComponent::Feedback);
		Fire->OnHitConfirmed.AddUObject(this, &UCombatBenchmarkComponent::Confirmed);
	}
}
void UCombatBenchmarkComponent::Submitted(uint32 ShotId)
{
	if (bPending || !TargetState.IsValid())
	{
		return;
	}
	Sample = FCombatShotSample();
	Sample.ShotId = ShotId;
	Sample.StartedAt = FPlatformTime::Seconds();
	Sample.HealthBefore = TargetState->Health->GetHealth();
	Sample.HealthAfter = Sample.HealthBefore;

	if (BoundCharacter.IsValid() && TargetState->GetPawn())
	{
		Sample.RangeCm = FVector::Dist(BoundCharacter->GetActorLocation(), TargetState->GetPawn()->GetActorLocation());
	}
	bPending = true;
}
void UCombatBenchmarkComponent::Feedback(uint32 ShotId, bool bDamagedPlayer)
{
	if (bPending)
	{
		Sample.Feedback(ShotId, bDamagedPlayer, FPlatformTime::Seconds());
	}
}
void UCombatBenchmarkComponent::Confirmed(uint32 ShotId)
{
	if (bPending)
	{
		Sample.Confirm(ShotId, FPlatformTime::Seconds());
	}
}
void UCombatBenchmarkComponent::Finish(bool bSuccess, const TCHAR* Reason)
{
	bFinished = true;
	UE_LOG(LogTemp, Display,
		TEXT("[FPSBaselineBenchmark] %s Role=%s Reason=%s Samples=%d Hits=%d RemoteMoved=%d Jump=%d Damage=%d "
			 "Death=%d Respawn=%d Reload=%d DeadFireBlocked=%d Kills=%d Deaths=%d"),
		bSuccess ? TEXT("PASS") : TEXT("FAIL"), bShooter ? TEXT("Shooter") : TEXT("Observer"), Reason, SampleCount,
		HitCount, bRemoteMoved, bJumped, bSawDamage, bSawDeath, bSawRespawn, bReloadVerified, bDeadFireBlocked,
		LastKills, LastDeaths);
	FPlatformMisc::RequestExit(false);
}
#endif
