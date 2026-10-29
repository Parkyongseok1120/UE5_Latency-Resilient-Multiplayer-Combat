// Player Controller (Enhanced Input)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/CombatExperimentState.h"
#include "Data/CombatShotCommand.h"
#include "CombatPlayerController.generated.h"

class UCombatBenchmarkComponent;
class UCombatSmokeComponent;
class UCombatTelemetryComponent;
class UCombatTimeSyncComponent;
class UCombatFeedbackComponent;

UCLASS()
class LATENCY_MULTICOMBAT_API ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACombatPlayerController();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Network")
	TObjectPtr<UCombatTimeSyncComponent> TimeSync;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
	TObjectPtr<UCombatTelemetryComponent> Telemetry;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UCombatFeedbackComponent> Feedback;
	UPROPERTY(VisibleAnywhere, Category = "Testing")
	TObjectPtr<UCombatBenchmarkComponent> Benchmark;
	UPROPERTY(VisibleAnywhere, Category = "Testing")
	TObjectPtr<UCombatSmokeComponent> Smoke;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	void RequestExperiment(bool bRewindEnabled, int32 PacketLagMs);
	void RequestScenario(const FCombatScenarioSettings& Settings);
	void RequestScenarioRunning(bool bStart);
	bool IsScenarioParticipant() const;
	UFUNCTION(Server, Reliable)
	void ServerScenarioSample(uint32 RunId, int32 Pass, int32 SampleIndex, FCombatShotResponse Response, float AckMs);
	bool IsExperimentRequestPending() const;
	void ToggleExperimentPanel();
	bool IsExperimentPanelOpen() const { return bExperimentPanelOpen; }
	void ToggleExperimentCursor();
	void ToggleExperimentRewind();
	void CycleExperimentLag();
	void ResetExperimentMetrics();
	const FString& GetExperimentNotice() const { return ExperimentNotice; }

private:
	void SetExperimentCursor(bool bVisible);
	bool bExperimentPanelOpen = false;
	UFUNCTION(Server, Reliable)
	void ServerSetExperiment(bool bRewindEnabled, int32 PacketLagMs, uint32 ExpectedRevision);
	UFUNCTION(Server, Reliable)
	void ServerSetScenario(FCombatScenarioSettings Settings, uint32 ExpectedRevision);
	UFUNCTION(Server, Reliable)
	void ServerSetScenarioRunning(bool bStart, uint32 ExpectedRevision, uint32 ExpectedRunId);
	UFUNCTION(Client, Reliable)
	void ClientScenarioNotice(const FString& Notice);
	UFUNCTION(Client, Reliable)
	void ClientExperimentRequestResult(bool bAccepted);
	bool bExperimentRequestPending = false;
	uint32 RequestedExperimentRevision = 0;
	FString ExperimentNotice;
};
