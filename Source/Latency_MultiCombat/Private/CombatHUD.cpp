#include "CombatHUD.h"
#include "CombatCharacter.h"
#include "CombatGameState.h"
#include "CombatPlayerController.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Components/CombatTelemetryComponent.h"
#include "Components/CombatTimeSyncComponent.h"
#include "Components/CombatScenarioComponent.h"
#include "Components/HealthComponent.h"
#include "Components/WeaponAmmoComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "UObject/ConstructorHelpers.h"

ACombatHUD::ACombatHUD()
{
	static ConstructorHelpers::FObjectFinder<UFontFace> Regular(
		TEXT("/Game/UI/Fonts/FF_NanumSquareNeo_Regular.FF_NanumSquareNeo_Regular"));
	static ConstructorHelpers::FObjectFinder<UFontFace> Bold(
		TEXT("/Game/UI/Fonts/FF_NanumSquareNeo_Bold.FF_NanumSquareNeo_Bold"));
	BodyFontFace = Regular.Object;
	HeadingFontFace = Bold.Object;
}

void ACombatHUD::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	BodyFont = CreateUIFont(BodyFontFace, TEXT("Regular"));
	HeadingFont = CreateUIFont(HeadingFontFace, TEXT("Bold"));
}

UFont* ACombatHUD::CreateUIFont(UFontFace* Face, FName TypefaceName)
{
	if (!Face)
	{
		return nullptr;
	}
	UFont* Font = NewObject<UFont>(this);
	Font->FontCacheType = EFontCacheType::Runtime;
	Font->LegacyFontSize = 12;
	Font->LegacyFontName = TypefaceName;
	FTypefaceEntry& Entry = Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.AddDefaulted_GetRef();
	Entry.Name = TypefaceName;
	Entry.Font = FFontData(Face);
	return Font;
}

void ACombatHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(PlayerOwner);
	const float Target = PC && PC->IsExperimentPanelOpen() ? 1.f : 0.f;
	// Constant progress gives a 0.25s transition; the easing curve is applied only when drawing.
	PanelSlideProgress = FMath::FInterpConstantTo(PanelSlideProgress, Target, DeltaSeconds, 4.f);
}

bool ACombatHUD::CanInteractWithExperimentPanel() const
{
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(PlayerOwner);
	return PC && PC->bShowMouseCursor && PC->IsExperimentPanelOpen() && PanelSlideProgress >= 1.f;
}

void ACombatHUD::DrawUIText(
	const FString& Text, FLinearColor Color, float X, float Y, float Scale, bool bHeading, float MaxWidth)
{
	if (!Canvas)
	{
		return;
	}
	UFont* Font = bHeading && HeadingFont ? HeadingFont.Get() : BodyFont.Get();
	float Width = 0.f;
	float Height = 0.f;
	Canvas->StrLen(Font, Text, Width, Height);
	// Keep text at its normal size while the panel moves beyond the viewport edge.
	const float RightEdge = PanelTextRightEdge > 0.f ? PanelTextRightEdge : Canvas->ClipX - 14.f;
	const float Available = MaxWidth > 0.f ? MaxWidth : RightEdge - X;
	if (Width > 0.f && Available > 0.f)
	{
		Scale = FMath::Min(Scale, Available / Width);
	}
	DrawText(Text, Color, X, Y, Font, Scale);
}

void ACombatHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner || !GetWorld())
	{
		return;
	}
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(PlayerOwner);
	const ACombatPlayerState* PS = PlayerOwner->GetPlayerState<ACombatPlayerState>();
	const ACombatCharacter* Character = Cast<ACombatCharacter>(PlayerOwner->GetPawn());
	const ACombatGameState* GS = GetWorld()->GetGameState<ACombatGameState>();
	const UCombatTelemetryComponent* Telemetry = PC ? PC->Telemetry.Get() : nullptr;

	const float Margin = 14.f;
	const float PanelW = FMath::Min(FMath::Clamp(Canvas->ClipX * 0.32f, 300.f, 410.f), Canvas->ClipX * 0.46f);
	const float OpenX = Canvas->ClipX - PanelW - Margin;
	const float Slide = FMath::SmoothStep(0.f, 1.f, PanelSlideProgress);
	const float PanelX = FMath::Lerp(Canvas->ClipX + Margin, OpenX, Slide);
	const float LeftLimitWidth =
		FMath::Max(260.f, FMath::Lerp(Canvas->ClipX - Margin * 2.f, OpenX - Margin * 2.f, Slide));

	DrawGameplayHUD(PS, Character, GS, LeftLimitWidth, FMath::Max(260.f, OpenX - Margin * 2.f));
	if (PanelSlideProgress > 0.f)
	{
		PanelTextRightEdge = PanelX + PanelW - 14.f;
		DrawRightTelemetryPanel(PC, GS, Telemetry, PanelX, PanelW);
		PanelTextRightEdge = 0.f;
	}
	else
	{
		DrawTelemetrySummary(PC, GS, Telemetry, OpenX, PanelW);
	}
}

