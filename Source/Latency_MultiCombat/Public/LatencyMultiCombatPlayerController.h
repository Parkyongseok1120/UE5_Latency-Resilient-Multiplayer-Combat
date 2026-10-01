// Latency_MultiCombat - Player Controller (Enhanced Input)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LatencyMultiCombatPlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class LATENCY_MULTICOMBAT_API ALatencyMultiCombatPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALatencyMultiCombatPlayerController();

	// Input Mapping Context - assign in Blueprint subclass or Default__ asset
	// Optional mapping context for a complete set of character Enhanced Input actions.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> CombatMappingContext;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	// Enable the mapping context on spawn
	void SetupEnhancedInput();

private:
#if !UE_BUILD_SHIPPING
	// Opt-in client automation uses the same gameplay RPCs as mouse input.
	void TickBaselineSmoke();
	bool bSmokeEnabled = false;
	bool bSmokeShooter = false;
	bool bSmokeSawDamage = false;
	bool bSmokeSawDeath = false;
	bool bSmokeFinished = false;
	float SmokeStartTime = 0.f;
	float SmokeLastHealth = -1.f;
#endif
};
