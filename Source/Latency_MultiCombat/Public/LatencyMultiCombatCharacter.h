// Latency_MultiCombat - FPS Character (4v4 Deathmatch)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "Data/CombatTypes.h"
#include "LatencyMultiCombatCharacter.generated.h"

class UCharacterDataAsset;
class UWeaponDataAsset;
class UInputAction;
class UEnhancedInputComponent;

UCLASS()
class LATENCY_MULTICOMBAT_API ALatencyMultiCombatCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ALatencyMultiCombatCharacter();

	// --- Data Assets (assign in BP) ---
	UPROPERTY(EditDefaultsOnly, Category = "Stats")
	TObjectPtr<UCharacterDataAsset> CharacterData;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UWeaponDataAsset> CurrentWeapon;

	// --- Combat State ---
	UPROPERTY(Replicated, BlueprintReadOnly)
	ECombatState CombatState;

	// --- Input Actions (assign in BP - editor-created assets only) ---
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Jump;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Shoot;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Reload;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_ADS; // Aim Down Sights (hold)

	// --- Ammo ---
	int32 MagazineAmmo;
	int32 ReserveAmmo;

	// --- Shooting ---
	void FireWeapon();
	void StartReload();
	void StopReload();
	bool CanFire() const;

	// --- Damage / Death ---
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	void Die(AController* Killer);

	// --- State Management ---
	void SetCombatState(ECombatState NewState);

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaTime) override;

	// --- Input Handlers (Enhanced Input callbacks) ---
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Jump();
	void ShootPressed(const FInputActionValue& Value);
	void ShootReleased(const FInputActionValue& Value);
	void ReloadStarted(const FInputActionValue& Value);
	void ADSStarted(const FInputActionValue& Value);
	void ADSEnded(const FInputActionValue& Value);

private:
	// Apply stats from data asset
	void ApplyCharacterStats();
	void ApplyWeaponStats();

	// Hitscan trace
	FVector GetMuzzleLocation() const;
	bool TraceHit(FVector Start, FVector End, AActor*& OutHitActor) const;

	float LastFireTime;
	bool bIsReloading;
	float ReloadEndTime;
	bool bWantsToFire; // for full-auto
	bool bIsADS;
};
