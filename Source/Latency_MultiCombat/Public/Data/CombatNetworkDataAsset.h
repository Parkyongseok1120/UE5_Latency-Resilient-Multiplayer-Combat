#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatNetworkDataAsset.generated.h"

class UWorld;

USTRUCT(BlueprintType)
struct FLagCompensationSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float MaxRewindSeconds = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.008", ClampMax = "0.1"))
	float RecordInterval = 1.f / 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "0.1"))
	float FutureTolerance = 0.05f;
	// 원격 캐릭터에 추가 스무딩을 쓸 때만 그 지연을 더해줌.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "0.2"))
	float DisplayDelaySeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "0.5"))
	float MaxTargetViewDelay = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "50"))
	float MaxInterpolationDistance = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.25", ClampMax = "10"))
	float ClockSyncInterval = 1.f;

	FLagCompensationSettings Normalized() const;
	static FLagCompensationSettings Get(const UWorld* World);
};

// GameMode에서 에셋을 지정하면 보정된 설정을 GameState로 공유함.
UCLASS(BlueprintType)
class LATENCY_MULTICOMBAT_API UCombatNetworkDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Network")
	FLagCompensationSettings LagCompensation;
};
