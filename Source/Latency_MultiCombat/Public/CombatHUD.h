#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CombatHUD.generated.h"

class ACombatCharacter;
class ACombatGameState;
class ACombatPlayerController;
class ACombatPlayerState;
class UCombatTelemetryComponent;
class UFont;
class UFontFace;
struct FCombatMetricStats;

// 게임 상태는 SSOT에서 조회하고, 패널 슬라이드 진행도만 로컬 표시 상태로 보관한다.
UCLASS()
class LATENCY_MULTICOMBAT_API ACombatHUD : public AHUD
{
	GENERATED_BODY()
public:
	ACombatHUD();
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	bool CanInteractWithExperimentPanel() const;

private:
	float PanelSlideProgress = 0.f;
	float PanelTextRightEdge = 0.f;
	void DrawTelemetrySummary(const ACombatPlayerController* PC, const ACombatGameState* GS,
		const UCombatTelemetryComponent* Telemetry, float PanelX, float PanelW);
	UPROPERTY(EditDefaultsOnly, Category = "UI|Fonts")
	TObjectPtr<UFontFace> BodyFontFace;
	UPROPERTY(EditDefaultsOnly, Category = "UI|Fonts")
	TObjectPtr<UFontFace> HeadingFontFace;
	UPROPERTY(Transient)
	TObjectPtr<UFont> BodyFont;
	UPROPERTY(Transient)
	TObjectPtr<UFont> HeadingFont;
	UFont* CreateUIFont(UFontFace* Face, FName TypefaceName);
	void DrawUIText(const FString& Text, FLinearColor Color, float X, float Y, float Scale = 1.f,
		bool bHeading = false, float MaxWidth = 0.f);
	void DrawGameplayHUD(const ACombatPlayerState* PS, const ACombatCharacter* Character, const ACombatGameState* GS,
		float LeftLimitWidth, float TopLimitWidth);
	void DrawRightTelemetryPanel(const ACombatPlayerController* PC, const ACombatGameState* GS,
		const UCombatTelemetryComponent* Telemetry, float PanelX, float PanelW);
	void DrawExperimentButton(FName Name, const FString& Label, float X, float Y, float W, bool bSelected,
		bool bEnabled);
	void DrawMetricRow(const TCHAR* Label, const FCombatMetricStats& Stats, float X, float Y, float W,
		FLinearColor Color);
	void DrawNetGraphBox(const UCombatTelemetryComponent* Telemetry, float X, float Y, float W, float H);
	void DrawShotChartBox(
		const UCombatTelemetryComponent* Telemetry, float MaxRewindMs, float X, float Y, float W, float H);
	const TCHAR* GetNetModeLabel() const;
};
