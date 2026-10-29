#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CombatTestWorld.h"
#include "Components/CombatTelemetryComponent.h"
#include "CombatHUD.h"
#include "InputCoreTypes.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMetricStatsTest, "LatencyCombat.Experiment.WholeIntervalStatistics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatMetricStatsTest::RunTest(const FString& Parameters)
{
	FCombatMetricStats Stats;
	TestEqual(TEXT("Empty average is finite"), Stats.Average(), 0.f);
	for (const float Value : {30.f, 10.f, 20.f, -1.f, std::numeric_limits<float>::quiet_NaN()})
	{
		Stats.Add(Value);
	}
	TestEqual(TEXT("Invalid measurements are excluded"), Stats.Count, uint64(3));
	TestEqual(TEXT("Current value"), Stats.Current, 20.f);
	TestEqual(TEXT("Arithmetic mean"), Stats.Average(), 20.f);
	TestEqual(TEXT("Minimum"), Stats.Minimum, 10.f);
	TestEqual(TEXT("Maximum"), Stats.Maximum, 30.f);
	Stats.Add(0.f);
	TestEqual(TEXT("Zero is a valid rewind sample"), Stats.Minimum, 0.f);
	for (int32 Index = 0; Index < 1000; ++Index)
	{
		Stats.Add(15.f);
	}
	TestEqual(TEXT("Statistics outlive the graph history"), Stats.Count, uint64(1004));
	TestEqual(TEXT("Old maximum is retained"), Stats.Maximum, 30.f);
	TestEqual(TEXT("Mean uses the entire interval"), Stats.Average(), 15.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatExperimentControlsTest, "LatencyCombat.Experiment.HUDAndKeyboardControls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatExperimentControlsTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	PC->SetAsLocalPlayerController();
	PC->ClientSetHUD(ACombatHUD::StaticClass());
	ACombatHUD* HUD = Cast<ACombatHUD>(PC->GetHUD());
	if (!TestNotNull(TEXT("Controller owns the experiment HUD"), HUD))
	{
		return false;
	}
	ACombatGameState* State = Fixture.World->GetGameState<ACombatGameState>();
	TestFalse(TEXT("Detailed telemetry starts collapsed"), PC->IsExperimentPanelOpen());
	HUD->NotifyHitBoxClick(TEXT("Lag100"));
	TestEqual(TEXT("Hidden cursor cannot operate HUD controls"), State->Experiment.Revision, uint32(0));
	PC->ToggleExperimentCursor();
	TestTrue(TEXT("Cursor shortcut exposes the pointer"), PC->bShowMouseCursor && PC->bEnableClickEvents);
	TestTrue(TEXT("Cursor shortcut reveals the detailed panel"), PC->IsExperimentPanelOpen());
	HUD->NotifyHitBoxClick(TEXT("Lag100"));
	TestEqual(TEXT("Moving panel buttons cannot be clicked"), State->Experiment.Revision, uint32(0));
	Fixture.Advance(0.3f);
	TestTrue(TEXT("Settled panel permits interaction"), HUD->CanInteractWithExperimentPanel());
	TestTrue(TEXT("HUD navigation blocks aim and movement"), PC->IsLookInputIgnored() && PC->IsMoveInputIgnored());
	const int32 Ammo = CastChecked<ACombatCharacter>(PC->GetPawn())->GetWeapon()->Ammo->GetMagazineAmmo();
	HUD->NotifyHitBoxClick(TEXT("Lag100"));
	TestEqual(TEXT("HUD lag button changes the server setting"), State->Experiment.PacketLagMs, 100);
	HUD->NotifyHitBoxClick(TEXT("RewindOff"));
	TestFalse(TEXT("HUD rewind button changes the actual policy"), State->LagCompensation.bEnabled);
	PC->ToggleExperimentRewind();
	TestTrue(TEXT("F6 toggles rewind"), State->LagCompensation.bEnabled);
	PC->CycleExperimentLag();
	TestEqual(TEXT("F7 selects the next delay"), State->Experiment.PacketLagMs, 150);
	PC->CycleExperimentLag();
	PC->CycleExperimentLag();
	TestEqual(TEXT("F7 wraps the highest delay back to zero"), State->Experiment.PacketLagMs, 0);
	const uint32 Previous = State->Experiment.Revision;
	HUD->NotifyHitBoxClick(TEXT("ResetExperiment"));
	TestEqual(TEXT("Reset button creates a fresh interval"), State->Experiment.Revision, Previous + 1);
	TestEqual(TEXT("HUD operations do not fire the weapon"),
		CastChecked<ACombatCharacter>(PC->GetPawn())->GetWeapon()->Ammo->GetMagazineAmmo(), Ammo);
	PC->ToggleExperimentCursor();
	TestFalse(TEXT("Cursor shortcut returns input to combat"), PC->bShowMouseCursor || PC->IsLookInputIgnored() || PC->IsMoveInputIgnored());
	PC->ToggleExperimentCursor();
	const uint32 RevisionBeforeClose = State->Experiment.Revision;
	PC->ToggleExperimentPanel();
	TestFalse(TEXT("Tab collapses the panel and releases cursor control"),
		PC->IsExperimentPanelOpen() || PC->bShowMouseCursor || PC->IsLookInputIgnored() || PC->IsMoveInputIgnored());
	HUD->NotifyHitBoxClick(TEXT("Lag100"));
	TestEqual(TEXT("Closing hides button interactions immediately"), State->Experiment.Revision, RevisionBeforeClose);
	Fixture.Advance(0.1f);
	PC->ToggleExperimentPanel();
	Fixture.Advance(0.3f);
	TestTrue(TEXT("Tab can reverse a closing animation"), PC->IsExperimentPanelOpen());
	TestFalse(TEXT("Opening for observation does not steal aiming input"), PC->bShowMouseCursor || PC->IsLookInputIgnored());
	TestEqual(TEXT("Panel visibility never changes experiment conditions"), State->Experiment.Revision, RevisionBeforeClose);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatExperimentIntervalsTest, "LatencyCombat.Experiment.ConditionBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatExperimentIntervalsTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatGameState* State = Fixture.World->GetGameState<ACombatGameState>();
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	PC->SetAsLocalPlayerController();
	UCombatTelemetryComponent* Telemetry = PC->Telemetry;
	Telemetry->SetComponentTickEnabled(false);
	Telemetry->RefreshWeaponBinding();
	UWeaponFireComponent* Fire = CastChecked<ACombatCharacter>(PC->GetPawn())->GetWeapon()->Fire;
	for (const int32 Lag : {0, 50, 100, 150, 200})
	{
		const uint32 Previous = State->Experiment.Revision;
		TestTrue(TEXT("Server accepts a valid delay preset"), State->SetExperiment(Lag % 100 == 0, Lag, Previous));
		TestFalse(TEXT("Stale control request cannot overwrite newer settings"),
			State->SetExperiment(false, 0, Previous));
		TestEqual(TEXT("Lag setting is retained"), State->Experiment.PacketLagMs, Lag);
		TestEqual(TEXT("Rewind is applied to actual validation settings"), State->LagCompensation.bEnabled,
			Lag % 100 == 0);
		Telemetry->RefreshExperiment();
		TestFalse(TEXT("Configuration transition excludes warmup"), Telemetry->IsMeasuring());
		Fire->OnShotSubmitted.Broadcast(1);
		TestEqual(TEXT("Warmup shots are excluded"), Telemetry->GetSummary().TotalSubmitted, 0);
		Fixture.Advance(3.2f);
		TestTrue(TEXT("Measurement starts after settling"), Telemetry->IsMeasuring());
		Telemetry->SampleNetworkMetrics(0.010f, true);
		Telemetry->SampleNetworkMetrics(0.030f, true);
		const FCombatTelemetrySummary Summary = Telemetry->GetSummary();
		TestTrue(TEXT("Frame average is raw ms"), FMath::IsNearlyEqual(Summary.Frame.Average(), 20.f));
		TestEqual(TEXT("Frame minimum includes fast frames"), Summary.Frame.Minimum, 10.f);
		TestTrue(TEXT("Frame maximum includes slow frames"), FMath::IsNearlyEqual(Summary.Frame.Maximum, 30.f));
		Fire->OnShotSubmitted.Broadcast(2);
		FCombatShotResponse Response;
		Response.ShotId = 2;
		Response.ExperimentRevision = State->Experiment.Revision;
		Response.ShotAgeSeconds = 0.1f;
		Response.TargetAgeSeconds = 0.05f;
		Fire->OnShotResult.Broadcast(Response);
		Fire->OnShotResult.Broadcast(Response);
		TestEqual(TEXT("Duplicate response does not double count"), Telemetry->GetSummary().Ack.Count, uint64(1));
		TestEqual(TEXT("Shot age is converted to milliseconds"), Telemetry->GetSummary().ShotAge.Average(), 100.f);
		Fire->OnShotSubmitted.Broadcast(3);
		Response.ShotId = 3;
		Response.ExperimentRevision = Previous;
		Fire->OnShotResult.Broadcast(Response);
		TestEqual(TEXT("Foreign experiment result is excluded"), Telemetry->GetSummary().Ack.Count, uint64(1));
		Fire->OnShotSubmitted.Broadcast(4);
		TestTrue(TEXT("Same condition reset creates a fresh interval"),
			State->SetExperiment(State->LagCompensation.bEnabled, Lag, State->Experiment.Revision));
		Telemetry->RefreshExperiment();
		Response.ShotId = 4;
		Fire->OnShotResult.Broadcast(Response);
		TestEqual(TEXT("In-flight previous interval result is excluded"), Telemetry->GetSummary().Ack.Count, uint64(0));
		TestEqual(TEXT("Reset clears graph samples"), Telemetry->GetNetSampleCount(), 0);
	}
	const uint32 Revision = State->Experiment.Revision;
	TestFalse(TEXT("Unknown preset rejected"), State->SetExperiment(true, 75, Revision));
	TestFalse(TEXT("Negative lag rejected"), State->SetExperiment(true, -50, Revision));
	TestFalse(TEXT("Excessive lag rejected"), State->SetExperiment(true, 250, Revision));
	TestEqual(TEXT("Invalid request does not reset measurement"), State->Experiment.Revision, Revision);
	return true;
}
#endif
