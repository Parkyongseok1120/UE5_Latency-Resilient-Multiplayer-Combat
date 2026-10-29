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
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/UWeaponDataAsset.h"
#include "HAL/IConsoleManager.h"
#include "Animation/CombatAnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CombatTelemetryComponent.h"
#include "Components/CombatTimeSyncComponent.h"
#include "Engine/NetDriver.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/CharacterPresentationComponent.h"
#include "Components/WeaponPresentationComponent.h"

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
		(FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineBenchmark")) ||
			FParse::Param(FCommandLine::Get(), TEXT("FPSLiveExperimentCheck"))))
	{
		bLiveExperimentCheck = FParse::Param(FCommandLine::Get(), TEXT("FPSLiveExperimentCheck"));
		bShooter = FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineShooter"));
		bPresentationCheck = FParse::Param(FCommandLine::Get(), TEXT("FPSPresentationCheck"));
		bAimIKCheck = FParse::Param(FCommandLine::Get(), TEXT("FPSAimIKCheck"));
		bCombatFeedbackCheck = FParse::Param(FCommandLine::Get(), TEXT("FPSCombatFeedbackCheck"));
		bMovingAimCheck = FParse::Param(FCommandLine::Get(), TEXT("FPSMovingAimCheck"));
		bFireModeCheck = FParse::Param(FCommandLine::Get(), TEXT("FPSFireModeCheck"));
		if (bPresentationCheck)
		{
			// Capture the game viewport without compiling every debug buffer material.
			if (IConsoleVariable* Targets =
					IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationOverviewTargets")))
			{
				Targets->Set(TEXT(""), ECVF_SetByCode);
			}
		}
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
	if (bLiveExperimentCheck)
	{
		CheckLiveExperiment();
		return;
	}
	if (bFinished)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	// The other client can disconnect as soon as its own presentation check passes.
	if (bPresentationCheck && bPresentationReady && bPresentationShot && (!bAimIKCheck || (bSawAimUp && bSawAimDown)) &&
		Now - ReadyAt > (bAimIKCheck ? 10.0 : 8.0))
	{
		Finish(true, TEXT("upright mesh, replicated weapon and firing animation verified"));
		return;
	}
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
	if ((bAimIKCheck || bMovingAimCheck) && Character)
	{
		// The owner's body is normally hidden and can skip animation ticks. Evaluate it for comparison
		// with the remote simulated proxy during this opt-in development check.
		Character->GetMesh()->VisibilityBasedAnimTickOption =
			EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		if (!bShooter && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->SetFOV(35.f);
		}
	}
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
	if (bCombatFeedbackCheck)
	{
		CheckCombatFeedback(Character, Peer);
	}
	if (bMovingAimCheck || bFireModeCheck)
	{
		CheckMovingPresentation(Character, Peer);
	}
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
	FRotator Aim = (Peer->GetActorLocation() + FVector(0, 0, 30) - Character->GetPawnViewLocation()).Rotation();
	if (bAimIKCheck && bShooter && Now - ReadyAt > 5.0 && Now - ReadyAt < 9.0)
	{
		Aim.Pitch = Now - ReadyAt < 7.0 ? 45.f : -45.f;
	}
	PC->SetControlRotation(Aim);
	if (bFireModeCheck && bShooter && !bFireModesVerified)
	{
		Aim.Pitch += 20.f;
		PC->SetControlRotation(Aim);
		CheckFireModes(Character);
		return;
	}
	if (bMovingAimCheck && bShooter && Now - ReadyAt < 6.5)
	{
		if (Now - ReadyAt < 5.0)
		{
			// Miss above the peer while checking running fire; normal damage samples follow this stage.
			Aim.Pitch += 15.f;
			PC->SetControlRotation(Aim);
			Character->AddMovementInput(MoveDirection);
			if (Now - MovingFireAt > 0.25 && Character->GetWeapon()->Fire->CanFire())
			{
				MovingFireAt = Now;
				Character->GetWeapon()->Fire->FireWeapon();
			}
		}
		else
		{
			Character->AddMovementInput(
				FVector(0, -1, 0), FMath::Clamp(Character->GetActorLocation().Y / 150.f, -1.f, 1.f));
		}
		return;
	}
	if (bPresentationCheck && Now - ReadyAt > 4.0)
	{
		CheckPresentation(Character, Peer);
		if (bAimIKCheck && !bFinished)
		{
			CheckAimIK(Character, Peer);
		}
		if (bFinished)
		{
			return;
		}
		if (bShooter && FParse::Param(FCommandLine::Get(), TEXT("FPSHUDSlideCapture")))
		{
			CaptureHUDSlide(PC, Now - ReadyAt);
		}
	}
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
	if (SampleCount >= 2 && !bReloadRequested && !bAimIKCheck)
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
void UCombatBenchmarkComponent::CheckFireModes(ACombatCharacter* Character)
{
	const ACombatPlayerController* PC = CastChecked<ACombatPlayerController>(GetOwner());
	if (!PC->TimeSync->IsReady())
	{
		return;
	}
	ACombatWeapon* Weapon = Character->GetWeapon();
	const double Now = FPlatformTime::Seconds();
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CombatPresentation/FireModes");
	if (FireModeStage == 0)
	{
		Weapon->Fire->ToggleFireMode();
		FireModeAmmoBefore = Weapon->Ammo->GetMagazineAmmo();
		Weapon->Fire->SetTriggerHeld(true);
		FireModeStageAt = Now;
		FireModeStage = 1;
	}
	else if (FireModeStage == 1 && Now - FireModeStageAt > 0.8)
	{
		Weapon->Fire->SetTriggerHeld(false);
		const int32 Rounds = FireModeAmmoBefore - Weapon->Ammo->GetMagazineAmmo();
		if (Weapon->Fire->IsAutomaticFire() || Rounds != 1)
		{
			Finish(false, TEXT("single mode did not produce exactly one round while held"));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[FireModeCheck] SINGLE Held=0.8s Rounds=%d"), Rounds);
		FScreenshotRequest::RequestScreenshot(Directory / TEXT("Single.png"), false, false);
		FireModeAmmoBefore = Weapon->Ammo->GetMagazineAmmo();
		FireModeStageAt = Now;
		FireModeStage = 2;
	}
	else if (FireModeStage == 2 && Now - FireModeStageAt > 0.12)
	{
		Weapon->Fire->ToggleFireMode();
		Weapon->Fire->SetTriggerHeld(true);
		FireModeStageAt = Now;
		FireModeStage = 3;
	}
	else if (FireModeStage == 3 && Now - FireModeStageAt > 0.8)
	{
		Weapon->Fire->SetTriggerHeld(false);
		FireModeStageAt = Now;
		FireModeStage = 4;
	}
	else if (FireModeStage == 4 && Now - FireModeStageAt > 0.5)
	{
		const int32 Rounds = FireModeAmmoBefore - Weapon->Ammo->GetMagazineAmmo();
		UE_LOG(LogTemp, Display, TEXT("[FireModeCheck] AUTO CHECK Mode=%d Rounds=%d AmmoBefore=%d AmmoNow=%d"),
			Weapon->Fire->IsAutomaticFire(), Rounds, FireModeAmmoBefore, Weapon->Ammo->GetMagazineAmmo());
		if (!Weapon->Fire->IsAutomaticFire() || Rounds < 3)
		{
			Finish(false, TEXT("automatic mode did not produce a held burst"));
			return;
		}
		bFireModesVerified = true;
		UE_LOG(LogTemp, Display, TEXT("[FireModeCheck] AUTO Held=0.8s Rounds=%d ArmsVisible=%d TotalInitialAmmo=360"),
			Rounds, Character->FirstPersonArms->IsVisible());
		FScreenshotRequest::RequestScreenshot(Directory / TEXT("Automatic.png"), false, false);
	}
}

void UCombatBenchmarkComponent::CheckMovingPresentation(ACombatCharacter* Character, ACombatCharacter* Peer)
{
	ACombatCharacter* Shooter = bShooter ? Character : Peer;
	ACombatWeapon* Weapon = Shooter->GetWeapon();
	UAnimInstance* Anim = Shooter->GetMesh()->GetAnimInstance();
	if (!Weapon || !Weapon->WeaponData || !Anim)
	{
		return;
	}
	const UWeaponDataAsset* Data = Weapon->WeaponData.Get();
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CombatPresentation/MovingReload");
	if (!bShooter && !bCapturedMovingFire && Shooter->GetVelocity().Size2D() > 150.f &&
		Anim->IsPlayingSlotAnimation(Data->CharacterFireAnimation.Get(), Data->CharacterAnimationSlot) &&
		Anim->GetSlotMontageGlobalWeight(Data->CharacterAnimationSlot) > 0.1f)
	{
		Shooter->GetMesh()->GetBoneSpaceTransforms();
		const FVector Barrel =
			Weapon->ThirdPersonMesh->GetSocketTransform(TEXT("Muzzle")).GetRotation().GetForwardVector();
		const float Error = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(FVector::DotProduct(Barrel, Shooter->GetBaseAimRotation().Vector()), -1.0, 1.0)));
		UE_LOG(LogTemp, Display, TEXT("[MovingReloadCheck] REMOTE FIRE Speed=%.1f BarrelErrorDeg=%.3f"),
			Shooter->GetVelocity().Size2D(), Error);
		if (!FMath::IsFinite(Error) || Error > 8.f)
		{
			Finish(false, TEXT("remote running fire points away from view"));
			return;
		}
		bCapturedMovingFire = true;
		FScreenshotRequest::RequestScreenshot(Directory / TEXT("RemoteMovingFire.png"), false, false);
	}
	if (Shooter->GetCombatState() == ECombatState::Reloading && ReloadPoseSeenAt == 0.0)
	{
		ReloadPoseSeenAt = FPlatformTime::Seconds();
	}
	if (!bCapturedReloadPose && ReloadPoseSeenAt > 0.0 && FPlatformTime::Seconds() - ReloadPoseSeenAt > 1.0 &&
		Weapon->Presentation->IsReloadAnimationPlaying() &&
		Anim->IsPlayingSlotAnimation(Data->CharacterReloadAnimation.Get(), Data->CharacterAnimationSlot))
	{
		bCapturedReloadPose = true;
		UE_LOG(LogTemp, Display, TEXT("[MovingReloadCheck] %s RELOAD Animation=%s SlotWeight=%.3f"),
			bShooter ? TEXT("LOCAL") : TEXT("REMOTE"), *GetNameSafe(Data->CharacterReloadAnimation.Get()),
			Anim->GetSlotMontageGlobalWeight(Data->CharacterAnimationSlot));
		FScreenshotRequest::RequestScreenshot(
			Directory / (bShooter ? TEXT("LocalReload.png") : TEXT("RemoteReload.png")), false, false);
	}
}

void UCombatBenchmarkComponent::CheckCombatFeedback(ACombatCharacter* Character, ACombatCharacter* Peer)
{
	ACombatPlayerController* PC = CastChecked<ACombatPlayerController>(GetOwner());
	UCombatFeedbackComponent* Feedback = PC->Feedback;
	Feedback->UpdateFeedback(0.f);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CombatPresentation/Feedback");
	const UHealthComponent* Health = PC->GetPlayerState<ACombatPlayerState>()->Health;
	if (!bShooter && !bCapturedHalfHealth && FMath::IsNearlyEqual(Health->GetHealth(), 50.f) &&
		Feedback->GetLowHealthIntensity() > 0.15f)
	{
		bCapturedHalfHealth = true;
		UE_LOG(LogTemp, Display, TEXT("[CombatFeedbackCheck] Victim THRESHOLD Health=%.1f Glitch=%.3f"),
			Health->GetHealth(), Feedback->GetLowHealthIntensity());
		FScreenshotRequest::RequestScreenshot(Directory / TEXT("Victim50HP.png"), false, false);
	}
	if (!bShooter && !bCapturedDamage && Feedback->GetHitFlash() > 0.85f && DamageSeenAt == 0.0)
	{
		DamageSeenAt = FPlatformTime::Seconds();
	}
	if (!bShooter && !bCapturedDamage && DamageSeenAt > 0.0 && FPlatformTime::Seconds() - DamageSeenAt > 0.05)
	{
		bCapturedDamage = true;
		UE_LOG(LogTemp, Display, TEXT("[CombatFeedbackCheck] Victim HIT Flash=%.2f Health=%.1f"),
			Feedback->GetHitFlash(), Health->GetHealth());
		FScreenshotRequest::RequestScreenshot(Directory / TEXT("VictimHit.png"), false, false);
	}
	if (!bShooter && !Health->IsDead() && Health->GetHealth() < 40.f && LowHealthSeenAt == 0.0)
	{
		LowHealthSeenAt = FPlatformTime::Seconds();
	}
	if (!bShooter && !bCapturedLowHealth && !Health->IsDead() && Health->GetHealth() < 40.f &&
		FPlatformTime::Seconds() - LowHealthSeenAt > 0.3 && Feedback->GetLowHealthIntensity() > 0.25f)
	{
		bCapturedLowHealth = true;
		UE_LOG(LogTemp, Display, TEXT("[CombatFeedbackCheck] Victim LOW Health=%.1f Glitch=%.3f"), Health->GetHealth(),
			Feedback->GetLowHealthIntensity());
		FScreenshotRequest::RequestScreenshot(Directory / TEXT("VictimLowHealth.png"), false, false);
	}
	if (bShooter && Peer->Presentation->IsRagdollActive())
	{
		if (RagdollSeenAt == 0.0)
		{
			RagdollSeenAt = FPlatformTime::Seconds();
		}
		if (!bCapturedRagdoll && FPlatformTime::Seconds() - RagdollSeenAt > 0.8)
		{
			bCapturedRagdoll = true;
			UE_LOG(LogTemp, Display, TEXT("[CombatFeedbackCheck] Observer RAGDOLL Physics=%d VictimOnlyFlash=%.2f"),
				Peer->GetMesh()->IsSimulatingPhysics(TEXT("pelvis")), Feedback->GetHitFlash());
			FScreenshotRequest::RequestScreenshot(Directory / TEXT("RemoteRagdoll.png"), false, false);
		}
	}
}

void UCombatBenchmarkComponent::CaptureHUDSlide(ACombatPlayerController* PC, double Elapsed)
{
	const double CaptureTimes[] = {4.5, 5.0, 5.1, 5.4, 6.2, 6.6};
	if (HUDCaptureStage >= UE_ARRAY_COUNT(CaptureTimes) || Elapsed < CaptureTimes[HUDCaptureStage])
	{
		return;
	}
	const TCHAR* Capture = nullptr;
	switch (HUDCaptureStage++)
	{
		case 0:
			Capture = TEXT("Summary");
			break;
		case 1:
			PC->ToggleExperimentPanel();
			break;
		case 2:
			Capture = TEXT("Sliding");
			break;
		case 3:
			Capture = TEXT("Expanded");
			break;
		case 4:
			PC->ToggleExperimentPanel();
			break;
		case 5:
			Capture = TEXT("Collapsed");
			break;
	}
	if (Capture)
	{
		FScreenshotRequest::RequestScreenshot(
			FPaths::ProjectSavedDir() / FString::Printf(TEXT("CombatPresentation/Slide/%s.png"), Capture), false,
			false);
	}
}

void UCombatBenchmarkComponent::CheckLiveExperiment()
{
	if (bFinished)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - StartedAt > 120.0)
	{
		Finish(false, TEXT("live experiment transition timeout"));
		return;
	}
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwner());
	ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>();
	ACombatCharacter* Pawn = PC ? Cast<ACombatCharacter>(PC->GetPawn()) : nullptr;
	if (!State || !Pawn || !Pawn->GetWeapon() || !PC->TimeSync->IsReady())
	{
		return;
	}
	const uint32 Revision = State->Experiment.Revision;
	if (Revision == 0)
	{
		if (bShooter && State->PlayerArray.Num() >= 2 && !PC->IsExperimentRequestPending())
		{
			PC->RequestExperiment(false, 0);
		}
		return;
	}
	if (Revision != LiveObservedRevision)
	{
		LiveObservedRevision = Revision;
		LiveConditionFinishedAt = 0.0;
		bReady = false;
		int32 ActualLag = -1;
#if DO_ENABLE_NET_TEST
		if (const UNetDriver* Driver = GetWorld()->GetNetDriver())
		{
			ActualLag = Driver->PacketSimulationSettings.PktLag;
		}
#endif
		if (ActualLag != State->Experiment.PacketLagMs ||
			State->LagCompensation.bEnabled != State->Experiment.bRewindEnabled)
		{
			Finish(false, TEXT("replicated condition does not match actual runtime settings"));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[LiveExperimentCheck] %s APPLIED Revision=%u Rewind=%d DriverLagMs=%d"),
			bShooter ? TEXT("Shooter") : TEXT("Observer"), Revision, State->LagCompensation.bEnabled, ActualLag);
	}
	UCombatTelemetryComponent* Telemetry = PC->Telemetry;
	const FCombatTelemetrySummary Summary = Telemetry->GetSummary();
	if (!Telemetry->IsMeasuring() || Summary.Rtt.Count < 10)
	{
		return;
	}
	if (!bReady)
	{
		bReady = true;
		UE_LOG(LogTemp, Display, TEXT("[LiveExperimentCheck] %s MEASURING Revision=%u RTT Avg/Min/Max=%.1f/%.1f/%.1f"),
			bShooter ? TEXT("Shooter") : TEXT("Observer"), Revision, Summary.Rtt.Average(), Summary.Rtt.Minimum,
			Summary.Rtt.Maximum);
	}
	if (Revision == 11)
	{
		if (Summary.TotalSubmitted != 0 || Summary.Ack.Count != 0)
		{
			Finish(false, TEXT("reset retained samples from the previous condition"));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("[LiveExperimentCheck] %s PASS all ten conditions and reset"),
			bShooter ? TEXT("Shooter") : TEXT("Observer"));
		Finish(true, TEXT("runtime conditions, whole-interval statistics and reset verified"));
		return;
	}
	if (!bShooter)
	{
		return;
	}
	if (Summary.Ack.Count < 3)
	{
		PC->SetControlRotation(FRotator(45.f, 90.f, 0.f));
		if (Now - LastCompletedAt > 0.55 && Pawn->GetWeapon()->Fire->CanFire())
		{
			LastCompletedAt = Now;
			Pawn->GetWeapon()->Fire->FireWeapon();
		}
		return;
	}
	if (LiveConditionFinishedAt == 0.0)
	{
		LiveConditionFinishedAt = Now;
		UE_LOG(LogTemp, Display,
			TEXT("[LiveExperimentCheck] Shooter SHOTS Revision=%u N=%llu Ack Avg/Min/Max=%.1f/%.1f/%.1f"), Revision,
			Summary.Ack.Count, Summary.Ack.Average(), Summary.Ack.Minimum, Summary.Ack.Maximum);
		if (FParse::Param(FCommandLine::Get(), TEXT("FPSBenchmarkCapture")))
		{
			FScreenshotRequest::RequestScreenshot(
				FPaths::ProjectSavedDir() / FString::Printf(TEXT("LiveExperiments/Condition-%02u.png"), Revision),
				false, false);
		}
	}
	if (Now - LiveConditionFinishedAt < 1.0 || PC->IsExperimentRequestPending())
	{
		return;
	}
	if (Revision == 10)
	{
		PC->ResetExperimentMetrics();
	}
	else
	{
		PC->RequestExperiment(Revision % 2 == 1, (Revision / 2) * 50);
	}
}

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
	if (bCombatFeedbackCheck && !bSawFireShake)
	{
		const ACombatPlayerController* PC = CastChecked<ACombatPlayerController>(GetOwner());
		bSawFireShake = PC->Feedback->HasActiveFireShake();
		UE_LOG(LogTemp, Display, TEXT("[CombatFeedbackCheck] Shooter FIRE ImmediateShake=%d"), bSawFireShake);
	}
	if (bMovingAimCheck && FPlatformTime::Seconds() - ReadyAt < 6.5)
	{
		return;
	}
	if (bFireModeCheck && !bFireModesVerified)
	{
		return;
	}
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
	if (bFireModeCheck && bShooter)
	{
		bSuccess &= bFireModesVerified;
	}
	if (bMovingAimCheck)
	{
		bSuccess &= bCapturedReloadPose && (bShooter || bCapturedMovingFire);
	}
	if (bCombatFeedbackCheck)
	{
		bSuccess &= bShooter ? (bSawFireShake && bCapturedRagdoll)
							 : (bCapturedDamage && bCapturedHalfHealth && bCapturedLowHealth);
	}
	if (bPresentationCheck)
	{
		bSuccess &= bPresentationReady && bPresentationShot;
		bSuccess &= !bAimIKCheck || (bSawAimUp && bSawAimDown);
	}
	bFinished = true;
	UE_LOG(LogTemp, Display,
		TEXT("[FPSBaselineBenchmark] %s Role=%s Reason=%s Samples=%d Hits=%d RemoteMoved=%d Jump=%d Damage=%d "
			 "Death=%d Respawn=%d Reload=%d DeadFireBlocked=%d Kills=%d Deaths=%d"),
		bSuccess ? TEXT("PASS") : TEXT("FAIL"), bShooter ? TEXT("Shooter") : TEXT("Observer"), Reason, SampleCount,
		HitCount, bRemoteMoved, bJumped, bSawDamage, bSawDeath, bSawRespawn, bReloadVerified, bDeadFireBlocked,
		LastKills, LastDeaths);
	FPlatformMisc::RequestExit(false);
}

