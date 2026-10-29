// Character Data Asset
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/CombatTypes.h"
#include "UCharacterDataAsset.generated.h"

class UAnimInstance;
class USkeletalMesh;
class UWeaponDataAsset;

UCLASS()
class LATENCY_MULTICOMBAT_API UCharacterDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FText CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FCharacterStats Stats;

	// Default weapon for this character
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Weapon")
	TSoftObjectPtr<UWeaponDataAsset> DefaultWeapon;

	// Third-person character skeletal mesh and locomotion AnimBP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Visual")
	TSoftObjectPtr<USkeletalMesh> CharacterSkelMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Visual")
	TSoftClassPtr<UAnimInstance> CharacterAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Visual")
	FVector MeshLocationOffset = FVector(0.f, 0.f, -90.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Visual")
	FRotator MeshRotationOffset = FRotator(0.f, -90.f, 0.f);

	// Material vector parameter used for team coloring on skeletal/static body materials
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Visual")
	FName TeamColorParameterName = TEXT("Paint Tint");

	// Socket on the third-person character mesh where equipped weapons attach
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Visual")
	FName WeaponAttachSocketName = TEXT("HandGrip_R");
};