void ACombatHUD::DrawTelemetrySummary(const ACombatPlayerController* PC, const ACombatGameState* GS,
	const UCombatTelemetryComponent* Telemetry, float PanelX, float PanelW)
{
	const FCombatTelemetrySummary Summary = Telemetry ? Telemetry->GetSummary() : FCombatTelemetrySummary();
	const FLinearColor Muted(0.70f, 0.77f, 0.84f);
	const bool bHasScenario = GS && GS->ScenarioRunner->GetState().RunId != 0;
	DrawRect(FLinearColor(0.02f, 0.04f, 0.08f, 0.78f), PanelX, 14.f, PanelW, bHasScenario ? 98.f : 78.f);
	DrawUIText(TEXT("현재 상태"), FLinearColor(0.4f, 0.9f, 1.f), PanelX + 8.f, 19.f, 0.85f, true);
	DrawUIText(TEXT("Tab 상세 보기"), Muted, PanelX + PanelW - 100.f, 20.f, 0.72f);
	DrawUIText(FString::Printf(TEXT("왕복 %.0fms | %.0f 프레임/초"), Summary.CurrentRttMs, Summary.CurrentFps),
		FLinearColor::White, PanelX + 8.f, 42.f, 0.85f);
	FString State = FString::Printf(TEXT("리와인드 %s | 편도 지연 %dms"),
		GS && GS->LagCompensation.bEnabled ? TEXT("켜짐") : TEXT("꺼짐"), GS ? GS->Experiment.PacketLagMs : 0);
	if (PC && PC->IsExperimentRequestPending())
	{
		State += TEXT(" · 적용 중");
	}
	else if (Telemetry && !Telemetry->IsMeasuring())
	{
		State += TEXT(" · 안정화 중");
	}
	DrawUIText(State, Muted, PanelX + 8.f, 65.f, 0.75f);
	if (bHasScenario)
	{
		DrawUIText(GS->ScenarioRunner->GetProgressLabel(), Muted, PanelX + 8.f, 87.f, 0.72f);
	}
}

void ACombatHUD::DrawGameplayHUD(const ACombatPlayerState* PS, const ACombatCharacter* Character,
	const ACombatGameState* GS, float LeftLimitWidth, float TopLimitWidth)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float Margin = 14.f;

	// 좌하단 로컬 플레이어 상태 박스 (오른쪽 텔레메트리 패널과 겹치지 않도록 좌하단 폭으로 제한)
	const float BottomBoxW = FMath::Min(530.f, LeftLimitWidth);
	const float BottomBoxH = 104.f;
	const float BottomBoxY = Canvas->ClipY - BottomBoxH - Margin;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), Margin, BottomBoxY, BottomBoxW, BottomBoxH);

	// 상단 중앙 팀 점수 박스
	const float ScoreBoxW = 310.f;
	const float ScoreBoxX = FMath::Min(CX - ScoreBoxW * 0.5f, TopLimitWidth + Margin - ScoreBoxW);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), FMath::Max(Margin, ScoreBoxX), Margin, ScoreBoxW, 34.f);

	if (GS)
	{
		const float RosterW = FMath::Min(390.f, TopLimitWidth);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.50f), Margin, 58.f, RosterW, GS->PlayerArray.Num() * 20.f + 14.f);
	}

	if (PS)
	{
		const FLinearColor TeamColor =
			PS->Team == ETeamSide::Red ? FLinearColor(1.f, 0.28f, 0.28f) : FLinearColor(0.32f, 0.62f, 1.f);
		DrawUIText(FString::Printf(TEXT("%s | 체력 %.0f/%.0f | 방어 %.0f"),
					   PS->Team == ETeamSide::Red ? TEXT("레드팀") : TEXT("블루팀"), PS->Health->GetHealth(),
					   PS->Health->GetMaxHealth(), PS->Health->GetArmor()),
			TeamColor, Margin + 14.f, BottomBoxY + 8.f, 1.f, true, BottomBoxW - 28.f);
		if (PS->Health->IsDead())
		{
			DrawUIText(TEXT("사망했습니다 · 재생성 대기 중"), FLinearColor::Red, CX - 115.f, CY + 40.f);
		}
	}

	if (Character && Character->GetWeapon())
	{
		DrawUIText(FString::Printf(TEXT("탄약 %d / %d · %s%s | 처치 %d / 사망 %d"),
					   Character->GetWeapon()->Ammo->GetMagazineAmmo(), Character->GetWeapon()->Ammo->GetReserveAmmo(),
					   Character->GetWeapon()->Fire->IsAutomaticFire() ? TEXT("연사") : TEXT("단발"),
					   Character->GetCombatState() == ECombatState::Reloading ? TEXT("   재장전 중") : TEXT(""),
					   PS ? PS->KillCount : 0, PS ? PS->DeathCount : 0),
			FLinearColor::White, Margin + 14.f, BottomBoxY + 34.f, 0.95f, false, BottomBoxW - 28.f);
		if (Character->GetCombatState() != ECombatState::Dead)
		{
			DrawLine(CX - 12.f, CY, CX - 4.f, CY, FLinearColor::White, 1.5f);
			DrawLine(CX + 4.f, CY, CX + 12.f, CY, FLinearColor::White, 1.5f);
			DrawLine(CX, CY - 12.f, CX, CY - 4.f, FLinearColor::White, 1.5f);
			DrawLine(CX, CY + 4.f, CX, CY + 12.f, FLinearColor::White, 1.5f);
			if (GetWorld()->GetTimeSeconds() - Character->GetWeapon()->Presentation->GetLastHitConfirmationTime() <
				0.15f)
			{
				DrawLine(CX - 8.f, CY - 8.f, CX + 8.f, CY + 8.f, FLinearColor::Red, 2.f);
				DrawLine(CX - 8.f, CY + 8.f, CX + 8.f, CY - 8.f, FLinearColor::Red, 2.f);
			}
		}
	}

	if (GS)
	{
		const float ScoreTextX = FMath::Max(Margin + 14.f, ScoreBoxX + 16.f);
		DrawUIText(FString::Printf(
					   TEXT("레드 %d : %d 블루  /  %d점 승리"), GS->RedTeamScore, GS->BlueTeamScore, GS->ScoreToWin),
			FLinearColor::White, ScoreTextX, Margin + 8.f, 1.f, true, ScoreBoxW - 32.f);
		if (GS->bMatchEnded)
		{
			DrawUIText(GS->WinningTeam == ETeamSide::Red ? TEXT("레드팀 승리") : TEXT("블루팀 승리"),
				FLinearColor::Yellow, CX - 100.f, CY - 70.f, 1.5f, true);
		}
		float Y = 66.f;
		for (APlayerState* Player : GS->PlayerArray)
		{
			if (const ACombatPlayerState* Other = Cast<ACombatPlayerState>(Player))
			{
				DrawUIText(FString::Printf(TEXT("%s  %s  체력 %.0f  처치/사망 %d/%d%s"),
							   Other->Team == ETeamSide::Red ? TEXT("레드") : TEXT("블루"), *Other->GetPlayerName(),
							   Other->Health->GetHealth(), Other->KillCount, Other->DeathCount,
							   Other->Health->IsDead() ? TEXT(" 사망") : TEXT("")),
					FLinearColor::White, Margin + 12.f, Y, 0.80f, false, FMath::Min(390.f, TopLimitWidth) - 24.f);
				Y += 20.f;
			}
		}
	}

	DrawUIText(TEXT("WASD 이동 · 마우스 조준 · Space 점프"), FLinearColor(0.72f, 0.72f, 0.72f), Margin + 14.f,
		BottomBoxY + 62.f, 0.75f, false, BottomBoxW - 28.f);
	DrawUIText(TEXT("좌클릭 발사 · 우클릭 정조준 · R 재장전 · B 단발/연사"), FLinearColor(0.72f, 0.72f, 0.72f),
		Margin + 14.f, BottomBoxY + 80.f, 0.75f, false, BottomBoxW - 28.f);
}

