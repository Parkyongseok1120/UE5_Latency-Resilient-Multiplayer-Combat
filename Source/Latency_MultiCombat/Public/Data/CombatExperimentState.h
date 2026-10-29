#pragma once

#include "CoreMinimal.h"
#include "CombatExperimentState.generated.h"

UENUM()
enum class ECombatExperimentKind : uint8
{
	Baseline,
	RewindComparison
};

USTRUCT()
struct FCombatScenarioSettings
{
	GENERATED_BODY()
	UPROPERTY()
	ECombatExperimentKind Kind = ECombatExperimentKind::RewindComparison;
	UPROPERTY()
	bool bMovingTarget = true;
	UPROPERTY()
	bool bIntentionalMisses = false;
	UPROPERTY()
	int32 DistanceMeters = 18;
	UPROPERTY()
	int32 ShotsPerRun = 24;
	UPROPERTY()
	int32 Repetitions = 1;
	bool IsValid() const
	{
		return (Kind == ECombatExperimentKind::Baseline || Kind == ECombatExperimentKind::RewindComparison) &&
			DistanceMeters >= 5 && DistanceMeters <= 30 && ShotsPerRun >= 1 && ShotsPerRun <= 60 &&
			Repetitions >= 1 && Repetitions <= 5;
	}
	int32 PassCount() const { return Repetitions * (Kind == ECombatExperimentKind::RewindComparison ? 2 : 1); }
	bool ShouldIntentionallyMiss(int32 ShotIndex) const
	{
		return bIntentionalMisses && Kind == ECombatExperimentKind::RewindComparison && ShotIndex >= 0 && ShotIndex % 4 == 3;
	}
};

// A server-approved measurement interval. Revision zero preserves startup/command-line settings.
USTRUCT()
struct FCombatExperimentState
{
	GENERATED_BODY()
	UPROPERTY()
	uint32 Revision = 0;
	UPROPERTY()
	bool bRewindEnabled = true;
	UPROPERTY()
	int32 PacketLagMs = 0;
	UPROPERTY()
	FCombatScenarioSettings Scenario;
	UPROPERTY()
	double CollectAfterServerTime = 0.0;

	static constexpr int32 MaxPacketLagMs = 1000;
	static bool IsValidLag(int32 Value) { return Value >= 0 && Value <= MaxPacketLagMs && Value % 50 == 0; }
};