void UCombatBenchmarkComponent::CheckPresentation(ACombatCharacter* Character, ACombatCharacter* Peer)
{
	const TCHAR* Role = bShooter ? TEXT("Shooter") : TEXT("Observer");
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CombatPresentation/Network");
	if (!bPresentationReady)
	{
		const ACombatWeapon* LocalWeapon = Character->GetWeapon();
		const ACombatWeapon* PeerWeapon = Peer->GetWeapon();
		const float Up = Peer->GetMesh()->GetUpVector().Z;
		const float HeadAbovePelvis =
			Peer->GetMesh()->GetSocketLocation(TEXT("head")).Z - Peer->GetMesh()->GetSocketLocation(TEXT("pelvis")).Z;
		bPresentationReady = Up > 0.99f && HeadAbovePelvis > 40.f &&
			Peer->GetBaseTranslationOffset().Equals(FVector(0.f, 0.f, -90.f), 0.1f) && LocalWeapon &&
			LocalWeapon->WeaponData && LocalWeapon->FirstPersonMesh->GetSkeletalMeshAsset() && PeerWeapon &&
			PeerWeapon->WeaponData && PeerWeapon->ThirdPersonMesh->GetSkeletalMeshAsset() &&
			PeerWeapon->ThirdPersonMesh->GetAttachParent() == Peer->GetMesh();
		UE_LOG(LogTemp, Display,
			TEXT("[FPSPresentation] %s READY=%d PeerRotation=%s PeerZ=%.2f Up=%.3f HeadAbovePelvis=%.2f Anim=%s "
				 "Weapon=%s"),
			Role, bPresentationReady, *Peer->GetMesh()->GetRelativeRotation().ToString(),
			Peer->GetMesh()->GetRelativeLocation().Z, Up, HeadAbovePelvis,
			*GetNameSafe(Peer->GetMesh()->GetAnimInstance()),
			*GetNameSafe(LocalWeapon ? LocalWeapon->WeaponData : nullptr));
		if (!bPresentationReady)
		{
			if (FPlatformTime::Seconds() - ReadyAt > 12.0)
			{
				Finish(false, TEXT("character upright pose or replicated weapon presentation"));
			}
			return;
		}
		IFileManager::Get().MakeDirectory(*Directory, true);
		FScreenshotRequest::RequestScreenshot(Directory / FString::Printf(TEXT("%sIdle.png"), Role), false, false);
	}
	ACombatCharacter* Shooter = bShooter ? Character : Peer;
	const ACombatWeapon* Weapon = Shooter->GetWeapon();
	const UWeaponDataAsset* Data = Weapon ? Weapon->WeaponData.Get() : nullptr;
	const UAnimInstance* Anim = Shooter->GetMesh()->GetAnimInstance();
	if (bPresentationShot || !Data || !Anim ||
		!Anim->IsPlayingSlotAnimation(Data->CharacterFireAnimation.Get(), Data->CharacterAnimationSlot))
	{
		return;
	}
	const float SlotWeight = Anim->GetSlotMontageGlobalWeight(Data->CharacterAnimationSlot);
	const float Recoil = Data->FirstPersonLocationOffset.X - Weapon->FirstPersonMesh->GetRelativeLocation().X;
	if ((bShooter && Recoil < 0.05f) || (!bShooter && SlotWeight < 0.1f))
	{
		return;
	}
	bPresentationShot = true;
	UE_LOG(LogTemp, Display, TEXT("[FPSPresentation] %s FIRE=1 Animation=%s SlotWeight=%.3f RecoilCm=%.3f"), Role,
		*GetNameSafe(Data->CharacterFireAnimation.Get()), SlotWeight, Recoil);
	FScreenshotRequest::RequestScreenshot(Directory / FString::Printf(TEXT("%sFire.png"), Role), false, false);
}

