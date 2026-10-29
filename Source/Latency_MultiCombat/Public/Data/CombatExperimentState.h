#pragma once

#include "CoreMinimal.h"
#include "CombatExperimentState.generated.h"

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
	double CollectAfterServerTime = 0.0;

	static bool IsValidLag(int32 Value) { return Value >= 0 && Value <= 200 && Value % 50 == 0; }
};
