// Player Controller (Enhanced Input)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.generated.h"

class UCombatBenchmarkComponent;
class UCombatSmokeComponent;
class UCombatTimeSyncComponent;

UCLASS()
class LATENCY_MULTICOMBAT_API ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACombatPlayerController();
	UPROPERTY(VisibleAnywhere, Category = "Network")
	TObjectPtr<UCombatTimeSyncComponent> TimeSync;
	UPROPERTY(VisibleAnywhere, Category = "Testing")
	TObjectPtr<UCombatBenchmarkComponent> Benchmark;
	UPROPERTY(VisibleAnywhere, Category = "Testing")
	TObjectPtr<UCombatSmokeComponent> Smoke;

	virtual void BeginPlay() override;
};
