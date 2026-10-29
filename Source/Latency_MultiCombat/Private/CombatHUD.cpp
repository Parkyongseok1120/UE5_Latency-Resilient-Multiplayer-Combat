#include "CombatHUD.h"
#include "CombatCharacter.h"
#include "CombatGameState.h"
#include "CombatPlayerController.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Components/CombatTelemetryComponent.h"
#include "Components/CombatTimeSyncComponent.h"
#include "Components/HealthComponent.h"
#include "Components/WeaponAmmoComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

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
	const float PanelW = FMath::Clamp(Canvas->ClipX * 0.30f, 285.f, 380.f);
	const float PanelX = Canvas->ClipX - PanelW - Margin;
	const float LeftLimitWidth = FMath::Max(260.f, PanelX - Margin * 2.f);

	DrawGameplayHUD(PS, Character, GS, LeftLimitWidth);
	DrawRightTelemetryPanel(PC, GS, Telemetry, PanelX, PanelW);
}

void ACombatHUD::DrawGameplayHUD(const ACombatPlayerState* PS, const ACombatCharacter* Character,
	const ACombatGameState* GS, float LeftLimitWidth)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const float Margin = 14.f;

	// 좌하단 로컬 플레이어 상태 박스 (오른쪽 텔레메트리 패널과 겹치지 않도록 좌하단 폭으로 제한)
	const float BottomBoxW = FMath::Min(530.f, LeftLimitWidth);
	const float BottomBoxH = 92.f;
	const float BottomBoxY = Canvas->ClipY - BottomBoxH - Margin;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), Margin, BottomBoxY, BottomBoxW, BottomBoxH);

	// 상단 중앙 팀 점수 박스
	const float ScoreBoxW = 310.f;
	const float ScoreBoxX = FMath::Min(CX - ScoreBoxW * 0.5f, LeftLimitWidth + Margin - ScoreBoxW);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), FMath::Max(Margin, ScoreBoxX), Margin, ScoreBoxW, 34.f);

	if (GS)
	{
		const float RosterW = FMath::Min(390.f, LeftLimitWidth);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.50f), Margin, 58.f, RosterW, GS->PlayerArray.Num() * 20.f + 14.f);
	}

	if (PS)
	{
		const FLinearColor TeamColor =
			PS->Team == ETeamSide::Red ? FLinearColor(1.f, 0.28f, 0.28f) : FLinearColor(0.32f, 0.62f, 1.f);
		DrawText(FString::Printf(TEXT("%s   HP %.0f / %.0f   Armor %.0f   K/D %d/%d"),
					 PS->Team == ETeamSide::Red ? TEXT("RED") : TEXT("BLUE"), PS->Health->GetHealth(),
					 PS->Health->GetMaxHealth(), PS->Health->GetArmor(), PS->KillCount, PS->DeathCount),
			TeamColor, Margin + 14.f, BottomBoxY + 10.f, nullptr, 1.18f);
		if (PS->Health->IsDead())
		{
			DrawText(TEXT("Eliminated - respawning..."), FLinearColor::Red, CX - 115.f, CY + 40.f);
		}
	}

	if (Character && Character->GetWeapon())
	{
		DrawText(FString::Printf(TEXT("AMMO  %d / %d%s"), Character->GetWeapon()->Ammo->GetMagazineAmmo(),
					 Character->GetWeapon()->Ammo->GetReserveAmmo(),
					 Character->GetCombatState() == ECombatState::Reloading ? TEXT("   RELOADING") : TEXT("")),
			FLinearColor::White, Margin + 14.f, BottomBoxY + 38.f, nullptr, 1.18f);
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
		DrawText(FString::Printf(
					 TEXT("RED %d  :  %d BLUE   /   %d to win"), GS->RedTeamScore, GS->BlueTeamScore, GS->ScoreToWin),
			FLinearColor::White, ScoreTextX, Margin + 8.f, nullptr, 1.15f);
		if (GS->bMatchEnded)
		{
			DrawText(GS->WinningTeam == ETeamSide::Red ? TEXT("RED TEAM WINS") : TEXT("BLUE TEAM WINS"),
				FLinearColor::Yellow, CX - 130.f, CY - 70.f, nullptr, 1.5f);
		}
		float Y = 66.f;
		for (APlayerState* Player : GS->PlayerArray)
		{
			if (const ACombatPlayerState* Other = Cast<ACombatPlayerState>(Player))
			{
				DrawText(FString::Printf(TEXT("%s  %s   HP %.0f  K/D %d/%d%s"),
							 Other->Team == ETeamSide::Red ? TEXT("R") : TEXT("B"), *Other->GetPlayerName(),
							 Other->Health->GetHealth(), Other->KillCount, Other->DeathCount,
							 Other->Health->IsDead() ? TEXT(" DEAD") : TEXT("")),
					FLinearColor::White, Margin + 12.f, Y, nullptr, 0.95f);
				Y += 20.f;
			}
		}
	}

	DrawText(TEXT("WASD Move | Mouse Aim | LMB Fire | RMB ADS | R Reload | Space Jump"),
		FLinearColor(0.72f, 0.72f, 0.72f), Margin + 14.f, BottomBoxY + 68.f, nullptr, 0.88f);
}

