#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "CombatFireCameraShake.generated.h"

// A short camera-only impulse; it never modifies controller aim.
UCLASS()
class LATENCY_MULTICOMBAT_API UCombatFireCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UCombatFireCameraShake(const FObjectInitializer& ObjectInitializer);
};
