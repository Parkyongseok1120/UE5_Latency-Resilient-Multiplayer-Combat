#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CombatHUD.generated.h"

class ACombatCharacter;
class ACombatGameState;
class ACombatPlayerController;
class ACombatPlayerState;
class UCombatTelemetryComponent;

// 상태를 직접 보관하지 않고 각 SSOT 컴포넌트(PlayerState, GameState, Weapon, Telemetry)를 조회해 그리는 순수 뷰.
UCLASS()
class LATENCY_MULTICOMBAT_API ACombatHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;

private:
	void DrawGameplayHUD(const ACombatPlayerState* PS, const ACombatCharacter* Character, const ACombatGameState* GS,
		float LeftLimitWidth);
	void DrawRightTelemetryPanel(const ACombatPlayerController* PC, const ACombatGameState* GS,
		const UCombatTelemetryComponent* Telemetry, float PanelX, float PanelW);
	void DrawNetGraphBox(const UCombatTelemetryComponent* Telemetry, float X, float Y, float W, float H);
	void DrawShotChartBox(
		const UCombatTelemetryComponent* Telemetry, float MaxRewindMs, float X, float Y, float W, float H);
	const TCHAR* GetNetModeLabel() const;
};
