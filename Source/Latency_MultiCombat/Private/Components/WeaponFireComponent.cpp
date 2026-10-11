#include "Components/WeaponFireComponent.h"
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
		CombatState != ECombatState::Reloading && CombatRules::IsMatchActive(GetWorld());
}

void UWeaponFireComponent::FireWeapon()
{
	if (!CanFire())
	{
		return;
	}
	const ACombatWeapon* Weapon = GetWeapon();
	const ACombatCharacter* Character = Weapon->GetOwningCharacter();
	if (Weapon->HasAuthority())
	{
		HandleServerFire(Character->GetBaseAimRotation(), ++NextShotId);
		return;
	}
	if (!Character->IsLocallyControlled())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastLocalFireTime < 1.f / Weapon->GetStats().FireRate)
	{
		return;
	}
	LastLocalFireTime = Now;
	const uint32 ShotId = ++NextShotId;
	OnShotSubmitted.Broadcast(ShotId);
	ServerFire(Character->GetBaseAimRotation(), ShotId);
}

void UWeaponFireComponent::ServerFire_Implementation(FRotator AimRotation, uint32 ShotId)
{
	HandleServerFire(AimRotation, ShotId);
}

void UWeaponFireComponent::HandleServerFire(const FRotator& AimRotation, uint32 ShotId)
{
	if (!GetOwner()->HasAuthority() || !CanFire() || AimRotation.ContainsNaN())
	{
		return;
	}
	ACombatWeapon* Weapon = GetWeapon();
	ACombatCharacter* Character = Weapon->GetOwningCharacter();
	const FWeaponStats& Stats = Weapon->GetStats();
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime + KINDA_SMALL_NUMBER < 1.f / Stats.FireRate)
	{
		return;
	}
	if (!Weapon->Ammo->ConsumeRound())
	{
		return;
	}
	LastFireTime = Now;
	SetCombatState(ECombatState::Firing);
	const FVector Start = Character->GetPawnViewLocation();
	const FRotator Aim(FMath::Clamp(FRotator::NormalizeAxis(AimRotation.Pitch), -89.f, 89.f),
		FRotator::NormalizeAxis(AimRotation.Yaw), 0.f);
	float Spread = Stats.BaseSpread;
	if (!bIsADS)
	{
		Spread *= Character->GetCharacterStats().HipFireSpreadMultiplier;
	}
	if (Character->GetVelocity().SizeSquared2D() > CombatRules::MovingSpeedSquared)
	{
		Spread *= Stats.MovingSpreadMultiplier;
	}
	const FVector Direction = FMath::VRandCone(Aim.Vector(), FMath::DegreesToRadians(FMath::Clamp(Spread, 0.f, 45.f)));
	const FVector End = Start + Direction * Stats.MaxRange * CombatRules::CentimetersPerMeter;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponTrace), false, Character);
	Params.AddIgnoredActor(Weapon);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	bool bDamagedPlayer = false;
	if (bHit && Hit.GetActor())
	{
		const float Applied =
			UGameplayStatics::ApplyPointDamage(Hit.GetActor(), CombatRules::DamageAtRange(Stats, Hit.Distance),
				Direction, Hit, Character->GetController(), Weapon, nullptr);
		bDamagedPlayer = Applied > 0.f && Cast<ACombatCharacter>(Hit.GetActor());
		if (bDamagedPlayer)
		{
			ClientConfirmHit(ShotId);
		}
	}
	MulticastShot(Start, bHit ? Hit.ImpactPoint : End, bHit, ShotId, bDamagedPlayer);
	GetOwner()->ForceNetUpdate();
	if (Weapon->Ammo->GetMagazineAmmo() == 0)
	{
		Weapon->Ammo->StartReload();
	}
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
			: (Character->GetVelocity().SizeSquared2D() > CombatRules::MovingSpeedSquared ? ECombatState::Moving
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
	const ACombatCharacter* Character = GetWeapon()->GetOwningCharacter();
	if (Character && Character->IsLocallyControlled())
	{
		OnShotFeedback.Broadcast(ShotId, bDamagedPlayer);
	}
	GetWeapon()->Presentation->PlayShot(Start, End, bHit);
}

void UWeaponFireComponent::ClientConfirmHit_Implementation(uint32 ShotId)
{
	GetWeapon()->Presentation->ConfirmHit();
	OnHitConfirmed.Broadcast(ShotId);
}
