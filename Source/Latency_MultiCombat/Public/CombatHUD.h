#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CombatHUD.generated.h"

UCLASS()
class LATENCY_MULTICOMBAT_API ACombatHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
