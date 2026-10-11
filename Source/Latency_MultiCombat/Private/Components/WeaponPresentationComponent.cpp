#include "Components/WeaponPresentationComponent.h"
#include "Engine/World.h"
#include "CombatWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Data/UWeaponDataAsset.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

UWeaponPresentationComponent::UWeaponPresentationComponent()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	DefaultMesh = Mesh.Object;
	DefaultMaterial = Material.Object;
}

void UWeaponPresentationComponent::ApplyAssets()
{
	ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	if (!Weapon)
	{
		return;
	}
	const UWeaponDataAsset* Data = Weapon->WeaponData.Get();
	const bool bCustomMesh = Data && !Data->WeaponMesh.IsNull();
	Weapon->WeaponMesh->SetStaticMesh(bCustomMesh ? Data->WeaponMesh.LoadSynchronous() : DefaultMesh.Get());
	if (!bCustomMesh)
	{
		Weapon->WeaponMesh->SetMaterial(0, DefaultMaterial);
	}
	Weapon->WeaponMesh->SetRelativeScale3D(bCustomMesh ? FVector::OneVector : FVector(0.45f, 0.08f, 0.08f));
	FireSound = Data && !Data->FireSound.IsNull() && GetNetMode() != NM_DedicatedServer
		? Data->FireSound.LoadSynchronous()
		: nullptr;
}

void UWeaponPresentationComponent::PlayShot(const FVector& Start, const FVector& End, bool bHit)
{
	DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 0.08f, 0, 1.f);
	if (bHit)
	{
		DrawDebugPoint(GetWorld(), End, 8.f, FColor::Orange, false, 0.15f);
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Start);
	}
}

void UWeaponPresentationComponent::ConfirmHit()
{
	LastHitConfirmationTime = GetWorld()->GetTimeSeconds();
}
