#include "Components/WeaponFireComponent.h"
#include "Components/WeaponHitValidationComponent.h"
#include "Components/CombatTimeSyncComponent.h"
#include "CombatPlayerController.h"
#include "Data/CombatNetworkDataAsset.h"
#include "Components/HealthComponent.h"
#include "Components/WeaponAmmoComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "CombatCharacter.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Data/CombatRules.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

UWeaponFireComponent::UWeaponFireComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}

ACombatWeapon* UWeaponFireComponent::GetWeapon() const
{
	return Cast<ACombatWeapon>(GetOwner());
}

void UWeaponFireComponent::BeginPlay()
{
	Super::BeginPlay();
	if (ACombatWeapon* Weapon = GetWeapon())
	{
		Weapon->Ammo->OnReloadChanged.AddUObject(this, &UWeaponFireComponent::ReloadChanged);
	}
}

void UWeaponFireComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ACombatWeapon* Weapon = GetWeapon())
	{
		Weapon->Ammo->OnReloadChanged.RemoveAll(this);
	}
	Super::EndPlay(Reason);
}

void UWeaponFireComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWeaponFireComponent, CombatState);
	DOREPLIFETIME(UWeaponFireComponent, bIsADS);
}

void UWeaponFireComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	const ACombatWeapon* Weapon = GetWeapon();
	const ACombatCharacter* Character = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	if (!Character)
	{
		return;
	}
	if (Character->IsLocallyControlled() && bWantsToFire && Weapon->GetStats().bFullAuto)
	{
		FireWeapon();
	}
	if (Weapon->HasAuthority())
	{
		RefreshCombatState();
	}
}

void UWeaponFireComponent::SetTriggerHeld(bool bHeld)
{
	if (CombatState == ECombatState::Dead)
	{
		return;
	}
	bWantsToFire = bHeld;
	if (bHeld)
	{
		FireWeapon();
	}
}

bool UWeaponFireComponent::CanFire() const
{
	const ACombatWeapon* Weapon = GetWeapon();
	const ACombatCharacter* Character = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	const ACombatPlayerState* PS = Character ? Character->GetPlayerState<ACombatPlayerState>() : nullptr;
	return Character && Character->GetController() && PS && !PS->Health->IsDead() && !Weapon->Ammo->IsReloading() &&
		Weapon->Ammo->GetMagazineAmmo() > 0 && CombatState != ECombatState::Dead &&
		CombatState != ECombatState::Reloading && FCombatRules::IsMatchActive(GetWorld());
}

void UWeaponFireComponent::FireWeapon()
{
	if (!CanFire())
	{
		return;
	}
	const ACombatWeapon* Weapon = GetWeapon();
	const ACombatCharacter* Character = Weapon->GetOwningCharacter();
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(Character->GetController());
	if (!Weapon->HasAuthority() &&
		(!Character->IsLocallyControlled() || !PC || !PC->TimeSync->IsReady() || !Character->GetLifeId().IsValid()))
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Weapon->HasAuthority() && Now - LastLocalFireTime < 1.f / Weapon->GetStats().FireRate)
	{
		return;
	}
	LastLocalFireTime = Now;
	FCombatShotCommand Command;
	Command.ShotId = ++NextShotId;
	Command.LifeId = Character->GetLifeId();
	Command.ServerFireTime = PC ? PC->TimeSync->GetServerTime() : Now;
	Command.AimRotation = Character->GetBaseAimRotation();
	OnShotSubmitted.Broadcast(Command.ShotId);
	if (Weapon->HasAuthority())
	{
		ClientShotResult(ProcessShotCommand(Command));
	}
	else
	{
		ServerFire(Command);
	}
}

void UWeaponFireComponent::ServerFire_Implementation(FCombatShotCommand Command)
{
	ClientShotResult(ProcessShotCommand(Command));
}