void ACombatHUD::DrawExperimentButton(
	FName Name, const FString& Label, float X, float Y, float W, bool bSelected, bool bEnabled)
{
	const FLinearColor Color =
		bSelected ? FLinearColor(0.08f, 0.39f, 0.50f, 0.95f) : FLinearColor(0.10f, 0.14f, 0.20f, 0.95f);
	DrawRect(Color, X, Y, W, 21.f);
	DrawUIText(Label, bEnabled ? FLinearColor::White : FLinearColor(0.45f, 0.48f, 0.52f), X + 7.f, Y + 3.f, 0.80f,
		false, W - 14.f);
	if (bEnabled && CanInteractWithExperimentPanel())
	{
		AddHitBox(FVector2D(X, Y), FVector2D(W, 21.f), Name, true);
	}
}

void ACombatHUD::DrawMetricRow(
	const TCHAR* Label, const FCombatMetricStats& Stats, float X, float Y, float W, FLinearColor Color)
{
	DrawUIText(Label, Color, X + 8.f, Y, 0.80f, false, 82.f);
	const float ColumnW = (W - 96.f) / 4.f;
	const float Values[] = {Stats.Current, Stats.Average(), Stats.Minimum, Stats.Maximum};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		DrawUIText(Stats.Count > 0 ? FString::Printf(TEXT("%.1f"), Values[Index]) : TEXT("--"), FLinearColor::White,
			X + 96.f + Index * ColumnW, Y, 0.80f, false, ColumnW - 3.f);
	}
}

void ACombatHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(PlayerOwner);
	const ACombatGameState* GS = GetWorld()->GetGameState<ACombatGameState>();
	if (!PC || !GS || !CanInteractWithExperimentPanel())
	{
		return;
	}
	if (BoxName == TEXT("ScenarioTab"))
	{
		bScenarioSettingsOpen = !bScenarioSettingsOpen;
		return;
	}
	if (!bScenarioSettingsOpen) { return; }
	if (BoxName == TEXT("StartScenario") || BoxName == TEXT("StopScenario"))
	{
		PC->RequestScenarioRunning(BoxName == TEXT("StartScenario"));
		return;
	}
	if (GS->ScenarioRunner->IsBusy() || PC->IsExperimentRequestPending()) { return; }
	FCombatScenarioSettings Settings = GS->Experiment.Scenario;
	bool bScenarioChange = true;
	if (BoxName == TEXT("ScenarioBaseline"))
	{
		Settings.Kind = ECombatExperimentKind::Baseline;
		Settings.bMovingTarget = false;
		Settings.bIntentionalMisses = false;
		Settings.Repetitions = 3;
	}
	else if (BoxName == TEXT("ScenarioCompare"))
	{
		Settings.Kind = ECombatExperimentKind::RewindComparison;
		Settings.bMovingTarget = true;
		Settings.bIntentionalMisses = false;
		Settings.Repetitions = 1;
	}
	else if (BoxName == TEXT("TargetStill")) { Settings.bMovingTarget = false; }
	else if (BoxName == TEXT("TargetMoving")) { Settings.bMovingTarget = true; }
	else if (BoxName == TEXT("MissOff")) { Settings.bIntentionalMisses = false; }
	else if (BoxName == TEXT("MissOn"))
	{
		if (Settings.Kind != ECombatExperimentKind::RewindComparison) { return; }
		Settings.bIntentionalMisses = true;
	}
	else if (BoxName == TEXT("DistanceMinus")) { --Settings.DistanceMeters; }
	else if (BoxName == TEXT("DistancePlus")) { ++Settings.DistanceMeters; }
	else if (BoxName == TEXT("ShotsMinus")) { --Settings.ShotsPerRun; }
	else if (BoxName == TEXT("ShotsPlus")) { ++Settings.ShotsPerRun; }
	else if (BoxName == TEXT("RepeatsMinus")) { --Settings.Repetitions; }
	else if (BoxName == TEXT("RepeatsPlus")) { ++Settings.Repetitions; }
	else { bScenarioChange = false; }
	if (bScenarioChange)
	{
		PC->RequestScenario(Settings);
		return;
	}
	if (BoxName == TEXT("LagMinus") || BoxName == TEXT("LagPlus"))
	{
		const int32 Delta = BoxName == TEXT("LagPlus") ? 50 : -50;
		PC->RequestExperiment(GS->LagCompensation.bEnabled, GS->Experiment.PacketLagMs + Delta);
		return;
	}
	if (BoxName == TEXT("RewindOn") || BoxName == TEXT("RewindOff"))
	{
		const bool bEnabled = BoxName == TEXT("RewindOn");
		if (bEnabled != GS->LagCompensation.bEnabled)
		{
			PC->RequestExperiment(bEnabled, GS->Experiment.PacketLagMs);
		}
	}
	else if (BoxName == TEXT("ResetExperiment"))
	{
		PC->ResetExperimentMetrics();
	}
	else
	{
		for (const int32 Lag : {0, 50, 100, 150, 200, 500})
		{
			if (BoxName == FName(*FString::Printf(TEXT("Lag%d"), Lag)))
			{
				if (Lag != GS->Experiment.PacketLagMs)
				{
					PC->RequestExperiment(GS->LagCompensation.bEnabled, Lag);
				}
				break;
			}
		}
	}
}

