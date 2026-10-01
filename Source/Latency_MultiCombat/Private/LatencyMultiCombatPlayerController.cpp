#include "LatencyMultiCombatPlayerController.h"
#include "LatencyMultiCombatCharacter.h"
#include "LatencyMultiCombatPlayerState.h"
#include "LatencyMultiCombatGameState.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ALatencyMultiCombatPlayerController::ALatencyMultiCombatPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ALatencyMultiCombatPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	SetupEnhancedInput();
#if !UE_BUILD_SHIPPING
	bSmokeEnabled = FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineSmoke"));
	bSmokeShooter = FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineShooter"));
	SmokeStartTime = GetWorld()->GetTimeSeconds();
#endif
}

void ALatencyMultiCombatPlayerController::SetupEnhancedInput()
{
	if (!IsLocalController() || CombatMappingContext.IsNull()) return;
	if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		if (auto* Context = CombatMappingContext.LoadSynchronous()) Subsystem->AddMappingContext(Context, 0);
}

void ALatencyMultiCombatPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
#if !UE_BUILD_SHIPPING
	if (bSmokeEnabled && IsLocalController() && !bSmokeFinished) TickBaselineSmoke();
#endif
}

#if !UE_BUILD_SHIPPING
void ALatencyMultiCombatPlayerController::TickBaselineSmoke()
{
	if (GetWorld()->GetTimeSeconds() - SmokeStartTime > 60.f)
	{
		UE_LOG(LogTemp, Error, TEXT("[FPSBaselineSmoke] FAIL timeout Damage=%d Death=%d"), bSmokeSawDamage, bSmokeSawDeath);
		bSmokeFinished = true;
		FPlatformMisc::RequestExit(false);
		return;
	}
	auto* GS = GetWorld()->GetGameState<ALatencyMultiCombatGameState>();
	auto* MyPS = GetPlayerState<ALatencyMultiCombatPlayerState>();
	auto* CombatCharacter = Cast<ALatencyMultiCombatCharacter>(GetPawn());
	if (!GS || !MyPS || !CombatCharacter || GS->PlayerArray.Num() < 2 || MyPS->Team == ETeamSide::Neutral) return;

	ALatencyMultiCombatPlayerState* Victim = nullptr;
	ALatencyMultiCombatPlayerState* Shooter = nullptr;
	for (APlayerState* ListedPlayer : GS->PlayerArray)
	{
		auto* PS = Cast<ALatencyMultiCombatPlayerState>(ListedPlayer);
		if (!PS) continue;
		if (PS->Team == ETeamSide::Blue) Victim = PS;
		if (PS->Team == ETeamSide::Red) Shooter = PS;
	}
	if (!Victim || !Shooter || !Victim->GetPawn() || !Shooter->GetPawn()) return;
	if (SmokeLastHealth != Victim->CurrentHealth)
	{
		UE_LOG(LogTemp, Display, TEXT("[FPSBaselineSmoke] %s observes Blue HP=%.1f Armor=%.1f Deaths=%d RedKills=%d Score=%d:%d"),
			bSmokeShooter ? TEXT("Shooter") : TEXT("Observer"), Victim->CurrentHealth, Victim->CurrentArmor,
			Victim->DeathCount, Shooter->KillCount, GS->RedTeamScore, GS->BlueTeamScore);
		SmokeLastHealth = Victim->CurrentHealth;
	}
	if (Victim->CurrentHealth > 0.f && Victim->CurrentHealth < Victim->MaxHealth) bSmokeSawDamage = true;
	if (Victim->bIsDead && Victim->DeathCount == 1) bSmokeSawDeath = true;
	if (bSmokeSawDamage && bSmokeSawDeath && !Victim->bIsDead && Victim->CurrentHealth == Victim->MaxHealth
		&& Victim->DeathCount == 1 && Shooter->KillCount == 1 && GS->RedTeamScore == 1 && GS->BlueTeamScore == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("[FPSBaselineSmoke] PASS %s: replicated HP, death, score and respawn agree"),
			bSmokeShooter ? TEXT("Shooter") : TEXT("Observer"));
		bSmokeFinished = true;
		FPlatformMisc::RequestExit(false);
		return;
	}
	// First client is Red; the second is Blue. No test-only damage/teleport RPC is exposed.
	if (bSmokeShooter && MyPS == Shooter && !bSmokeSawDeath && Victim->DeathCount == 0
		&& GetWorld()->GetTimeSeconds() - SmokeStartTime > 5.f)
	{
		const FVector Target = Victim->GetPawn()->GetActorLocation() + FVector(0.f, 0.f, 30.f);
		SetControlRotation((Target - CombatCharacter->GetPawnViewLocation()).Rotation());
		CombatCharacter->FireWeapon();
	}
}
#endif
