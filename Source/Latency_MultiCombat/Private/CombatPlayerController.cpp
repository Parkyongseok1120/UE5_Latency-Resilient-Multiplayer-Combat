#include "CombatPlayerController.h"
#include "Components/CombatTelemetryComponent.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/CombatTimeSyncComponent.h"
#include "Components/CombatScenarioComponent.h"
#include "CombatBenchmarkComponent.h"
#include "Components/CombatSmokeComponent.h"
#include "CombatGameState.h"
#include "CombatCharacter.h"
#include "CombatWeapon.h"
#include "Components/WeaponFireComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

ACombatPlayerController::ACombatPlayerController()
{
	TimeSync = CreateDefaultSubobject<UCombatTimeSyncComponent>(TEXT("TimeSync"));
	Telemetry = CreateDefaultSubobject<UCombatTelemetryComponent>(TEXT("CombatTelemetry"));
	Feedback = CreateDefaultSubobject<UCombatFeedbackComponent>(TEXT("CombatFeedback"));
	Benchmark = CreateDefaultSubobject<UCombatBenchmarkComponent>(TEXT("CombatBenchmark"));
	Smoke = CreateDefaultSubobject<UCombatSmokeComponent>(TEXT("CombatSmoke"));
}

void ACombatPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void ACombatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ACombatPlayerController::ToggleExperimentPanel);
	}
	if (InputComponent && ACombatGameState::SupportsLiveExperiments())
	{
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ACombatPlayerController::ToggleExperimentCursor);
		InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ACombatPlayerController::ToggleExperimentRewind);
		InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &ACombatPlayerController::CycleExperimentLag);
		InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ACombatPlayerController::ResetExperimentMetrics);
	}
}

bool ACombatPlayerController::IsExperimentRequestPending() const
{
	const ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>();
	return bExperimentRequestPending && (!State || State->Experiment.Revision < RequestedExperimentRevision);
}

void ACombatPlayerController::RequestExperiment(bool bRewindEnabled, int32 PacketLagMs)
{
	const ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>();
	if (!IsLocalController() || !State || !ACombatGameState::SupportsLiveExperiments() ||
		IsExperimentRequestPending() || State->ScenarioRunner->IsBusy() || !FCombatExperimentState::IsValidLag(PacketLagMs))
	{
		return;
	}
	ExperimentNotice.Reset();
	bExperimentRequestPending = true;
	RequestedExperimentRevision = State->Experiment.Revision + 1;
	ServerSetExperiment(bRewindEnabled, PacketLagMs, State->Experiment.Revision);
}

void ACombatPlayerController::ServerSetExperiment_Implementation(
	bool bRewindEnabled, int32 PacketLagMs, uint32 ExpectedRevision)
{
	ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>();
	ClientExperimentRequestResult(State && !State->ScenarioRunner->IsBusy() &&
		State->SetExperiment(bRewindEnabled, PacketLagMs, ExpectedRevision));
}

void ACombatPlayerController::ClientExperimentRequestResult_Implementation(bool bAccepted)
{
	if (!bAccepted)
	{
		bExperimentRequestPending = false;
		ExperimentNotice = TEXT("조건이 변경됐습니다. 다시 선택해 주세요.");
	}
}

void ACombatPlayerController::ToggleExperimentPanel()
{
	if (!IsLocalController())
	{
		return;
	}
	bExperimentPanelOpen = !bExperimentPanelOpen;
	if (!bExperimentPanelOpen)
	{
		SetExperimentCursor(false);
	}
}

void ACombatPlayerController::ToggleExperimentCursor()
{
	if (!IsLocalController())
	{
		return;
	}
	if (!bShowMouseCursor)
	{
		bExperimentPanelOpen = true;
	}
	SetExperimentCursor(!bShowMouseCursor);
}

