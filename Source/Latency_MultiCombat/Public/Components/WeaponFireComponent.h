#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "Data/CombatShotCommand.h"
#include "WeaponFireComponent.generated.h"

class ACombatWeapon;
DECLARE_MULTICAST_DELEGATE_OneParam(FBaselineShotSubmitted, uint32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FBaselineShotFeedback, uint32, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FBaselineHitConfirmed, uint32);
DECLARE_MULTICAST_DELEGATE_OneParam(FWeaponStateChanged, ECombatState);
DECLARE_MULTICAST_DELEGATE_OneParam(FCombatShotResult, const FCombatShotResponse&);

USTRUCT()
struct FCombatFireModeState
{
	GENERATED_BODY()
	UPROPERTY()
	bool bAutomatic = true;
	UPROPERTY()
	uint32 Revision = 0;
};

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class LATENCY_MULTICOMBAT_API UWeaponFireComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWeaponFireComponent();
	void FireWeapon();
	void SetTriggerHeld(bool bHeld);
	void ToggleFireMode();
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsAutomaticFire() const;
	void SetADS(bool bNewADS);
	void MarkDead();
	// RPC랑 테스트 모두 여기서 시각, 생명 ID, 발사 상태를 확인함.
	FCombatShotResponse ProcessShotCommand(const FCombatShotCommand& Command);
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
	FCombatShotResult OnShotResult;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION(Server, Reliable)
	void ServerFire(FCombatShotCommand Command);
	UFUNCTION(Server, Reliable)
	void ServerSetADS(bool bNewADS);
	UFUNCTION(Server, Reliable)
	void ServerSetAutomaticFire(bool bNewAutomatic, uint32 Revision);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShot(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd, bool bBlockingHit, uint32 ShotId,
		bool bDamagedPlayer);
	UFUNCTION(Client, Reliable)
	void ClientShotResult(FCombatShotResponse Response);
	UFUNCTION()
	void OnRep_CombatState();
	EShotRejection ValidateCommand(const FCombatShotCommand& Command);
	void SetCombatState(ECombatState NewState);
	void RefreshCombatState();
	void ReloadChanged();
	void SetAutomaticFire(bool bNewAutomatic, uint32 Revision);
	ACombatWeapon* GetWeapon() const;
	UPROPERTY(ReplicatedUsing = OnRep_CombatState, BlueprintReadOnly, Category = "Combat",
		meta = (AllowPrivateAccess = "true"))
	ECombatState CombatState = ECombatState::Idle;
	UPROPERTY(Replicated)
	bool bIsADS = false;
	UPROPERTY(Replicated)
	FCombatFireModeState FireMode;
	bool bPredictedAutomaticFire = true;
	uint32 PredictedFireModeRevision = 0;
	uint32 NextShotId = 0;
	uint32 LastProcessedShotId = 0;
	double LastAcceptedShotTime = -1000.0;
	float LastFireTime = -1000.f;
	float LastLocalFireTime = -1000.f;
	bool bWantsToFire = false;
};
