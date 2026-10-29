// Player Controller (Enhanced Input)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
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
	UFUNCTION(Client, Reliable)
	void ClientExperimentRequestResult(bool bAccepted);
	bool bExperimentRequestPending = false;
	uint32 RequestedExperimentRevision = 0;
	FString ExperimentNotice;
};
