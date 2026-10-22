// Game State (4v4 Deathmatch)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Data/CombatTypes.h"
#include "Data/CombatNetworkDataAsset.h"
#include "CombatGameState.generated.h"

class ACombatPlayerState;
class URewindBenchmarkComponent;

UCLASS()
class LATENCY_MULTICOMBAT_API ACombatGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ACombatGameState();
	UPROPERTY(VisibleAnywhere, Category = "Testing")
	TObjectPtr<URewindBenchmarkComponent> RewindBenchmark;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Network")
	FLagCompensationSettings LagCompensation;

	// Team scores (total kills per team)
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 RedTeamScore;

	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 BlueTeamScore;

	// Win condition (SSOT: configured in editor)
	UPROPERTY(EditDefaultsOnly, Category = "Deathmatch")
	int32 ScoreToWin = 50;

	// Match state
	UPROPERTY(Replicated, BlueprintReadOnly)
	bool bMatchEnded;

	UPROPERTY(Replicated, BlueprintReadOnly)
	ETeamSide WinningTeam;

	// --- Team Management ---
	int32 GetTeamMemberCount(ETeamSide Team) const;
	TArray<ACombatPlayerState*> GetTeamMembers(ETeamSide Team) const;

	// Server-authoritative score updates.
	FORCEINLINE void AddKillToTeam(ETeamSide Team)
	{
		if (!HasAuthority() || bMatchEnded)
		{
			return;
		}
		if (Team == ETeamSide::Red)
		{
			RedTeamScore++;
		}
		else if (Team == ETeamSide::Blue)
		{
			BlueTeamScore++;
		}
		ForceNetUpdate();
	}
	FORCEINLINE bool CheckWinCondition() const
	{
		return bMatchEnded || RedTeamScore >= ScoreToWin || BlueTeamScore >= ScoreToWin;
	}
	void EndMatch(ETeamSide Winner);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