void ACombatHUD::DrawRightTelemetryPanel(const ACombatPlayerController* PC, const ACombatGameState* GS,
	const UCombatTelemetryComponent* Telemetry, float PanelX, float PanelW)
{
	const float Gap = 5.f;
	const float HeaderH = 42.f;
	const float ControlsH = 28.f;
	const float StatsH = 158.f;
	const float SummaryH = 64.f;
	const float GraphsH = FMath::Max(110.f, Canvas->ClipY - 28.f - HeaderH - ControlsH - StatsH - SummaryH - Gap * 5.f);
	const float NetH = GraphsH * 0.46f;
	const FLinearColor Background(0.02f, 0.04f, 0.08f, 0.82f);
	const FLinearColor Cyan(0.4f, 0.9f, 1.f);
	const FLinearColor Muted(0.65f, 0.72f, 0.80f);
	const FCombatTelemetrySummary Summary = Telemetry ? Telemetry->GetSummary() : FCombatTelemetrySummary();
	const bool bPending = PC && PC->IsExperimentRequestPending();
	const bool bRewind = GS && GS->LagCompensation.bEnabled;
	const int32 Lag = GS ? GS->Experiment.PacketLagMs : 0;
	float Y = 14.f;
	DrawRect(Background, PanelX, Y, PanelW, HeaderH);
	DrawUIText(TEXT("실시간 실험 지표"), Cyan, PanelX + 8.f, Y + 4.f, 0.90f, true);
	DrawExperimentButton(TEXT("ScenarioTab"), bScenarioSettingsOpen ? TEXT("통계 보기") : TEXT("실험 설정"),
		PanelX + PanelW - 159.f, Y + 2.f, 78.f, bScenarioSettingsOpen, true);
	DrawUIText(TEXT("Tab 접기"), Muted, PanelX + PanelW - 75.f, Y + 7.f, 0.72f);
	DrawUIText(
		FString::Printf(TEXT("%s | %s | 측정 구간 %u"), GetNetModeLabel(),
			PC && PC->TimeSync->IsReady() ? TEXT("동기화 완료") : TEXT("동기화 중"), GS ? GS->Experiment.Revision : 0),
		Muted, PanelX + 8.f, Y + 25.f, 0.70f);

	Y += HeaderH + Gap;
	if (bScenarioSettingsOpen)
	{
		DrawScenarioSettings(PC, GS, PanelX, PanelW, Y);
		return;
	}
	DrawRect(Background, PanelX, Y, PanelW, ControlsH);
	DrawUIText(FString::Printf(TEXT("리와인드 %s | 편도 %dms · 왕복 약 +%dms"),
		bRewind ? TEXT("켜짐") : TEXT("꺼짐"), Lag, Lag * 2), Muted, PanelX + 8.f, Y + 6.f, 0.76f);

	Y += ControlsH + Gap;
	DrawRect(Background, PanelX, Y, PanelW, StatsH);
	const bool bMeasuring = Telemetry && Telemetry->IsMeasuring();
	FString Status = bPending ? TEXT("서버 설정 적용 중...")
		: bMeasuring		  ? TEXT("측정 중 · 현재 조건의 전체 표본")
							  : TEXT("안정화 중 · 새 표본 수집 대기");
	if (PC && !PC->GetExperimentNotice().IsEmpty())
	{
		Status = PC->GetExperimentNotice();
	}
	else if (GS && GS->ScenarioRunner->GetState().RunId != 0)
	{
		Status = GS->ScenarioRunner->GetProgressLabel();
	}
	DrawUIText(Status, bMeasuring && !bPending ? FLinearColor(0.35f, 0.95f, 0.6f) : FLinearColor(1.f, 0.8f, 0.3f),
		PanelX + 8.f, Y + 4.f, 0.78f);
	const float ColumnW = (PanelW - 96.f) / 4.f;
	const TCHAR* Headers[] = {TEXT("현재"), TEXT("평균"), TEXT("최소"), TEXT("최대")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		DrawUIText(Headers[Index], Muted, PanelX + 96.f + Index * ColumnW, Y + 23.f, 0.76f);
	}
	const FCombatMetricStats* Stats[] = {
		&Summary.Rtt, &Summary.Fps, &Summary.Frame, &Summary.Ack, &Summary.ShotAge, &Summary.TargetAge};
	const TCHAR* Labels[] = {TEXT("왕복(ms)"), TEXT("프레임/초"), TEXT("프레임(ms)"), TEXT("응답(ms)"),
		TEXT("발사경과(ms)"), TEXT("되감기(ms)")};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		DrawMetricRow(Labels[Index], *Stats[Index], PanelX, Y + 42.f + Index * 16.f, PanelW,
			Index < 3 ? Cyan : FLinearColor(0.5f, 0.95f, 0.65f));
	}
	DrawUIText(FString::Printf(TEXT("표본 · 통신 %llu / 응답 %llu / 승인 %llu"), Summary.Rtt.Count, Summary.Ack.Count,
				   Summary.ShotAge.Count),
		Muted, PanelX + 8.f, Y + 140.f, 0.72f);

	Y += StatsH + Gap;
	DrawNetGraphBox(Telemetry, PanelX, Y, PanelW, NetH);
	Y += NetH + Gap;
	DrawShotChartBox(
		Telemetry, GS ? GS->LagCompensation.MaxRewindSeconds * 1000.f : 750.f, PanelX, Y, PanelW, GraphsH - NetH);
	Y += GraphsH - NetH + Gap;
	DrawRect(Background, PanelX, Y, PanelW, SummaryH);
	const float HitRate = Summary.TotalAccepted > 0 ? 100.f * Summary.TotalHits / Summary.TotalAccepted : 0.f;
	DrawUIText(FString::Printf(TEXT("요청 %d | 승인 %d | 거절 %d"), Summary.TotalSubmitted, Summary.TotalAccepted,
				   Summary.TotalRejected),
		FLinearColor::White, PanelX + 8.f, Y + 4.f, 0.80f);
	DrawUIText(FString::Printf(
				   TEXT("명중 %d / %d (%.1f%%) · 내 플레이어 기준"), Summary.TotalHits, Summary.TotalAccepted, HitRate),
		Muted, PanelX + 8.f, Y + 23.f, 0.78f);
	FCombatShotTelemetry Last;
	const FString LastLabel = Telemetry && Telemetry->GetLastShot(Last)
		? FString::Printf(TEXT("최근 #%u: %s [%s]"), Last.ShotId,
			  Last.Rejection != EShotRejection::None ? UCombatTelemetryComponent::RejectionToString(Last.Rejection)
				  : Last.bDamagedPlayer				 ? TEXT("명중")
													 : TEXT("빗나감"),
			  Last.bUsedRewind ? TEXT("리와인드") : TEXT("현재 판정"))
		: TEXT("좌클릭으로 사격하면 측정값을 표시합니다");
	DrawUIText(LastLabel, Muted, PanelX + 8.f, Y + 43.f, 0.78f);
}

