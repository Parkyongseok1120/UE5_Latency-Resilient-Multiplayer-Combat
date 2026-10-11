#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "WeaponFireComponent.generated.h"

class ACombatWeapon;
DECLARE_MULTICAST_DELEGATE_OneParam(FBaselineShotSubmitted, uint32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FBaselineShotFeedback, uint32, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FBaselineHitConfirmed, uint32);
DECLARE_MULTICAST_DELEGATE_OneParam(FWeaponStateChanged, ECombatState);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class LATENCY_MULTICOMBAT_API UWeaponFireComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWeaponFireComponent();
	void FireWeapon();
	void SetTriggerHeld(bool bHeld);
	void SetADS(bool bNewADS);
	void MarkDead();
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanFire() const;
	UFUNCTION(BlueprintPure, Category = "Combat")
	ECombatState GetCombatState() const { return CombatState; }
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsADS() const { return bIsADS; }
	FBaselineShotSubmitted OnShotSubmitted;
	FBaselineShotFeedback OnShotFeedback;
	FBaselineHitConfirmed OnHitConfirmed;
	FWeaponStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION(Server, Reliable)
	void ServerFire(FRotator AimRotation, uint32 ShotId);
	UFUNCTION(Server, Reliable)
	void ServerSetADS(bool bNewADS);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShot(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd, bool bBlockingHit, uint32 ShotId,
		bool bDamagedPlayer);
	UFUNCTION(Client, Unreliable)
	void ClientConfirmHit(uint32 ShotId);
	UFUNCTION()
	void OnRep_CombatState();
	void HandleServerFire(const FRotator& AimRotation, uint32 ShotId);
	void SetCombatState(ECombatState NewState);
	void RefreshCombatState();
	void ReloadChanged();
	ACombatWeapon* GetWeapon() const;
	UPROPERTY(ReplicatedUsing = OnRep_CombatState, BlueprintReadOnly, Category = "Combat",
		meta = (AllowPrivateAccess = "true"))
	ECombatState CombatState = ECombatState::Idle;
	UPROPERTY(Replicated)
	bool bIsADS = false;
	uint32 NextShotId = 0;
	float LastFireTime = -1000.f;
	float LastLocalFireTime = -1000.f;
	bool bWantsToFire = false;
};