void ACombatPlayerController::SetExperimentCursor(bool bVisible)
{
	if (bShowMouseCursor == bVisible)
	{
		return;
	}
	bShowMouseCursor = bVisible;
	bEnableClickEvents = bShowMouseCursor;
	SetIgnoreLookInput(bShowMouseCursor);
	SetIgnoreMoveInput(bShowMouseCursor);
	if (bShowMouseCursor)
	{
		if (const ACombatCharacter* CombatPawn = Cast<ACombatCharacter>(GetPawn()); CombatPawn && CombatPawn->GetWeapon())
		{
			CombatPawn->GetWeapon()->Fire->SetTriggerHeld(false);
			CombatPawn->GetWeapon()->Fire->SetADS(false);
		}
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void ACombatPlayerController::ToggleExperimentRewind()
{
	if (const ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>())
	{
		RequestExperiment(!State->LagCompensation.bEnabled, State->Experiment.PacketLagMs);
	}
}

void ACombatPlayerController::CycleExperimentLag()
{
	if (const ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>())
	{
		const int32 Lag = State->Experiment.PacketLagMs;
		const int32 Next = Lag < 200 ? (Lag / 50 + 1) * 50 : Lag < 500 ? 500 : 0;
		RequestExperiment(State->LagCompensation.bEnabled, Next);
	}
}

void ACombatPlayerController::RequestScenario(const FCombatScenarioSettings& Settings)
{
	const ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>();
	if (!IsLocalController() || !State || !ACombatGameState::SupportsLiveExperiments() ||
		IsExperimentRequestPending() || State->ScenarioRunner->IsBusy() || !Settings.IsValid())
	{
		return;
	}
	ExperimentNotice.Reset();
	bExperimentRequestPending = true;
	RequestedExperimentRevision = State->Experiment.Revision + 1;
	ServerSetScenario(Settings, State->Experiment.Revision);
}

void ACombatPlayerController::ServerSetScenario_Implementation(FCombatScenarioSettings Settings, uint32 ExpectedRevision)
{
	ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>();
	ClientExperimentRequestResult(State && State->SetScenario(Settings, ExpectedRevision));
}

void ACombatPlayerController::RequestScenarioRunning(bool bStart)
{
	const ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>();
	if (IsLocalController() && State && !IsExperimentRequestPending())
	{
		ExperimentNotice.Reset();
		ServerSetScenarioRunning(bStart, State->Experiment.Revision, State->ScenarioRunner->GetState().RunId);
	}
}

void ACombatPlayerController::ServerSetScenarioRunning_Implementation(
	bool bStart, uint32 ExpectedRevision, uint32 ExpectedRunId)
{
	ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>();
	if (!State || !ACombatGameState::SupportsLiveExperiments() || (bStart && ExpectedRevision != State->Experiment.Revision) ||
		ExpectedRunId != State->ScenarioRunner->GetState().RunId)
	{
		ClientScenarioNotice(TEXT("조건이 변경됐습니다. 다시 선택해 주세요."));
		return;
	}
	if (bStart)
	{
		ClientScenarioNotice(State->ScenarioRunner->Start(this));
	}
	else
	{
		State->ScenarioRunner->Stop(TEXT("사용자 중지"));
	}
}

void ACombatPlayerController::ServerScenarioSample_Implementation(
	uint32 RunId, int32 Pass, int32 SampleIndex, FCombatShotResponse Response, float AckMs)
{
	if (ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>())
	{
		State->ScenarioRunner->AcceptSample(this, RunId, Pass, SampleIndex, Response, AckMs);
	}
}

void ACombatPlayerController::ClientScenarioNotice_Implementation(const FString& Notice)
{
	ExperimentNotice = Notice;
}

bool ACombatPlayerController::IsScenarioParticipant() const
{
	const ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>();
	return State && State->ScenarioRunner->IsParticipant(this);
}

void ACombatPlayerController::ResetExperimentMetrics()
{
	if (const ACombatGameState* State = GetWorld()->GetGameState<ACombatGameState>())
	{
		RequestExperiment(State->LagCompensation.bEnabled, State->Experiment.PacketLagMs);
	}
}
