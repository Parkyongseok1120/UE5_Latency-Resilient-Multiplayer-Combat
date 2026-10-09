#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CombatShotSample.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMeasurementTest, "LatencyCombat.Measurement.Correlation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatMeasurementTest::RunTest(const FString& Parameters)
{
	FCombatShotSample Sample;
	Sample.ShotId = 42;
	Sample.StartedAt = 10.0;
	Sample.HealthBefore = 100.f;
	TestFalse(TEXT("Foreign feedback rejected"), Sample.Feedback(41, true, 10.1));
	TestFalse(TEXT("Negative elapsed feedback rejected"), Sample.Feedback(42, true, 9.9));
	TestFalse(TEXT("Foreign confirmation rejected"), Sample.Confirm(41, 10.1));
	Sample.ObserveHealth(100.f, 10.1);
	TestEqual(TEXT("Unchanged health is not a response"), Sample.HealthMs, -1.0);
	Sample.ObserveHealth(75.f, 10.2);
	TestTrue(TEXT("Confirmation can precede feedback"), Sample.Confirm(42, 10.3));
	TestFalse(TEXT("Await feedback even with HP and confirmation"), Sample.IsComplete());
	TestTrue(TEXT("Matching feedback accepted"), Sample.Feedback(42, true, 10.4));
	TestTrue(TEXT("All hit responses complete"), Sample.IsComplete());
	TestFalse(TEXT("Duplicate feedback rejected"), Sample.Feedback(42, false, 11.0));
	TestFalse(TEXT("Duplicate confirmation rejected"), Sample.Confirm(42, 11.0));
	Sample.ObserveHealth(50.f, 11.0);
	TestEqual(TEXT("First HP response preserved"), Sample.HealthAfter, 75.f);
	TestTrue(TEXT("Same monotonic clock elapsed"), FMath::IsNearlyEqual(Sample.HealthMs, 200.0, 0.0001));
	FCombatShotSample Miss;
	Miss.ShotId = 7;
	Miss.Feedback(7, false, 0.1);
	TestTrue(TEXT("Miss needs no HP change or confirmation"), Miss.IsComplete());
	TestEqual(TEXT("Missing response remains sentinel"), Miss.HealthMs, -1.0);
	return true;
}
#endif