void ACombatHUD::DrawScenarioSettings(const ACombatPlayerController* PC, const ACombatGameState* GS,
	float X, float W, float Y)
{
	if (!GS) { return; }
	const FLinearColor Background(0.02f, 0.04f, 0.08f, 0.86f);
	const FLinearColor Muted(0.70f, 0.77f, 0.84f);
	const FLinearColor Cyan(0.4f, 0.9f, 1.f);
	const bool bBusy = GS->ScenarioRunner->IsBusy();
	const bool bEnabled = ACombatGameState::SupportsLiveExperiments() && PC && !PC->IsExperimentRequestPending() && !bBusy;
	const int32 Lag = GS->Experiment.PacketLagMs;
	DrawRect(Background, X, Y, W, 119.f);
	DrawUIText(TEXT("리와인드"), Cyan, X + 8.f, Y + 4.f, 0.84f, true);
	const float Third = (W - 90.f) / 3.f;
	DrawExperimentButton(TEXT("RewindOff"), TEXT("끄기"), X + 80.f, Y + 2.f, Third, !GS->LagCompensation.bEnabled, bEnabled);
	DrawExperimentButton(TEXT("RewindOn"), TEXT("켜기"), X + 83.f + Third, Y + 2.f, Third, GS->LagCompensation.bEnabled, bEnabled);
	DrawExperimentButton(TEXT("ResetExperiment"), TEXT("초기화"), X + 86.f + Third * 2.f, Y + 2.f, Third, false, bEnabled);
	DrawUIText(FString::Printf(TEXT("편도 %dms · 추가 왕복 약 %dms"), Lag, Lag * 2), Muted, X + 8.f, Y + 28.f, 0.80f);
	const int32 Presets[] = {0, 50, 100, 150, 200, 500};
	const float LagW = (W - 31.f) / 6.f;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		DrawExperimentButton(FName(*FString::Printf(TEXT("Lag%d"), Presets[Index])), FString::FromInt(Presets[Index]),
			X + 8.f + Index * (LagW + 3.f), Y + 46.f, LagW, Lag == Presets[Index], bEnabled);
	}
	DrawExperimentButton(TEXT("LagMinus"), TEXT("-50"), X + 8.f, Y + 72.f, 57.f, false, bEnabled && Lag > 0);
	DrawUIText(FString::Printf(TEXT("%d ms"), Lag), Cyan, X + 78.f, Y + 75.f, 0.83f, true, W - 154.f);
	DrawExperimentButton(TEXT("LagPlus"), TEXT("+50"), X + W - 65.f, Y + 72.f, 57.f, false,
		bEnabled && Lag < FCombatExperimentState::MaxPacketLagMs);
	DrawUIText(TEXT("편도 0~1,000ms · 50ms 단위 조절"), Muted, X + 8.f, Y + 99.f, 0.73f);
	Y += 124.f;
	const bool bCompact = Canvas->ClipY < 540.f;
	const float RowStep = bCompact ? 23.f : 27.f;
	const float PlanY = 87.f + RowStep * 4.f;
	const float SettingsH = PlanY + 25.f;
	DrawRect(Background, X, Y, W, SettingsH);
	DrawUIText(TEXT("실험 설정"), Cyan, X + 8.f, Y + 4.f, 0.84f, true);
	const FCombatScenarioSettings& Settings = GS->Experiment.Scenario;
	const float ChoiceW = (W - 94.f) / 2.f;
	DrawUIText(TEXT("종류"), Muted, X + 8.f, Y + 31.f, 0.80f);
	DrawExperimentButton(TEXT("ScenarioBaseline"), TEXT("기준 지연"), X + 82.f, Y + 28.f, ChoiceW,
		Settings.Kind == ECombatExperimentKind::Baseline, bEnabled);
	DrawExperimentButton(TEXT("ScenarioCompare"), TEXT("보정 비교"), X + 86.f + ChoiceW, Y + 28.f, ChoiceW,
		Settings.Kind == ECombatExperimentKind::RewindComparison, bEnabled);
	DrawUIText(TEXT("표적"), Muted, X + 8.f, Y + 58.f, 0.80f);
	DrawExperimentButton(TEXT("TargetStill"), TEXT("정지"), X + 82.f, Y + 55.f, ChoiceW, !Settings.bMovingTarget, bEnabled);
	DrawExperimentButton(TEXT("TargetMoving"), TEXT("이동"), X + 86.f + ChoiceW, Y + 55.f, ChoiceW, Settings.bMovingTarget, bEnabled);
	DrawUIText(TEXT("의도적 빗맞힘"), Muted, X + 8.f, Y + 85.f, 0.73f, false, 70.f);
	const bool bMissEnabled = bEnabled && Settings.Kind == ECombatExperimentKind::RewindComparison;
	DrawExperimentButton(TEXT("MissOff"), TEXT("끄기"), X + 82.f, Y + 82.f, ChoiceW, !Settings.bIntentionalMisses, bMissEnabled);
	DrawExperimentButton(TEXT("MissOn"), TEXT("켜기"), X + 86.f + ChoiceW, Y + 82.f, ChoiceW, Settings.bIntentionalMisses, bMissEnabled);
	const TCHAR* Labels[] = {TEXT("거리"), TEXT("발사 수"), TEXT("반복")};
	const TCHAR* Minus[] = {TEXT("DistanceMinus"), TEXT("ShotsMinus"), TEXT("RepeatsMinus")};
	const TCHAR* Plus[] = {TEXT("DistancePlus"), TEXT("ShotsPlus"), TEXT("RepeatsPlus")};
	const int32 Values[] = {Settings.DistanceMeters, Settings.ShotsPerRun, Settings.Repetitions};
	const int32 Min[] = {5, 1, 1};
	const int32 Max[] = {30, 60, 5};
	const TCHAR* Units[] = {TEXT("m"), TEXT("발 / 구간"), TEXT("회 / 모드")};
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const float RowY = Y + 82.f + (Row + 1) * RowStep;
		DrawUIText(Labels[Row], Muted, X + 8.f, RowY + 3.f, 0.80f);
		DrawExperimentButton(FName(Minus[Row]), TEXT("-"), X + 82.f, RowY, 32.f, false, bEnabled && Values[Row] > Min[Row]);
		DrawUIText(FString::Printf(TEXT("%d %s"), Values[Row], Units[Row]), FLinearColor::White,
			X + 125.f, RowY + 3.f, 0.80f, false, W - 180.f);
		DrawExperimentButton(FName(Plus[Row]), TEXT("+"), X + W - 40.f, RowY, 32.f, false, bEnabled && Values[Row] < Max[Row]);
	}
	DrawUIText(FString::Printf(TEXT("예정 %d구간 · 총 %d발 | %s"), Settings.PassCount(),
		Settings.PassCount() * Settings.ShotsPerRun,
		Settings.Kind == ECombatExperimentKind::Baseline ? TEXT("보정 끄기") : TEXT("끄기/켜기 순서")),
		Muted, X + 8.f, Y + PlanY, 0.73f);
	Y += SettingsH + 5.f;
	DrawRect(Background, X, Y, W, bCompact ? 99.f : 117.f);
	const float Half = (W - 20.f) * 0.5f;
	DrawExperimentButton(TEXT("StartScenario"), TEXT("실험 시작"), X + 8.f, Y + 4.f, Half, false, bEnabled);
	DrawExperimentButton(TEXT("StopScenario"), TEXT("중지"), X + 12.f + Half, Y + 4.f, Half, false,
		bBusy && GS->ScenarioRunner->GetState().Stage != ECombatScenarioStage::Stopping);
	DrawUIText(GS->ScenarioRunner->GetProgressLabel(), Cyan, X + 8.f, Y + 32.f, 0.77f);
	const FCombatScenarioRun& Run = GS->ScenarioRunner->GetState();
	DrawUIText(FString::Printf(TEXT("끄기 명중 %d/%d · 켜기 명중 %d/%d"), Run.Off.Hits, Run.Off.Shots, Run.On.Hits, Run.On.Shots),
		Muted, X + 8.f, Y + 55.f, 0.76f);
	FString Notice = bBusy ? TEXT("진행 중에는 중지 후 조건을 변경해 주세요.") :
		TEXT("1 커서 · 두 플레이어 임시 배치 / 종료 후 복구");
	if (PC && !PC->GetExperimentNotice().IsEmpty()) { Notice = PC->GetExperimentNotice(); }
	else if (!Run.Notice.IsEmpty()) { Notice = Run.Notice; }
	DrawUIText(Notice, Muted, X + 8.f, Y + 78.f, 0.70f);
	if (!bCompact)
	{
		DrawUIText(TEXT("F6 보정 · F7 지연 · F8 통계 초기화"), Muted, X + 8.f, Y + 98.f, 0.70f);
	}
}

