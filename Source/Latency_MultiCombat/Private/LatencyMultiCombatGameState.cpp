// Latency_MultiCombat - Game State Implementation
#include "LatencyMultiCombatGameState.h"
#include "Net/UnrealNetwork.h"
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
	if (!HasAuthority() || !PlayerState) return;

	PlayerState->SetTeam(Team);
}

int32 ALatencyMultiCombatGameState::GetTeamMemberCount(ETeamSide Team) const
{
	return GetTeamMembers(Team).Num();
}

TArray<ALatencyMultiCombatPlayerState*> ALatencyMultiCombatGameState::GetTeamMembers(ETeamSide Team) const
{
	TArray<ALatencyMultiCombatPlayerState*> Members;
	// PlayerArray is maintained by the engine on both server and clients, including disconnects.
	for (APlayerState* Player : PlayerArray)
		if (auto* PS = Cast<ALatencyMultiCombatPlayerState>(Player))
			if (IsValid(PS) && !PS->IsInactive() && PS->Team == Team) Members.Add(PS);
	return Members;
}

// AddKillToTeam and CheckWinCondition are FORCEINLINE in header

void ALatencyMultiCombatGameState::EndMatch(ETeamSide Winner)
{
	if (!HasAuthority() || bMatchEnded) return;
	bMatchEnded = true;
	WinningTeam = Winner;
	ForceNetUpdate();
}
