#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LatencyMultiCombatHUD.generated.h"

UCLASS()
class LATENCY_MULTICOMBAT_API ALatencyMultiCombatHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
