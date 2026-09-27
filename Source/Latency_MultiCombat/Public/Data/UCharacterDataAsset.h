// Latency_MultiCombat - Character Data Asset
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/CombatTypes.h"
#include "UCharacterDataAsset.generated.h"

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

	// Optional: skeletal mesh / animation references
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Visual")
	TSoftObjectPtr<USkeletalMesh> CharacterSkelMesh;
};
