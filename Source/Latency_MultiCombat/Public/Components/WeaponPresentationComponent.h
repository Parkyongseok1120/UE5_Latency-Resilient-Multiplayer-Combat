#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponPresentationComponent.generated.h"

class UStaticMesh;
class UMaterialInterface;
class USoundBase;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class LATENCY_MULTICOMBAT_API UWeaponPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWeaponPresentationComponent();
	void ApplyAssets();
	void PlayShot(const FVector& Start, const FVector& End, bool bHit);
	void ConfirmHit();
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetLastHitConfirmationTime() const { return LastHitConfirmationTime; }

private:
	UPROPERTY()
	TObjectPtr<UStaticMesh> DefaultMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultMaterial;
	UPROPERTY()
	TObjectPtr<USoundBase> FireSound;
	float LastHitConfirmationTime = -1000.f;
};
