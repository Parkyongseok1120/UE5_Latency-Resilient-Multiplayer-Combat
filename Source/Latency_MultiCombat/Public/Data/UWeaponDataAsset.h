// Weapon Data Asset
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/CombatTypes.h"
#include "UWeaponDataAsset.generated.h"

class USkeletalMesh;
class UStaticMesh;
class USoundBase;
class UAnimSequence;

UCLASS()
class LATENCY_MULTICOMBAT_API UWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FText WeaponName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FWeaponStats Stats;

	// Primary weapon skeletal mesh (used for both 1P view and 3P character attachment when set)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	TSoftObjectPtr<USkeletalMesh> WeaponSkeletalMesh;

	// Optional static mesh fallback when WeaponSkeletalMesh is unset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	TSoftObjectPtr<UStaticMesh> WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	FName ThirdPersonAttachSocketName = TEXT("HandGrip_R");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	FVector FirstPersonLocationOffset = FVector(38.f, 16.f, -18.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	FRotator FirstPersonRotationOffset = FRotator(0.f, -90.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	FVector ThirdPersonLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	FRotator ThirdPersonRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TSoftObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TSoftObjectPtr<USoundBase> ReloadSound;

	// Played through the character AnimBP slot for each accepted shot, including remote viewers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
	TSoftObjectPtr<UAnimSequence> CharacterFireAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
	FName CharacterAnimationSlot = TEXT("DefaultSlot");

	// Desired hand_l transform on the weapon. NAME_None disables support-hand IK.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
	FName LeftHandGripSocketName = TEXT("LeftHandGrip");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Animation")
	FTransform LeftHandGripOffset = FTransform::Identity;
};
