#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatAnimationAssetLibrary.generated.h"

class UAnimBlueprint;
class UAnimSequence;
class USkeletalMesh;

// Graph-pin editing is not exposed by UE Python. The implementation is editor-only.
UCLASS()
class LATENCY_MULTICOMBAT_API UCombatAnimationAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Combat|Editor")
	static bool ConfigureRifleUpperBody(UAnimBlueprint* Blueprint, UAnimSequence* AimPose);
	UFUNCTION(BlueprintCallable, Category = "Combat|Editor")
	static bool KeepFirstPersonArmGeometry(USkeletalMesh* Mesh);
};
