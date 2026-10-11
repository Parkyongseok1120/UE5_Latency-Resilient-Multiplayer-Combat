#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/CombatTypes.h"
#include "CombatWeapon.generated.h"

class ACombatCharacter;
class UWeaponDataAsset;
class UStaticMeshComponent;
class UWeaponFireComponent;
class UWeaponAmmoComponent;
class UWeaponPresentationComponent;

// The server spawns one weapon per pawn; its owner chain routes component RPCs to the owning client.
UCLASS()
class LATENCY_MULTICOMBAT_API ACombatWeapon : public AActor
{
	GENERATED_BODY()
public:
	ACombatWeapon();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponFireComponent> Fire;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponAmmoComponent> Ammo;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponPresentationComponent> Presentation;
	// Spawn configuration, replicated so remote clients load the same visual/audio assets.
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing = OnRep_WeaponData, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponDataAsset> WeaponData;
	const FWeaponStats& GetStats() const { return Stats; }
	ACombatCharacter* GetOwningCharacter() const;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_WeaponData();
	FWeaponStats Stats;
};
