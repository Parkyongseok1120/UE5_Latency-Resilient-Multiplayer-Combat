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
	// This is the ONLY input setup needed in C++. All key→action mappings live in this editor asset.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> CombatMappingContext;

	virtual void BeginPlay() override;

protected:
	// Enable the mapping context on spawn
	void SetupEnhancedInput();
};
