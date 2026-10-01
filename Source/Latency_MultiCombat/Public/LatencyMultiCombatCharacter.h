// Server-authoritative FPS baseline.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Data/CombatTypes.h"
#include "LatencyMultiCombatCharacter.generated.h"

class UCameraComponent;
class UCharacterDataAsset;
class UWeaponDataAsset;
class UInputAction;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class LATENCY_MULTICOMBAT_API ALatencyMultiCombatCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ALatencyMultiCombatCharacter();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UCharacterDataAsset> CharacterData;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponDataAsset> CurrentWeapon;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> BodyProxy;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> FirstPersonWeapon;
	UPROPERTY(ReplicatedUsing = OnRep_CombatState, BlueprintReadOnly, Category = "Combat")
	ECombatState CombatState = ECombatState::Idle;

	// A complete set uses Enhanced Input; otherwise built-in keyboard/mouse bindings apply.
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
	TObjectPtr<UInputAction> IA_ADS;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
	int32 MagazineAmmo = 30;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
	int32 ReserveAmmo = 90;

	void FireWeapon();
	void StartReload();
	void StopReload();
	bool CanFire() const;
	void Die(AController* Killer);
	void SetCombatState(ECombatState NewState);
	float GetLastHitConfirmationTime() const { return LastHitConfirmationTime; }
	virtual FVector GetPawnViewLocation() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaTime) override;
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void ShootPressed(const FInputActionValue& Value);
	void ShootReleased(const FInputActionValue& Value);
	void ReloadStarted(const FInputActionValue& Value);
	void ADSStarted(const FInputActionValue& Value);
	void ADSEnded(const FInputActionValue& Value);

	// Only aiming intent crosses this boundary. Origin, target, ammo and damage stay on the server.
	UFUNCTION(Server, Reliable)
	void ServerFire(FRotator AimRotation);
	UFUNCTION(Server, Reliable)
	void ServerStartReload();
	UFUNCTION(Server, Reliable)
	void ServerSetADS(bool bNewADS);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShot(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd, bool bBlockingHit);
	UFUNCTION(Client, Unreliable)
	void ClientConfirmHit();
	UFUNCTION()
	void OnRep_CombatState();

private:
	void ApplyCharacterStats();
	void ApplyWeaponStats();
	void HandleServerFire(const FRotator& AimRotation);
	void SetADS(bool bNewADS);
	void RefreshCombatState();
	void UpdateBodyColor();
	bool IsMatchActive() const;
	UPROPERTY(Replicated)
	bool bIsADS = false;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
	ETeamSide DisplayedTeam = ETeamSide::Neutral;
	bool bDisplayedDead = false;
	float LastFireTime = -1000.f;
	float LastLocalFireTime = -1000.f;
	float LastHitConfirmationTime = -1000.f;
	bool bIsReloading = false;
	float ReloadEndTime = 0.f;
	bool bWantsToFire = false;
	FWeaponStats WeaponStats;

	void LegacyMoveForward(float Value);
	void LegacyMoveRight(float Value);
	void LegacyLookYaw(float Value);
	void LegacyLookPitch(float Value);
	void LegacyJump();
	void LegacyStopJump();
	void LegacyShoot();
	void LegacyStopShoot();
	void LegacyReload();
	void LegacyADSOn();
	void LegacyADSOff();
};