void UCombatBenchmarkComponent::CheckAimIK(ACombatCharacter* Character, ACombatCharacter* Peer)
{
	const ACombatCharacter* Shooter = bShooter ? Character : Peer;
	const UCombatAnimInstance* Anim = Cast<UCombatAnimInstance>(Shooter->GetMesh()->GetAnimInstance());
	const ACombatWeapon* Weapon = Shooter->GetWeapon();
	if (!Anim || !Weapon || !Weapon->WeaponData || Anim->LeftHandIKAlpha < 0.99f)
	{
		return;
	}
	const bool bUp = Anim->AimPitch > 40.f;
	const bool bDown = Anim->AimPitch < -40.f;
	if ((!bUp || bSawAimUp) && (!bDown || bSawAimDown))
	{
		return;
	}
	Shooter->GetMesh()->GetBoneSpaceTransforms();
	const FTransform Grip = Weapon->WeaponData->LeftHandGripOffset *
		Weapon->ThirdPersonMesh->GetSocketTransform(Weapon->WeaponData->LeftHandGripSocketName);
	const float Error = FVector::Distance(Shooter->GetMesh()->GetSocketLocation(TEXT("hand_l")), Grip.GetLocation());
	const TCHAR* Role = bShooter ? TEXT("Shooter") : TEXT("Observer");
	UE_LOG(LogTemp, Display, TEXT("[FPSAimIK] %s %s Pitch=%.2f IKAlpha=%.3f GripErrorCm=%.4f PawnRole=%d"), Role,
		bUp ? TEXT("UP") : TEXT("DOWN"), Anim->AimPitch, Anim->LeftHandIKAlpha, Error,
		static_cast<int32>(Shooter->GetLocalRole()));
	if (!FMath::IsFinite(Error) || Error > 1.f)
	{
		Finish(false, TEXT("support hand detached during vertical aim"));
		return;
	}
	bSawAimUp |= bUp;
	bSawAimDown |= bDown;
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CombatPresentation/Network");
	FScreenshotRequest::RequestScreenshot(
		Directory / FString::Printf(TEXT("%sAim%s.png"), Role, bUp ? TEXT("Up") : TEXT("Down")), false, false);
}
#endif
