#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatExperimentState.h"
#include "Data/CombatShotCommand.h"
#include "Data/CombatTypes.h"
#include "CombatScenarioComponent.generated.h"

class ACombatCharacter;
class ACombatPlayerController;
class ACombatPlayerState;
class UWeaponFireComponent;

UENUM()
enum class ECombatScenarioStage : uint8 { Idle, Preparing, Running, Stopping, Complete, Cancelled };

USTRUCT()
struct FCombatScenarioModeStats
{
	GENERATED_BODY()
	UPROPERTY() int32 Shots = 0;
	UPROPERTY() int32 Hits = 0;
	UPROPERTY() int32 Rejected = 0;
};

USTRUCT()
struct FCombatScenarioRun
{
	GENERATED_BODY()
	UPROPERTY() uint32 RunId = 0;
	UPROPERTY() ECombatScenarioStage Stage = ECombatScenarioStage::Idle;
	UPROPERTY() FCombatScenarioSettings Settings;
	UPROPERTY() TObjectPtr<ACombatPlayerState> Shooter = nullptr;
	UPROPERTY() TObjectPtr<ACombatPlayerState> Target = nullptr;
	UPROPERTY() int32 Pass = 0;
	UPROPERTY() int32 Samples = 0;
	UPROPERTY() double Epoch = 0.0;
	UPROPERTY() bool bRewind = false;
	UPROPERTY() FCombatScenarioModeStats Off;
	UPROPERTY() FCombatScenarioModeStats On;
	UPROPERTY() FString Notice;
};

// 명령줄 벤치마크와 독립된 플레이 중 실험 수명. 전투와 통계 계산은 기존 구성요소에 맡긴다.
UCLASS()
class LATENCY_MULTICOMBAT_API UCombatScenarioComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatScenarioComponent();
	const FCombatScenarioRun& GetState() const { return State; }
	bool IsBusy() const;
	bool IsParticipant(const ACombatPlayerController* PC) const;
	FString Start(ACombatPlayerController* Requester);
	void Stop(const FString& Reason);
	void AcceptSample(ACombatPlayerController* PC, uint32 RunId, int32 Pass, int32 SampleIndex,
		const FCombatShotResponse& Response, float AckMs);
	FString GetProgressLabel() const;
protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	UPROPERTY(Replicated) FCombatScenarioRun State;
	struct FParticipantSnapshot
	{
		TWeakObjectPtr<ACombatCharacter> Pawn;
		FTransform Transform;
		FRotator Aim;
		FCharacterStats Stats;
		float Health = 0.f;
		float Armor = 0.f;
		int32 Magazine = 0;
		int32 Reserve = 0;
		uint8 MovementMode = 0;
		uint8 CustomMovementMode = 0;
	};
	TArray<FParticipantSnapshot> Participants;
	TWeakObjectPtr<UWeaponFireComponent> BoundFire;
	TWeakObjectPtr<ACombatPlayerController> LockedController;
	uint32 LocalRunId = 0;
	int32 LocalPass = INDEX_NONE;
	int32 LocalSamples = 0;
	uint32 PendingShot = 0;
	uint32 LastAcceptedShot = 0;
	double SubmittedAt = 0.0;
	double LastShotAt = -1000.0;
	double StopAfter = 0.0;
	bool bCancelled = false;
	void StartPass();
	void TickServer();
	void TickShooter(ACombatPlayerController* PC);
	void HandleSubmitted(uint32 ShotId);
	void HandleResult(const FCombatShotResponse& Response);
	void UnbindFire();
	void RestoreParticipants();
};
