#include "Components/WeaponAmmoComponent.h"
#include "Engine/World.h"
#include "CombatWeapon.h"
#include "CombatCharacter.h"
#include "Data/CombatRules.h"
#include "Net/UnrealNetwork.h"

UWeaponAmmoComponent::UWeaponAmmoComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}

void UWeaponAmmoComponent::Initialize(const FWeaponStats& Stats)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	MagazineAmmo = Stats.MagazineSize;
	ReserveAmmo = Stats.ReserveAmmo;
	GetOwner()->ForceNetUpdate();
}

void UWeaponAmmoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UWeaponAmmoComponent, MagazineAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWeaponAmmoComponent, ReserveAmmo, COND_OwnerOnly);
}

bool UWeaponAmmoComponent::ConsumeRound()
{
	if (!GetOwner()->HasAuthority() || bIsReloading || MagazineAmmo <= 0)
	{
		return false;
	}
	--MagazineAmmo;
	GetOwner()->ForceNetUpdate();
	return true;
}

void UWeaponAmmoComponent::StartReload()
{
	const ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	const ACombatCharacter* Character = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	if (!Character || Character->GetCombatState() == ECombatState::Dead)
	{
		return;
	}
	if (!Weapon->HasAuthority())
	{
		if (Character->IsLocallyControlled() && Character->GetCombatState() != ECombatState::Reloading)
		{
			ServerStartReload();
		}
		return;
	}
	if (bIsReloading || !FCombatRules::IsMatchActive(GetWorld()) || MagazineAmmo >= Weapon->GetStats().MagazineSize ||
		ReserveAmmo <= 0)
	{
		return;
	}
	bIsReloading = true;
	ReloadEndTime = GetWorld()->GetTimeSeconds() + Weapon->GetStats().ReloadTime;
	OnReloadChanged.Broadcast();
}

void UWeaponAmmoComponent::ServerStartReload_Implementation()
{
	StartReload();
}

void UWeaponAmmoComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (GetOwner()->HasAuthority() && bIsReloading && GetWorld()->GetTimeSeconds() >= ReloadEndTime)
	{
		FinishReload();
	}
}

void UWeaponAmmoComponent::FinishReload()
{
	const ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	const ACombatCharacter* Character = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	if (!Weapon || !Weapon->HasAuthority() || !bIsReloading || !Character ||
		Character->GetCombatState() == ECombatState::Dead || GetWorld()->GetTimeSeconds() < ReloadEndTime)
	{
		return;
	}
	const int32 Transfer = FMath::Min(Weapon->GetStats().MagazineSize - MagazineAmmo, ReserveAmmo);
	MagazineAmmo += Transfer;
	ReserveAmmo -= Transfer;
	bIsReloading = false;
	OnReloadChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
}

void UWeaponAmmoComponent::CancelReload()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	bIsReloading = false;
}

void UWeaponAmmoComponent::RestoreExperimentAmmo(int32 Magazine, int32 Reserve)
{
	const ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	if (!Weapon || !Weapon->HasAuthority())
	{
		return;
	}
	CancelReload();
	MagazineAmmo = FMath::Clamp(Magazine, 0, Weapon->GetStats().MagazineSize);
	ReserveAmmo = FMath::Clamp(Reserve, 0, Weapon->GetStats().ReserveAmmo);
	OnReloadChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
}
