#include "CombatWeapon.h"
#include "Components/WeaponHitValidationComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Components/WeaponAmmoComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "CombatCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/CombatRules.h"
#include "Net/UnrealNetwork.h"

ACombatWeapon::ACombatWeapon()
{
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	SetReplicateMovement(true);

	WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	SetRootComponent(WeaponRoot);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(WeaponRoot);
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetCastShadow(false);
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ThirdPersonMesh"));
	ThirdPersonMesh->SetupAttachment(WeaponRoot);
	ThirdPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonMesh->SetOwnerNoSee(true);
	ThirdPersonMesh->SetCastShadow(true);
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(WeaponRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetOnlyOwnerSee(true);
	WeaponMesh->SetCastShadow(false);
	WeaponMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);

	ThirdPersonStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonStaticMesh"));
	ThirdPersonStaticMesh->SetupAttachment(WeaponRoot);
	ThirdPersonStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonStaticMesh->SetOwnerNoSee(true);
	ThirdPersonStaticMesh->SetCastShadow(true);
	ThirdPersonStaticMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

	Fire = CreateDefaultSubobject<UWeaponFireComponent>(TEXT("Fire"));
	Ammo = CreateDefaultSubobject<UWeaponAmmoComponent>(TEXT("Ammo"));
	Presentation = CreateDefaultSubobject<UWeaponPresentationComponent>(TEXT("Presentation"));
	HitValidation = CreateDefaultSubobject<UWeaponHitValidationComponent>(TEXT("HitValidation"));
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

void ACombatWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	if (Presentation)
	{
		Presentation->AttachToCharacter();
	}
}

void ACombatWeapon::OnRep_WeaponData()
{
	Stats = FCombatRules::ReadWeaponStats(WeaponData);
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
