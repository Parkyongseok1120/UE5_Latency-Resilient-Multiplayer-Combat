// Latency_MultiCombat - Game Mode Implementation
#include "LatencyMultiCombatGameMode.h"
#include "LatencyMultiCombatGameState.h"
#include "LatencyMultiCombatPlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"

ALatencyMultiCombatGameMode::ALatencyMultiCombatGameMode()
	: TeamSize(4)
	, RespawnDelay(3.0f)
{
	PlayerStateClass = ALatencyMultiCombatPlayerState::StaticClass();
	GameStateClass = ALatencyMultiCombatGameState::StaticClass();
}

void ALatencyMultiCombatGameMode::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Display, TEXT("[DM] 4v4 Deathmatch started. TeamSize=%d"), TeamSize);
}

AActor* ALatencyMultiCombatGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Use default player start selection for now
	// Later: pick nearest spawn on player's team side
	return Super::ChoosePlayerStart_Implementation(Player);
}

ETeamSide ALatencyMultiCombatGameMode::AssignTeam()
{
	ALatencyMultiCombatGameState* GS = GetDMGameState();
	if (!GS) return ETeamSide::Red;

	int32 RedCount = GS->GetTeamMemberCount(ETeamSide::Red);
	int32 BlueCount = GS->GetTeamMemberCount(ETeamSide::Blue);

	// Balance teams: assign to smaller team, cap at TeamSize
	if (RedCount < BlueCount && RedCount < TeamSize)
		return ETeamSide::Red;
	else if (BlueCount < RedCount && BlueCount < TeamSize)
		return ETeamSide::Blue;
	else if (RedCount < TeamSize)
		return ETeamSide::Red;
	else
		return ETeamSide::Blue; // fallback
}

void ALatencyMultiCombatGameMode::HandlePlayerDeath(ALatencyMultiCombatPlayerState* DeadPlayer, ALatencyMultiCombatPlayerState* Killer)
{
	if (!DeadPlayer) return;

	// Award kill to killer
	if (Killer && Killer != DeadPlayer)
	{
		Killer->AddKill();

		ALatencyMultiCombatGameState* GS = GetDMGameState();
		if (GS)
		{
			GS->AddKillToTeam(Killer->Team);

			// Check win condition
			if (GS->CheckWinCondition())
			{
				ETeamSide Winner = (GS->RedTeamScore >= GS->BlueTeamScore) ? ETeamSide::Red : ETeamSide::Blue;
				GS->EndMatch(Winner);
				UE_LOG(LogTemp, Display, TEXT("[DM] Match ended! Red=%d Blue=%d"), GS->RedTeamScore, GS->BlueTeamScore);
			}
		}
	}

	// Schedule respawn
	AController* Controller = Cast<AController>(DeadPlayer->GetOwner());
	if (Controller)
	{
		AController* Captured = Controller;
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, [this, Captured]() { RespawnPlayer(Captured); }, RespawnDelay, false);
	}
}

void ALatencyMultiCombatGameMode::RespawnPlayer(AController* Controller)
{
	if (!Controller) return;

	ALatencyMultiCombatPlayerState* PS = Cast<ALatencyMultiCombatPlayerState>(Controller->GetPlayerState<ALatencyMultiCombatPlayerState>());
	if (!PS || !PS->bIsDead) return;

	// Reset player state for respawn
	PS->ResetForRespawn();

	// Respawn character at a player start
	AActor* SpawnLocation = ChoosePlayerStart(Controller);
	if (SpawnLocation)
	{
		FVector SpawnLoc = SpawnLocation->GetActorLocation();
		FRotator SpawnRot = SpawnLocation->GetActorRotation();

		Controller->UnPossess();
		ACharacter* NewChar = GetWorld()->SpawnActor<ACharacter>(DefaultCharacterClass, SpawnLoc, SpawnRot);
		if (NewChar)
		{
			Controller->Possess(NewChar);
		}
	}
}

ALatencyMultiCombatGameState* ALatencyMultiCombatGameMode::GetDMGameState() const
{
	return Cast<ALatencyMultiCombatGameState>(GameState);
}