void ACombatHUD::DrawRightTelemetryPanel(const ACombatPlayerController* PC, const ACombatGameState* GS,
	const UCombatTelemetryComponent* Telemetry, float PanelX, float PanelW)
{
	const float TopMargin = 14.f;
	const float Gap = 6.f;
	const float AvailableH = FMath::Max(420.f, Canvas->ClipY - TopMargin * 2.f);

	const float StatusH = 104.f;
	const float SummaryH = 100.f;
	const float GraphsTotalH = FMath::Clamp(AvailableH - StatusH - SummaryH - Gap * 3.f, 190.f, 340.f);
	const float NetGraphH = GraphsTotalH * 0.48f;
	const float ShotChartH = GraphsTotalH - NetGraphH;

	const FLagCompensationSettings LagSettings = GS ? GS->LagCompensation : FLagCompensationSettings();
	const FCombatTelemetrySummary Summary = Telemetry ? Telemetry->GetSummary() : FCombatTelemetrySummary();
	const UCombatTimeSyncComponent* TimeSync = PC ? PC->TimeSync.Get() : nullptr;
	const bool bSynced = TimeSync && TimeSync->IsReady();
	const double EstServerTime = TimeSync ? TimeSync->GetServerTime() : GetWorld()->GetTimeSeconds();

	// 1. 서버 & 네트워크 상태 수치 박스
	float CurY = TopMargin;
	DrawRect(FLinearColor(0.02f, 0.04f, 0.08f, 0.74f), PanelX, CurY, PanelW, StatusH);
	DrawLine(PanelX, CurY, PanelX + PanelW, CurY, FLinearColor(0.2f, 0.65f, 0.95f, 0.9f), 1.5f);

	DrawText(TEXT("SERVER & LAG COMPENSATION"), FLinearColor(0.55f, 0.85f, 1.f), PanelX + 10.f, CurY + 6.f, nullptr,
		0.95f);
	const FLinearColor BadgeColor =
		LagSettings.bEnabled ? FLinearColor(0.15f, 0.9f, 0.35f) : FLinearColor(1.f, 0.55f, 0.15f);
	DrawText(LagSettings.bEnabled ? TEXT("[REWIND ON]") : TEXT("[REWIND OFF]"), BadgeColor, PanelX + PanelW - 98.f,
		CurY + 6.f, nullptr, 0.92f);

	DrawText(FString::Printf(TEXT("Mode: %s  |  Clock: %s (%.1fs)"), GetNetModeLabel(),
				 bSynced ? TEXT("SYNCED") : TEXT("SYNCING"), EstServerTime),
		FLinearColor::White, PanelX + 10.f, CurY + 28.f, nullptr, 0.88f);

	DrawText(FString::Printf(TEXT("RTT: %.1f ms (Peak %.1f)  |  Ping: %.0f ms"), Summary.CurrentRttMs,
				 Summary.PeakRttMs, Summary.CurrentPingMs),
		FLinearColor(0.35f, 0.92f, 1.f), PanelX + 10.f, CurY + 47.f, nullptr, 0.88f);

	DrawText(FString::Printf(TEXT("FPS: %.1f (%.2f ms)"), Summary.CurrentFps, Summary.CurrentFrameMs),
		FLinearColor(1.f, 0.9f, 0.35f), PanelX + 10.f, CurY + 66.f, nullptr, 0.88f);

	DrawText(FString::Printf(TEXT("MaxRewind %.0fms | ViewCap %.0fms | Smooth %.0fms"),
				 LagSettings.MaxRewindSeconds * 1000.f, LagSettings.MaxTargetViewDelay * 1000.f,
				 LagSettings.DisplayDelaySeconds * 1000.f),
		FLinearColor(0.75f, 0.78f, 0.82f), PanelX + 10.f, CurY + 84.f, nullptr, 0.82f);

	// 2. 실시간 RTT & Frame Time 시계열 선 그래프
	CurY += StatusH + Gap;
	DrawNetGraphBox(Telemetry, PanelX, CurY, PanelW, NetGraphH);

	// 3. 최근 발사별 응답 지연 & 리와인드 막대 차트
	CurY += NetGraphH + Gap;
	DrawShotChartBox(Telemetry, LagSettings.MaxRewindSeconds * 1000.f, PanelX, CurY, PanelW, ShotChartH);

	// 4. 누적 사격 통계 & 마지막 발사 상세 수치
	CurY += ShotChartH + Gap;
	DrawRect(FLinearColor(0.02f, 0.04f, 0.08f, 0.74f), PanelX, CurY, PanelW, SummaryH);
	DrawLine(PanelX, CurY, PanelX + PanelW, CurY, FLinearColor(0.2f, 0.85f, 0.55f, 0.85f), 1.2f);

	const float HitRate =
		Summary.TotalAccepted > 0 ? (static_cast<float>(Summary.TotalHits) / Summary.TotalAccepted) * 100.f : 0.f;
	DrawText(TEXT("COMBAT SHOT TELEMETRY"), FLinearColor(0.45f, 0.95f, 0.65f), PanelX + 10.f, CurY + 6.f, nullptr,
		0.95f);
	DrawText(FString::Printf(TEXT("Shots: %d  |  Hits: %d (%.1f%%)  |  Reject: %d"), Summary.TotalSubmitted,
				 Summary.TotalHits, HitRate, Summary.TotalRejected),
		FLinearColor::White, PanelX + 10.f, CurY + 27.f, nullptr, 0.88f);
	DrawText(FString::Printf(TEXT("Avg Ack: %.1f ms  |  Avg Age S/T: %.1f / %.1f ms"), Summary.AverageAckMs,
				 Summary.AverageShotAgeMs, Summary.AverageTargetAgeMs),
		FLinearColor(0.82f, 0.86f, 0.90f), PanelX + 10.f, CurY + 46.f, nullptr, 0.85f);

	FCombatShotTelemetry LastShot;
	if (Telemetry && Telemetry->GetLastShot(LastShot))
	{
		const bool bRejected = LastShot.Rejection != EShotRejection::None;
		const FLinearColor ResultColor = bRejected ? FLinearColor(1.f, 0.3f, 0.3f)
			: LastShot.bDamagedPlayer			   ? FLinearColor(0.25f, 0.95f, 0.45f)
												   : FLinearColor(0.8f, 0.8f, 0.8f);
		const FString ResultLabel = bRejected
			? FString::Printf(TEXT("REJECT (%s)"), UCombatTelemetryComponent::RejectionToString(LastShot.Rejection))
			: (LastShot.bDamagedPlayer ? TEXT("HIT") : TEXT("MISS"));
		DrawText(FString::Printf(TEXT("Last #%u: %s [%s]"), LastShot.ShotId, *ResultLabel,
					 LastShot.bUsedRewind ? TEXT("Rewind") : TEXT("CurrWorld")),
			ResultColor, PanelX + 10.f, CurY + 65.f, nullptr, 0.88f);
		DrawText(FString::Printf(TEXT("Ack %.1fms | ShotAge %.1fms | TgtAge %.1fms"), LastShot.AckMs,
					 LastShot.ShotAgeMs, LastShot.TargetAgeMs),
			FLinearColor(0.75f, 0.88f, 1.f), PanelX + 10.f, CurY + 82.f, nullptr, 0.82f);
	}
	else
	{
		DrawText(TEXT("Last Shot: Fire weapon (LMB) to record live server metrics"),
			FLinearColor(0.6f, 0.65f, 0.7f), PanelX + 10.f, CurY + 68.f, nullptr, 0.84f);
	}
}