EShotRejection UWeaponFireComponent::ValidateCommand(const FCombatShotCommand& Command)
{
	const ACombatWeapon* Weapon = GetWeapon();
	const ACombatCharacter* Character = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	if (!Character || !Weapon->HasAuthority() || !Character->GetController())
	{
		return EShotRejection::InvalidOwner;
	}
	if (!Command.LifeId.IsValid() || Command.LifeId != Character->GetLifeId())
	{
		return EShotRejection::InvalidLife;
	}
	if (Command.ShotId == 0 || Command.ShotId <= LastProcessedShotId)
	{
		return EShotRejection::InvalidSequence;
	}
	// 거절한 요청도 처리한 번호로 남겨서 같은 요청을 다시 못 쓰게 함.
	LastProcessedShotId = Command.ShotId;
	if (!FMath::IsFinite(Command.AimRotation.Pitch) || !FMath::IsFinite(Command.AimRotation.Yaw) ||
		!FMath::IsFinite(Command.AimRotation.Roll))
	{
		return EShotRejection::InvalidAim;
	}
	if (!FMath::IsFinite(Command.ServerFireTime))
	{
		return EShotRejection::InvalidTime;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const FLagCompensationSettings Settings = FLagCompensationSettings::Get(GetWorld());
	if (Command.ServerFireTime > Now + Settings.FutureTolerance)
	{
		return EShotRejection::Future;
	}
	if (Now - Command.ServerFireTime > Settings.MaxRewindSeconds)
	{
		return EShotRejection::TooOld;
	}
	if (Command.ServerFireTime < LastAcceptedShotTime)
	{
		return EShotRejection::OutOfOrderTime;
	}
	if (!CanFire())
	{
		return EShotRejection::CannotFire;
	}
	const double Interval = 1.0 / Weapon->GetStats().FireRate;
	if (Now - LastFireTime + KINDA_SMALL_NUMBER < Interval ||
		Command.ServerFireTime - LastAcceptedShotTime + KINDA_SMALL_NUMBER < Interval)
	{
		return EShotRejection::FireRate;
	}
	return EShotRejection::None;
}

FCombatShotResponse UWeaponFireComponent::ProcessShotCommand(const FCombatShotCommand& Command)
{
	FCombatShotResponse Response;
	Response.ShotId = Command.ShotId;
	Response.Rejection = ValidateCommand(Command);
	if (Response.Rejection != EShotRejection::None)
	{
		return Response;
	}
	ACombatWeapon* Weapon = GetWeapon();
	FHitResult Hit;
	FVector Start, End, Direction;
	Response.Rejection = Weapon->HitValidation->Resolve(Command, Hit, Start, End, Direction, Response);
	if (Response.Rejection != EShotRejection::None)
	{
		return Response;
	}
	if (!Weapon->Ammo->ConsumeRound())
	{
		Response.Rejection = EShotRejection::CannotFire;
		return Response;
	}
	LastFireTime = GetWorld()->GetTimeSeconds();
	LastAcceptedShotTime = Command.ServerFireTime;
	SetCombatState(ECombatState::Firing);
	if (Hit.bBlockingHit && Hit.GetActor())
	{
		const float Applied = Weapon->HitValidation->ApplyValidatedDamage(Hit, Direction);
		Response.bDamagedPlayer = Applied > 0.f && Cast<ACombatCharacter>(Hit.GetActor());
	}
	MulticastShot(Start, End, Hit.bBlockingHit, Command.ShotId, Response.bDamagedPlayer);
	GetOwner()->ForceNetUpdate();
	if (Weapon->Ammo->GetMagazineAmmo() == 0)
	{
		Weapon->Ammo->StartReload();
	}
	return Response;
}

void UWeaponFireComponent::SetADS(bool bNewADS)
{
	if (CombatState == ECombatState::Dead)
	{
		return;
	}
	bIsADS = bNewADS;
	const ACombatCharacter* Character = GetWeapon()->GetOwningCharacter();
	if (!GetOwner()->HasAuthority() && Character && Character->IsLocallyControlled())
	{
		ServerSetADS(bNewADS);
	}
}

void UWeaponFireComponent::ServerSetADS_Implementation(bool bNewADS)
{
	SetADS(bNewADS);
}

void UWeaponFireComponent::ReloadChanged()
{
	if (CombatState == ECombatState::Dead)
	{
		return;
	}
	SetCombatState(GetWeapon()->Ammo->IsReloading() ? ECombatState::Reloading
													: (bIsADS ? ECombatState::Aiming : ECombatState::Idle));
}

void UWeaponFireComponent::RefreshCombatState()
{
	const ACombatWeapon* Weapon = GetWeapon();
	const ACombatCharacter* Character = Weapon->GetOwningCharacter();
	if (!Character || CombatState == ECombatState::Dead || Weapon->Ammo->IsReloading())
	{
		return;
	}
	if (GetWorld()->GetTimeSeconds() - LastFireTime < 1.f / Weapon->GetStats().FireRate)
	{
		return;
	}
	SetCombatState(bIsADS
			? ECombatState::Aiming
			: (Character->GetVelocity().SizeSquared2D() > FCombatRules::MovingSpeedSquared ? ECombatState::Moving
																						   : ECombatState::Idle));
}

void UWeaponFireComponent::MarkDead()
{
	if (!GetOwner()->HasAuthority() || CombatState == ECombatState::Dead)
	{
		return;
	}
	GetWeapon()->Ammo->CancelReload();
	SetCombatState(ECombatState::Dead);
}

void UWeaponFireComponent::SetCombatState(ECombatState NewState)
{
	if (!GetOwner()->HasAuthority() || CombatState == NewState)
	{
		return;
	}
	CombatState = NewState;
	OnRep_CombatState();
	GetOwner()->ForceNetUpdate();
}

void UWeaponFireComponent::OnRep_CombatState()
{
	if (CombatState == ECombatState::Dead)
	{
		bWantsToFire = false;
		bIsADS = false;
	}
	OnStateChanged.Broadcast(CombatState);
}

void UWeaponFireComponent::MulticastShot_Implementation(
	FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit, uint32 ShotId, bool bDamagedPlayer)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	GetWeapon()->Presentation->PlayShot(Start, End, bHit);
}

void UWeaponFireComponent::ClientShotResult_Implementation(FCombatShotResponse Response)
{
	OnShotResult.Broadcast(Response);
	if (Response.Rejection == EShotRejection::None)
	{
		OnShotFeedback.Broadcast(Response.ShotId, Response.bDamagedPlayer);
		if (Response.bDamagedPlayer)
		{
			GetWeapon()->Presentation->ConfirmHit();
			OnHitConfirmed.Broadcast(Response.ShotId);
		}
	}
}
