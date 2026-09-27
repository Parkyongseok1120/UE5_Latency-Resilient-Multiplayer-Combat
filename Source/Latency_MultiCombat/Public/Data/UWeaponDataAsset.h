// Latency_MultiCombat - Weapon Data Asset
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/CombatTypes.h"
#include "UWeaponDataAsset.generated.h"

UCLASS()
class LATENCY_MULTICOMBAT_API UWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FText WeaponName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FWeaponStats Stats;

	// Optional: visual/audio references (fill in editor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Visual")
	TSoftObjectPtr<UStaticMesh> WeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TSoftObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	TSoftObjectPtr<USoundBase> ReloadSound;
};