void ACombatHUD::DrawNetGraphBox(const UCombatTelemetryComponent* Telemetry, float X, float Y, float W, float H)
{
	DrawRect(FLinearColor(0.02f, 0.04f, 0.08f, 0.74f), X, Y, W, H);
	DrawLine(X, Y, X + W, Y, FLinearColor(0.3f, 0.75f, 1.f, 0.7f), 1.f);

	const int32 Count = Telemetry ? Telemetry->GetNetSampleCount() : 0;
	float MaxScaleMs = 100.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FCombatNetSample& Sample = Telemetry->GetNetSampleAt(Index);
		MaxScaleMs = FMath::Max3(MaxScaleMs, Sample.RttMs * 1.15f, Sample.FrameMs * 1.15f);
	}
	MaxScaleMs = FMath::CeilToFloat(MaxScaleMs / 50.f) * 50.f;

	DrawUIText(FString::Printf(TEXT("왕복(하늘색) · 프레임(노랑) [0~%.0fms]"), MaxScaleMs),
		FLinearColor(0.75f, 0.9f, 1.f), X + 10.f, Y + 5.f, 0.86f);

	const float PlotX = X + 10.f;
	const float PlotY = Y + 24.f;
	const float PlotW = W - 20.f;
	const float PlotH = FMath::Max(12.f, H - 32.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), PlotX, PlotY, PlotW, PlotH);

	// 수평 기준선 (25%, 50%, 75%, 100%)
	for (int32 Grid = 1; Grid <= 4; ++Grid)
	{
		const float GridY = PlotY + PlotH - (PlotH * Grid * 0.25f);
		DrawLine(PlotX, GridY, PlotX + PlotW, GridY, FLinearColor(1.f, 1.f, 1.f, 0.10f), 1.f);
	}
	DrawLine(PlotX, PlotY + PlotH, PlotX + PlotW, PlotY + PlotH, FLinearColor(1.f, 1.f, 1.f, 0.28f), 1.f);

	if (Count < 2)
	{
		DrawUIText(TEXT("통신·프레임 표본 수집 중..."), FLinearColor(0.55f, 0.6f, 0.65f), PlotX + 8.f,
			PlotY + PlotH * 0.4f, 0.82f);
		return;
	}

	const int32 MaxPoints = UCombatTelemetryComponent::MaxNetSamples;
	for (int32 Index = 1; Index < Count; ++Index)
	{
		const FCombatNetSample& Prev = Telemetry->GetNetSampleAt(Index - 1);
		const FCombatNetSample& Curr = Telemetry->GetNetSampleAt(Index);
		const float X0 = PlotX + (static_cast<float>(Index - 1) / (MaxPoints - 1)) * PlotW;
		const float X1 = PlotX + (static_cast<float>(Index) / (MaxPoints - 1)) * PlotW;

		const float RttY0 = PlotY + PlotH - FMath::Clamp(Prev.RttMs / MaxScaleMs, 0.f, 1.f) * PlotH;
		const float RttY1 = PlotY + PlotH - FMath::Clamp(Curr.RttMs / MaxScaleMs, 0.f, 1.f) * PlotH;
		DrawLine(X0, RttY0, X1, RttY1, FLinearColor(0.2f, 0.9f, 1.f, 0.95f), 1.6f);

		const float FrameY0 = PlotY + PlotH - FMath::Clamp(Prev.FrameMs / MaxScaleMs, 0.f, 1.f) * PlotH;
		const float FrameY1 = PlotY + PlotH - FMath::Clamp(Curr.FrameMs / MaxScaleMs, 0.f, 1.f) * PlotH;
		DrawLine(X0, FrameY0, X1, FrameY1, FLinearColor(1.f, 0.88f, 0.25f, 0.90f), 1.3f);
	}
}

