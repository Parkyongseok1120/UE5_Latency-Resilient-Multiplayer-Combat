#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatShotSample.h"
#include "CombatBenchmarkComponent.generated.h"

class ACombatCharacter;
class ACombatPlayerState;
class UWeaponFireComponent;

// Development-only opt-in behavior. No replicated test state or test damage RPCs.
UCLASS()
class LATENCY_MULTICOMBAT_API UCombatBenchmarkComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatBenchmarkComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
#if !UE_BUILD_SHIPPING
	void BindCharacter(ACombatCharacter* Character);
	void Submitted(uint32 ShotId);
	void Feedback(uint32 ShotId, bool bDamagedPlayer);
	void Confirmed(uint32 ShotId);
	void Finish(bool bSuccess, const TCHAR* Reason);
	TWeakObjectPtr<ACombatCharacter> BoundCharacter;
	TWeakObjectPtr<UWeaponFireComponent> BoundFire;
	TWeakObjectPtr<ACombatCharacter> InitialBluePawn;
	TWeakObjectPtr<ACombatPlayerState> TargetState;
	FCombatShotSample Sample;
	bool bShooter = false;
	bool bFinished = false;
	bool bPending = false;
	bool bReady = false;
	bool bRemoteMoved = false;
	bool bJumped = false;
	bool bSawDamage = false;
	bool bSawDeath = false;
	bool bSawRespawn = false;
	bool bDeadFireBlocked = false;
	bool bReloadRequested = false;
	bool bReloadStarted = false;
	bool bReloadVerified = false;
	bool bCaptured = false;
	bool bMovementLogged = false;
	int32 ReloadTotalAmmo = 0;
	int32 ReloadMagazineBefore = 0;
	int32 SampleCount = 0;
	int32 DesiredSamples = 24;
	int32 HitCount = 0;
	int32 LastKills = 0;
	int32 LastDeaths = 0;
	float LastHealth = -1.f;
	double StartedAt = 0.0;
	double ReadyAt = 0.0;
	double LastCompletedAt = 0.0;
	double ReloadRequestedAt = 0.0;
	FVector PeerStart = FVector::ZeroVector;
	FVector LocalStart = FVector::ZeroVector;
	FVector MoveDirection = FVector::ZeroVector;
#endif
};
