#include "Data/CombatNetworkDataAsset.h"
#include "CombatGameState.h"
#include "Engine/World.h"

FLagCompensationSettings FLagCompensationSettings::Normalized() const
{
	FLagCompensationSettings Result = *this;
	const FLagCompensationSettings Defaults;
	Result.MaxRewindSeconds =
		FMath::IsFinite(MaxRewindSeconds) ? FMath::Clamp(MaxRewindSeconds, 0.05f, 2.f) : Defaults.MaxRewindSeconds;
	Result.RecordInterval =
		FMath::IsFinite(RecordInterval) ? FMath::Clamp(RecordInterval, 0.008f, 0.1f) : Defaults.RecordInterval;
	Result.FutureTolerance =
		FMath::IsFinite(FutureTolerance) ? FMath::Clamp(FutureTolerance, 0.f, 0.1f) : Defaults.FutureTolerance;
	Result.DisplayDelaySeconds = FMath::IsFinite(DisplayDelaySeconds) ? FMath::Clamp(DisplayDelaySeconds, 0.f, 0.2f)
																	  : Defaults.DisplayDelaySeconds;
	Result.MaxTargetViewDelay =
		FMath::IsFinite(MaxTargetViewDelay) ? FMath::Clamp(MaxTargetViewDelay, 0.f, 0.5f) : Defaults.MaxTargetViewDelay;
	Result.MaxInterpolationDistance = FMath::IsFinite(MaxInterpolationDistance)
		? FMath::Clamp(MaxInterpolationDistance, 50.f, 10000.f)
		: Defaults.MaxInterpolationDistance;
	Result.ClockSyncInterval =
		FMath::IsFinite(ClockSyncInterval) ? FMath::Clamp(ClockSyncInterval, 0.25f, 10.f) : Defaults.ClockSyncInterval;
	return Result;
}

FLagCompensationSettings FLagCompensationSettings::Get(const UWorld* World)
{
	const ACombatGameState* State = World ? World->GetGameState<ACombatGameState>() : nullptr;
	return State ? State->LagCompensation : FLagCompensationSettings();
}