void ACombatHUD::DrawShotChartBox(
	const UCombatTelemetryComponent* Telemetry, float MaxRewindMs, float X, float Y, float W, float H)
{
	DrawRect(FLinearColor(0.02f, 0.04f, 0.08f, 0.74f), X, Y, W, H);
	DrawLine(X, Y, X + W, Y, FLinearColor(0.3f, 0.95f, 0.65f, 0.7f), 1.f);

	const int32 Count = Telemetry ? Telemetry->GetShotSampleCount() : 0;
	float MaxScaleMs = 150.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FCombatShotTelemetry& Shot = Telemetry->GetShotSampleAt(Index);
		MaxScaleMs = FMath::Max3(MaxScaleMs, Shot.AckMs * 1.15f, Shot.TargetAgeMs * 1.15f);
	}
	MaxScaleMs = FMath::CeilToFloat(MaxScaleMs / 50.f) * 50.f;

	DrawUIText(FString::Printf(TEXT("사격 응답 · 되감기 [0~%.0fms]"), MaxScaleMs), FLinearColor(0.65f, 0.98f, 0.78f),
		X + 10.f, Y + 5.f, 0.86f);
	DrawUIText(TEXT("초록 명중 · 회색 빗나감 · 빨강 거절 · 하늘 되감기"), FLinearColor(0.68f, 0.74f, 0.8f), X + 10.f,
		Y + 21.f, 0.66f);

	const float PlotX = X + 10.f;
	const float PlotY = Y + 37.f;
	const float PlotW = W - 20.f;
	const float PlotH = FMath::Max(12.f, H - 45.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), PlotX, PlotY, PlotW, PlotH);

	for (int32 Grid = 1; Grid <= 2; ++Grid)
	{
		const float GridY = PlotY + PlotH - (PlotH * Grid * 0.5f);
		DrawLine(PlotX, GridY, PlotX + PlotW, GridY, FLinearColor(1.f, 1.f, 1.f, 0.10f), 1.f);
	}
	if (MaxRewindMs > 0.f && MaxRewindMs <= MaxScaleMs)
	{
		const float LimitY = PlotY + PlotH - (MaxRewindMs / MaxScaleMs) * PlotH;
		DrawLine(PlotX, LimitY, PlotX + PlotW, LimitY, FLinearColor(1.f, 0.35f, 0.35f, 0.55f), 1.f);
	}
	DrawLine(PlotX, PlotY + PlotH, PlotX + PlotW, PlotY + PlotH, FLinearColor(1.f, 1.f, 1.f, 0.28f), 1.f);

	if (Count <= 0)
	{
		DrawUIText(TEXT("아직 사격 표본이 없습니다"), FLinearColor(0.55f, 0.6f, 0.65f), PlotX + 8.f,
			PlotY + PlotH * 0.35f, 0.82f);
		return;
	}

	const int32 SlotCount = UCombatTelemetryComponent::MaxShotSamples;
	const float SlotW = PlotW / SlotCount;
	const float BarW = FMath::Max(3.f, SlotW - 3.f);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FCombatShotTelemetry& Shot = Telemetry->GetShotSampleAt(Index);
		const float BarX = PlotX + Index * SlotW + 1.5f;

		const float AckRatio = FMath::Clamp(Shot.AckMs / MaxScaleMs, 0.04f, 1.f);
		const float AckH = AckRatio * PlotH;
		const FLinearColor BarColor = Shot.Rejection != EShotRejection::None ? FLinearColor(0.95f, 0.22f, 0.22f, 0.90f)
			: Shot.bDamagedPlayer											 ? FLinearColor(0.20f, 0.88f, 0.42f, 0.88f)
																			 : FLinearColor(0.55f, 0.60f, 0.66f, 0.78f);
		DrawRect(BarColor, BarX, PlotY + PlotH - AckH, BarW, AckH);

		// 리와인드가 사용된 경우 표적 되감기 시간(TargetAgeMs)을 안쪽 청록색 막대로 겹쳐 표시
		if (Shot.bUsedRewind && Shot.TargetAgeMs > 0.f)
		{
			const float RewindH = FMath::Clamp(Shot.TargetAgeMs / MaxScaleMs, 0.03f, 1.f) * PlotH;
			const float InnerW = FMath::Max(2.f, BarW * 0.48f);
			DrawRect(FLinearColor(0.15f, 0.92f, 1.f, 0.95f), BarX + (BarW - InnerW) * 0.5f, PlotY + PlotH - RewindH,
				InnerW, RewindH);
		}
	}
}

const TCHAR* ACombatHUD::GetNetModeLabel() const
{
	if (!GetWorld())
	{
		return TEXT("확인 중");
	}
	switch (GetWorld()->GetNetMode())
	{
		case NM_Client:
			return TEXT("클라이언트");
		case NM_ListenServer:
			return TEXT("호스트");
		case NM_DedicatedServer:
			return TEXT("전용 서버");
		case NM_Standalone:
		default:
			return TEXT("단독 실행");
	}
}
