#include "LatencyMultiCombatHUD.h"
#include "LatencyMultiCombatCharacter.h"
#include "LatencyMultiCombatPlayerState.h"
#include "LatencyMultiCombatGameState.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"

void ALatencyMultiCombatHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !PlayerOwner) return;
	const auto* PS = PlayerOwner->GetPlayerState<ALatencyMultiCombatPlayerState>();
	const auto* Character = Cast<ALatencyMultiCombatCharacter>(PlayerOwner->GetPawn());
	const auto* GS = GetWorld()->GetGameState<ALatencyMultiCombatGameState>();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 16.f, Canvas->ClipY - 106.f, Canvas->ClipX - 32.f, 100.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CX - 180.f, 16.f, 360.f, 35.f);
	if (GS) DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 16.f, 66.f, 440.f, GS->PlayerArray.Num() * 20.f + 14.f);
	if (PS)
	{
		const FLinearColor TeamColor = PS->Team == ETeamSide::Red ? FLinearColor(1.f, 0.25f, 0.25f) : FLinearColor(0.3f, 0.6f, 1.f);
		DrawText(FString::Printf(TEXT("%s   HP %.0f / %.0f   Armor %.0f   K/D %d/%d"),
			PS->Team == ETeamSide::Red ? TEXT("RED") : TEXT("BLUE"), PS->CurrentHealth, PS->MaxHealth,
			PS->CurrentArmor, PS->KillCount, PS->DeathCount), TeamColor, 30.f, Canvas->ClipY - 90.f, nullptr, 1.3f);
		if (PS->bIsDead) DrawText(TEXT("Eliminated - respawning..."), FLinearColor::Red, CX - 130.f, CY + 40.f);
	}
	if (Character)
	{
		DrawText(FString::Printf(TEXT("AMMO  %d / %d%s"), Character->MagazineAmmo, Character->ReserveAmmo,
			Character->CombatState == ECombatState::Reloading ? TEXT("   RELOADING") : TEXT("")),
			FLinearColor::White, 30.f, Canvas->ClipY - 60.f, nullptr, 1.3f);
		if (Character->CombatState != ECombatState::Dead)
		{
			DrawLine(CX - 12.f, CY, CX - 4.f, CY, FLinearColor::White, 1.5f);
			DrawLine(CX + 4.f, CY, CX + 12.f, CY, FLinearColor::White, 1.5f);
			DrawLine(CX, CY - 12.f, CX, CY - 4.f, FLinearColor::White, 1.5f);
			DrawLine(CX, CY + 4.f, CX, CY + 12.f, FLinearColor::White, 1.5f);
			if (GetWorld()->GetTimeSeconds() - Character->GetLastHitConfirmationTime() < 0.15f)
			{
				DrawLine(CX - 8.f, CY - 8.f, CX + 8.f, CY + 8.f, FLinearColor::Red, 2.f);
				DrawLine(CX - 8.f, CY + 8.f, CX + 8.f, CY - 8.f, FLinearColor::Red, 2.f);
			}
		}
	}
	if (GS)
	{
		DrawText(FString::Printf(TEXT("RED %d  :  %d BLUE   /   %d to win"), GS->RedTeamScore, GS->BlueTeamScore, GS->ScoreToWin),
			FLinearColor::White, CX - 160.f, 25.f, nullptr, 1.3f);
		if (GS->bMatchEnded)
			DrawText(GS->WinningTeam == ETeamSide::Red ? TEXT("RED TEAM WINS") : TEXT("BLUE TEAM WINS"),
				FLinearColor::Yellow, CX - 140.f, CY - 70.f, nullptr, 1.6f);
		float Y = 75.f;
		for (APlayerState* Player : GS->PlayerArray)
			if (const auto* Other = Cast<ALatencyMultiCombatPlayerState>(Player))
			{
				DrawText(FString::Printf(TEXT("%s  %s   HP %.0f  K/D %d/%d%s"),
					Other->Team == ETeamSide::Red ? TEXT("R") : TEXT("B"), *Other->GetPlayerName(),
					Other->CurrentHealth, Other->KillCount, Other->DeathCount, Other->bIsDead ? TEXT(" DEAD") : TEXT("")),
					FLinearColor::White, 30.f, Y);
				Y += 20.f;
			}
	}
	DrawText(TEXT("WASD move | Mouse aim | LMB fire | RMB ADS | R reload | Space jump"),
		FLinearColor(0.7f, 0.7f, 0.7f), 30.f, Canvas->ClipY - 30.f);
}
