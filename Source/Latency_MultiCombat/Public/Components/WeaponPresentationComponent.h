#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "WeaponPresentationComponent.generated.h"

class ACombatCharacter;
class UStaticMesh;
class UMaterialInterface;
class USoundBase;
class UWeaponDataAsset;
class UAnimMontage;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class LATENCY_MULTICOMBAT_API UWeaponPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWeaponPresentationComponent();
	void ApplyAssets();
	void AttachToCharacter();
	void PlayShot(const FVector& Start, const FVector& End, bool bHit);
	void ConfirmHit();
	bool UsesSkeletalWeapon() const { return bUsesSkeletalWeapon; }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetLastHitConfirmationTime() const { return LastHitConfirmationTime; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	void HandleCombatStateChanged(ECombatState NewState);
	void UpdateVisibility(bool bVisible);
	void ApplyFirstPersonRecoil();
	FName ResolveGripSocket(const ACombatCharacter* Character, const UWeaponDataAsset* Data) const;
	FVector ResolveVisualShotOrigin(const FVector& AuthoritativeStart) const;

	UPROPERTY()
	TObjectPtr<UStaticMesh> DefaultMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultMaterial;
	UPROPERTY()
	TObjectPtr<USoundBase> FireSound;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CharacterFireMontage;
	FVector FirstPersonRestLocation = FVector::ZeroVector;
	FRotator FirstPersonRestRotation = FRotator::ZeroRotator;
	float RecoilWeight = 0.f;
	float LastHitConfirmationTime = -1000.f;
	bool bUsesSkeletalWeapon = false;
};
