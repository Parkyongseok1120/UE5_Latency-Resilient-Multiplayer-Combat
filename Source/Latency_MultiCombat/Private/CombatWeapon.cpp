#include "CombatWeapon.h"
#include "Components/WeaponFireComponent.h"
#include "Components/WeaponAmmoComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "CombatCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Data/CombatRules.h"
#include "Net/UnrealNetwork.h"

ACombatWeapon::ACombatWeapon()
{
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	SetReplicateMovement(true);
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetOnlyOwnerSee(true);
	WeaponMesh->SetCastShadow(false);
	Fire = CreateDefaultSubobject<UWeaponFireComponent>(TEXT("Fire"));
	Ammo = CreateDefaultSubobject<UWeaponAmmoComponent>(TEXT("Ammo"));
	Presentation = CreateDefaultSubobject<UWeaponPresentationComponent>(TEXT("Presentation"));
}

void ACombatWeapon::BeginPlay()
{
	Super::BeginPlay();
	OnRep_WeaponData();
	if (HasAuthority())
	{
		Ammo->Initialize(GetStats());
	}
}

void ACombatWeapon::OnRep_WeaponData()
{
	Stats = CombatRules::ReadWeaponStats(WeaponData);
	Presentation->ApplyAssets();
}

ACombatCharacter* ACombatWeapon::GetOwningCharacter() const
{
	return Cast<ACombatCharacter>(GetOwner());
}

void ACombatWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatWeapon, WeaponData);
}
