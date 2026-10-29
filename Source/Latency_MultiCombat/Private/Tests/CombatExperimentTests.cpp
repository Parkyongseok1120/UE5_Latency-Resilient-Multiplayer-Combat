#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CombatTestWorld.h"
#include "Components/CombatTelemetryComponent.h"
#include "Components/CombatScenarioComponent.h"
#include "CombatHUD.h"
#include "InputCoreTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	HUD->NotifyHitBoxClick(TEXT("ScenarioTab"));
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
	TestEqual(TEXT("F7 includes the 500ms preset"), State->Experiment.PacketLagMs, 500);
	PC->CycleExperimentLag();
	TestEqual(TEXT("F7 wraps the highest delay back to zero"), State->Experiment.PacketLagMs, 0);
	const uint32 Previous = State->Experiment.Revision;
	HUD->NotifyHitBoxClick(TEXT("ResetExperiment"));
	TestEqual(TEXT("Reset button creates a fresh interval"), State->Experiment.Revision, Previous + 1);
	HUD->NotifyHitBoxClick(TEXT("ScenarioBaseline"));
	TestEqual(TEXT("HUD selects baseline preset"), State->Experiment.Scenario.Kind, ECombatExperimentKind::Baseline);
	TestFalse(TEXT("Baseline preset starts with a stationary target"), State->Experiment.Scenario.bMovingTarget);
	HUD->NotifyHitBoxClick(TEXT("TargetMoving"));
	HUD->NotifyHitBoxClick(TEXT("DistancePlus"));
	HUD->NotifyHitBoxClick(TEXT("ShotsPlus"));
	HUD->NotifyHitBoxClick(TEXT("RepeatsMinus"));
	TestTrue(TEXT("HUD overrides target motion"), State->Experiment.Scenario.bMovingTarget);
	TestEqual(TEXT("HUD changes distance"), State->Experiment.Scenario.DistanceMeters, 19);
	TestEqual(TEXT("HUD changes shots per pass"), State->Experiment.Scenario.ShotsPerRun, 25);
	TestEqual(TEXT("HUD changes repetitions"), State->Experiment.Scenario.Repetitions, 2);
	HUD->NotifyHitBoxClick(TEXT("MissOn"));
	TestFalse(TEXT("Baseline cannot enable intentional misses"), State->Experiment.Scenario.bIntentionalMisses);
	HUD->NotifyHitBoxClick(TEXT("ScenarioCompare"));
	TestFalse(TEXT("Comparison preset defaults to normal aim"), State->Experiment.Scenario.bIntentionalMisses);
	HUD->NotifyHitBoxClick(TEXT("MissOn"));
	TestTrue(TEXT("HUD explicitly enables intentional misses"), State->Experiment.Scenario.bIntentionalMisses);
	HUD->NotifyHitBoxClick(TEXT("MissOff"));
	TestFalse(TEXT("HUD disables intentional misses"), State->Experiment.Scenario.bIntentionalMisses);
	HUD->NotifyHitBoxClick(TEXT("MissOn"));
	HUD->NotifyHitBoxClick(TEXT("ScenarioCompare"));
	TestFalse(TEXT("Reselecting the experiment clears intentional misses"), State->Experiment.Scenario.bIntentionalMisses);
	HUD->NotifyHitBoxClick(TEXT("Lag500"));
	HUD->NotifyHitBoxClick(TEXT("LagPlus"));
	TestEqual(TEXT("HUD supports a custom 550ms delay"), State->Experiment.PacketLagMs, 550);
	HUD->NotifyHitBoxClick(TEXT("LagMinus"));
	TestEqual(TEXT("HUD supports decrement back to 500ms"), State->Experiment.PacketLagMs, 500);
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
	TestFalse(TEXT("Excessive lag rejected"), State->SetExperiment(true, 1050, Revision));
	TestEqual(TEXT("Invalid request does not reset measurement"), State->Experiment.Revision, Revision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatScenarioSettingsTest, "LatencyCombat.Experiment.ScenarioSettingsAndCustomLag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatScenarioSettingsTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatGameState* GS = Fixture.World->GetGameState<ACombatGameState>();
	FCombatScenarioSettings Settings;
	TestTrue(TEXT("Comparison defaults are valid"), Settings.IsValid());
	TestFalse(TEXT("Intentional misses default off"), Settings.bIntentionalMisses);
	TestEqual(TEXT("Comparison runs both policies"), Settings.PassCount(), 2);
	Settings.Kind = ECombatExperimentKind::Baseline;
	Settings.Repetitions = 3;
	TestTrue(TEXT("Server approves scenario setup"), GS->SetScenario(Settings, GS->Experiment.Revision));
	TestEqual(TEXT("Baseline uses the configured repetition count"), GS->Experiment.Scenario.PassCount(), 3);
	const uint32 Revision = GS->Experiment.Revision;
	Settings.ShotsPerRun = 61;
	TestFalse(TEXT("Out of range shots rejected"), GS->SetScenario(Settings, Revision));
	Settings.ShotsPerRun = 24;
	Settings.DistanceMeters = 31;
	TestFalse(TEXT("Out of range distance rejected"), GS->SetScenario(Settings, Revision));
	Settings.DistanceMeters = 18;
	Settings.Repetitions = 6;
	TestFalse(TEXT("Out of range repetitions rejected"), GS->SetScenario(Settings, Revision));
	Settings.Repetitions = 1;
	Settings.Kind = static_cast<ECombatExperimentKind>(255);
	TestFalse(TEXT("Unknown experiment kind rejected"), GS->SetScenario(Settings, Revision));
	TestEqual(TEXT("Bad setup preserves interval"), GS->Experiment.Revision, Revision);
	for (int32 Lag = 0; Lag <= 1000; Lag += 50)
	{
		TestTrue(TEXT("50ms steps including 500ms and 1000ms accepted"), GS->SetExperiment(true, Lag, GS->Experiment.Revision));
		TestEqual(TEXT("Custom delay retained"), GS->Experiment.PacketLagMs, Lag);
	}
	TestFalse(TEXT("Non-step delay rejected"), GS->SetExperiment(true, 175, GS->Experiment.Revision));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatScenarioLifecycleTest, "LatencyCombat.Experiment.ScenarioCancelRestoreAndRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatScenarioLifecycleTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* Red = Fixture.SpawnPlayer(FVector(-600, 80, 300), ETeamSide::Red);
	ACombatPlayerController* Blue = Fixture.SpawnPlayer(FVector(600, -80, 300), ETeamSide::Blue);
	ACombatGameState* GS = Fixture.World->GetGameState<ACombatGameState>();
	UCombatScenarioComponent* Runner = GS->ScenarioRunner;
	ACombatCharacter* Pawn = CastChecked<ACombatCharacter>(Red->GetPawn());
	Pawn->GetWeapon()->Ammo->ConsumeRound();
	Red->GetPlayerState<ACombatPlayerState>()->Health->ApplyDamage(20.f);
	const float Health = Red->GetPlayerState<ACombatPlayerState>()->Health->GetHealth();
	const float Armor = Red->GetPlayerState<ACombatPlayerState>()->Health->GetArmor();
	const FVector Location = Pawn->GetActorLocation();
	FCombatScenarioSettings Settings;
	Settings.DistanceMeters = 12;
	Settings.bMovingTarget = false;
	TestTrue(TEXT("Setup approved"), GS->SetScenario(Settings, GS->Experiment.Revision));
	TestTrue(TEXT("Start accepted"), Runner->Start(Red).IsEmpty());
	TestTrue(TEXT("Participants marked busy"), Runner->IsParticipant(Red) && Runner->IsParticipant(Blue));
	TestEqual(TEXT("Selected distance applied to actual pawns"),
		FVector::Dist2D(Red->GetPawn()->GetActorLocation(), Blue->GetPawn()->GetActorLocation()), 1200.0);
	TestFalse(TEXT("Duplicate start rejected"), Runner->Start(Blue).IsEmpty());
	TestFalse(TEXT("Setup cannot change during a run"), GS->SetScenario(Settings, GS->Experiment.Revision));
	Runner->Stop(TEXT("test cancel"));
	TestTrue(TEXT("Stop drains packets before restoring"), Runner->IsBusy());
	Fixture.Advance(2.5f);
	TestFalse(TEXT("Cancellation releases session"), Runner->IsBusy());
	TestTrue(TEXT("Position restored"), Pawn->GetActorLocation().Equals(Location, 1.f));
	TestEqual(TEXT("Health restored without healing"), Red->GetPlayerState<ACombatPlayerState>()->Health->GetHealth(), Health);
	TestEqual(TEXT("Armor restored"), Red->GetPlayerState<ACombatPlayerState>()->Health->GetArmor(), Armor);
	TestEqual(TEXT("Ammo restored"), Pawn->GetWeapon()->Ammo->GetMagazineAmmo(), 29);
	TestTrue(TEXT("Restart accepted"), Runner->Start(Red).IsEmpty());
	TestEqual(TEXT("Restart gets a new run ID"), Runner->GetState().RunId, uint32(2));
	Blue->GetPawn()->Destroy();
	Fixture.Advance(0.1f);
	TestEqual(TEXT("Destroyed participant cancels the run"), Runner->GetState().Stage, ECombatScenarioStage::Stopping);
	Fixture.Advance(2.5f);
	TestFalse(TEXT("Remaining participant is released after destruction"), Runner->IsBusy());
	TestEqual(TEXT("Remaining participant ammo is restored"), Pawn->GetWeapon()->Ammo->GetMagazineAmmo(), 29);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatScenarioRepetitionTest, "LatencyCombat.Experiment.ScenarioRepetitionsAndComparison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatScenarioRepetitionTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* Red = Fixture.SpawnPlayer(FVector(-900, 0, 88), ETeamSide::Red);
	Fixture.SpawnPlayer(FVector(900, 0, 88), ETeamSide::Blue);
	ACombatGameState* GS = Fixture.World->GetGameState<ACombatGameState>();
	UCombatScenarioComponent* Runner = GS->ScenarioRunner;
	FCombatScenarioSettings Settings;
	Settings.ShotsPerRun = 2;
	Settings.Repetitions = 2;
	TestTrue(TEXT("Comparison setup approved"), GS->SetScenario(Settings, GS->Experiment.Revision));
	TestTrue(TEXT("Comparison starts"), Runner->Start(Red).IsEmpty());
	uint32 ShotId = 0;
	for (int32 Pass = 0; Pass < 4; ++Pass)
	{
		Fixture.Advance(4.2f);
		TestEqual(TEXT("Selected pass retained"), Runner->GetState().Pass, Pass);
		TestEqual(TEXT("Policies alternate"), GS->LagCompensation.bEnabled, Pass % 2 == 1);
		FCombatShotResponse Result;
		Result.ExperimentRevision = GS->Experiment.Revision;
		Result.bDamagedPlayer = true;
		for (int32 Index = 1; Index <= 2; ++Index)
		{
			Result.ShotId = ++ShotId;
			Runner->AcceptSample(Red, Runner->GetState().RunId, Pass, Index, Result, 20.f);
			Runner->AcceptSample(Red, Runner->GetState().RunId, Pass, Index, Result, 20.f);
		}
	}
	Fixture.Advance(2.5f);
	TestEqual(TEXT("All repetitions complete"), Runner->GetState().Stage, ECombatScenarioStage::Complete);
	TestEqual(TEXT("Off counted exactly four responses"), Runner->GetState().Off.Shots, 4);
	TestEqual(TEXT("On counted exactly four responses"), Runner->GetState().On.Shots, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatScenarioMissOptionTest, "LatencyCombat.Experiment.IntentionalMissOptionControlsActualAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatScenarioMissOptionTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* Red = Fixture.SpawnPlayer(FVector(-500, 0, 88), ETeamSide::Red);
	ACombatPlayerController* Blue = Fixture.SpawnPlayer(FVector(500, 0, 88), ETeamSide::Blue);
	Red->SetAsLocalPlayerController();
	Red->Telemetry->SetComponentTickEnabled(true);
	CastChecked<ACombatCharacter>(Red->GetPawn())->GetCharacterMovement()->DisableMovement();
	CastChecked<ACombatCharacter>(Blue->GetPawn())->GetCharacterMovement()->DisableMovement();
	ACombatGameState* GS = Fixture.World->GetGameState<ACombatGameState>();
	UWeaponFireComponent* Fire = CastChecked<ACombatCharacter>(Red->GetPawn())->GetWeapon()->Fire;
	for (int32 Case = 0; Case < 3; ++Case)
	{
		FCombatScenarioSettings Settings;
		Settings.Kind = Case == 0 ? ECombatExperimentKind::Baseline : ECombatExperimentKind::RewindComparison;
		Settings.bMovingTarget = false;
		Settings.bIntentionalMisses = Case != 1;
		Settings.DistanceMeters = 10;
		Settings.ShotsPerRun = 4;
		TArray<float> Pitches;
		const FDelegateHandle Handle = Fire->OnShotSubmitted.AddLambda([Red, &Pitches](uint32 ShotId)
		{
			Pitches.Add(FRotator::NormalizeAxis(Red->GetControlRotation().Pitch));
		});
		TestTrue(TEXT("Aim test setup approved"), GS->SetScenario(Settings, GS->Experiment.Revision));
		TestTrue(TEXT("Aim test starts"), GS->ScenarioRunner->Start(Red).IsEmpty());
		TestEqual(TEXT("Approved option is copied into active run"),
			GS->ScenarioRunner->GetState().Settings.bIntentionalMisses, Settings.bIntentionalMisses);
		Fixture.Advance(20.f);
		Fire->OnShotSubmitted.Remove(Handle);
		const int32 ExpectedShots = Case == 0 ? 4 : 8;
		TestEqual(TEXT("Automatic fire completed every selected shot"), Pitches.Num(), ExpectedShots);
		TestEqual(TEXT("Automatic run completes"), GS->ScenarioRunner->GetState().Stage, ECombatScenarioStage::Complete);
		for (int32 Index = 0; Index < Pitches.Num(); ++Index)
		{
			const bool bShouldAimUp = Case == 2 && (Index == 3 || Index == 7);
			TestEqual(TEXT("Actual controller aim rises only for explicitly enabled fourth shots"), Pitches[Index] > 0.f, bShouldAimUp);
		}
		const FCombatScenarioRun& Run = GS->ScenarioRunner->GetState();
		TestEqual(TEXT("Actual damage excludes only the selected intentional misses"),
			Run.Off.Hits + Run.On.Hits, ExpectedShots - (Case == 2 ? 2 : 0));
	}
	return true;
}
#endif
