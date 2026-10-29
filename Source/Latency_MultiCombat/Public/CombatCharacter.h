#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/CombatTypes.h"
#include "CombatCharacter.generated.h"

class UCameraComponent;
class UCharacterDataAsset;
class UCombatInputComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UCharacterPresentationComponent;
class ACombatWeapon;
class URewindHistoryComponent;

// Engine-facing pawn: locomotion, input forwarding, possession and damage/death orchestration.
UCLASS()
class LATENCY_MULTICOMBAT_API ACombatCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ACombatCharacter();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UCharacterDataAsset> CharacterData;
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<ACombatWeapon> WeaponClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> BodyProxy;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UCharacterPresentationComponent> Presentation;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UCombatInputComponent> CombatInput;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network")
	TObjectPtr<URewindHistoryComponent> RewindHistory;
	const FGuid& GetLifeId() const { return LifeId; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	ACombatWeapon* GetWeapon() const { return Weapon; }
	UFUNCTION(BlueprintPure, Category = "Combat")
	ECombatState GetCombatState() const;
	FCharacterStats GetCharacterStats() const;
	void Die(AController* Killer);
	virtual FVector GetPawnViewLocation() const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
		AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Weapon();
	void HandleCombatStateChanged(ECombatState State);
	UPROPERTY(
		ReplicatedUsing = OnRep_Weapon, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ACombatWeapon> Weapon;
	TWeakObjectPtr<ACombatWeapon> BoundWeapon;
	bool bDeathHandled = false;
	UPROPERTY(Replicated)
	FGuid LifeId;
};
