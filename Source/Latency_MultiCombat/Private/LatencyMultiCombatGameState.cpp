// Latency_MultiCombat - Game State Implementation
#include "Net/UnrealNetwork.h"
#include "LatencyMultiCombatGameState.h"
#include "LatencyMultiCombatPlayerState.h"

ALatencyMultiCombatGameState::ALatencyMultiCombatGameState()
	: RedTeamScore(0)
	, BlueTeamScore(0)
	, bMatchEnded(false)
	, WinningTeam(ETeamSide::Neutral)
{
}

void ALatencyMultiCombatGameState::BeginPlay()
{
	Super::BeginPlay();
}

void ALatencyMultiCombatGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALatencyMultiCombatGameState, RedTeamScore);
	DOREPLIFETIME(ALatencyMultiCombatGameState, BlueTeamScore);
	DOREPLIFETIME(ALatencyMultiCombatGameState, bMatchEnded);
	DOREPLIFETIME(ALatencyMultiCombatGameState, WinningTeam);
}

void ALatencyMultiCombatGameState::OnRep_TeamScores()
{
	// Update scoreboard / HUD
}

void ALatencyMultiCombatGameState::AddPlayerToTeam(ALatencyMultiCombatPlayerState* PlayerState, ETeamSide Team)
{
	if (!PlayerState) return;

	PlayerState->SetTeam(Team);
	TeamMembers.FindOrAdd(Team).AddUnique(PlayerState);
}

int32 ALatencyMultiCombatGameState::GetTeamMemberCount(ETeamSide Team) const
{
	const TArray<ALatencyMultiCombatPlayerState*>* Members = TeamMembers.Find(Team);
	return Members ? Members->Num() : 0;
}

TArray<ALatencyMultiCombatPlayerState*> ALatencyMultiCombatGameState::GetTeamMembers(ETeamSide Team) const
{
	const TArray<ALatencyMultiCombatPlayerState*>* Members = TeamMembers.Find(Team);
	return Members ? *Members : TArray<ALatencyMultiCombatPlayerState*>();
}

// AddKillToTeam and CheckWinCondition are FORCEINLINE in header

void ALatencyMultiCombatGameState::EndMatch(ETeamSide Winner)
{
	bMatchEnded = true;
	WinningTeam = Winner;
}
