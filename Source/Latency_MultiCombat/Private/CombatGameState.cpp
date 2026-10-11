#include "CombatGameState.h"
#include "Net/UnrealNetwork.h"
#include "CombatPlayerState.h"

ACombatGameState::ACombatGameState()
	: RedTeamScore(0), BlueTeamScore(0), bMatchEnded(false), WinningTeam(ETeamSide::Neutral)
{
}

void ACombatGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACombatGameState, RedTeamScore);
	DOREPLIFETIME(ACombatGameState, BlueTeamScore);
	DOREPLIFETIME(ACombatGameState, bMatchEnded);
	DOREPLIFETIME(ACombatGameState, WinningTeam);
}

int32 ACombatGameState::GetTeamMemberCount(ETeamSide Team) const
{
	return GetTeamMembers(Team).Num();
}

TArray<ACombatPlayerState*> ACombatGameState::GetTeamMembers(ETeamSide Team) const
{
	TArray<ACombatPlayerState*> Members;
	// PlayerArray is maintained by the engine on both server and clients, including disconnects.
	for (APlayerState* Player : PlayerArray)
	{
		if (ACombatPlayerState* PS = Cast<ACombatPlayerState>(Player))
		{
			if (IsValid(PS) && !PS->IsInactive() && PS->Team == Team)
			{
				Members.Add(PS);
			}
		}
	}
	return Members;
}

void ACombatGameState::EndMatch(ETeamSide Winner)
{
	if (!HasAuthority() || bMatchEnded)
	{
		return;
	}
	bMatchEnded = true;
	WinningTeam = Winner;
	ForceNetUpdate();
}