void ACombatHUD::DrawNetGraphBox(
	const UCombatTelemetryComponent* Telemetry, float X, float Y, float W, float H)
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

	DrawText(FString::Printf(TEXT("RTT (Cyan) & FRAME (Yellow) [0..%.0f ms]"), MaxScaleMs),
		FLinearColor(0.75f, 0.9f, 1.f), X + 10.f, Y + 5.f, nullptr, 0.86f);

	const float PlotX = X + 10.f;
	const float PlotY = Y + 24.f;
	const float PlotW = W - 20.f;
	const float PlotH = FMath::Max(36.f, H - 32.f);
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
		DrawText(TEXT("Collecting RTT/Frame samples..."), FLinearColor(0.55f, 0.6f, 0.65f), PlotX + 8.f,
			PlotY + PlotH * 0.4f, nullptr, 0.82f);
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

	DrawText(FString::Printf(TEXT("SHOT ACK & REWIND [0..%.0f ms]"), MaxScaleMs),
		FLinearColor(0.65f, 0.98f, 0.78f), X + 10.f, Y + 5.f, nullptr, 0.86f);
	DrawText(TEXT("Grn:Hit Gry:Miss Red:Rej Cyn:Rewind"), FLinearColor(0.68f, 0.74f, 0.8f), X + 10.f, Y + 21.f,
		nullptr, 0.76f);

	const float PlotX = X + 10.f;
	const float PlotY = Y + 37.f;
	const float PlotW = W - 20.f;
	const float PlotH = FMath::Max(34.f, H - 45.f);
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
		DrawText(TEXT("No shots fired yet"), FLinearColor(0.55f, 0.6f, 0.65f), PlotX + 8.f, PlotY + PlotH * 0.35f,
			nullptr, 0.82f);
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
		return TEXT("Unknown");
	}
	switch (GetWorld()->GetNetMode())
	{
		case NM_Client:
			return TEXT("Client");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_DedicatedServer:
			return TEXT("DedicatedSrv");
		case NM_Standalone:
		default:
			return TEXT("Standalone");
	}
}
