// Latency_MultiCombat - Game State (4v4 Deathmatch)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Data/CombatTypes.h"
#include "LatencyMultiCombatGameState.generated.h"

class ALatencyMultiCombatPlayerState;

UCLASS()
class LATENCY_MULTICOMBAT_API ALatencyMultiCombatGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALatencyMultiCombatGameState();

	// Team scores (total kills per team)
	UPROPERTY(ReplicatedUsing = OnRep_TeamScores, BlueprintReadOnly)
	int32 RedTeamScore;

	UPROPERTY(ReplicatedUsing = OnRep_TeamScores, BlueprintReadOnly)
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
	void AddPlayerToTeam(ALatencyMultiCombatPlayerState* PlayerState, ETeamSide Team);
	int32 GetTeamMemberCount(ETeamSide Team) const;
	TArray<ALatencyMultiCombatPlayerState*> GetTeamMembers(ETeamSide Team) const;

	// --- Score Management (FORCEINLINE: small, called per kill) ---
	FORCEINLINE void AddKillToTeam(ETeamSide Team) { if (!HasAuthority() || bMatchEnded) return; if (Team == ETeamSide::Red) RedTeamScore++; else if (Team == ETeamSide::Blue) BlueTeamScore++; ForceNetUpdate(); }
	FORCEINLINE bool CheckWinCondition() const { return bMatchEnded || RedTeamScore >= ScoreToWin || BlueTeamScore >= ScoreToWin; }
	void EndMatch(ETeamSide Winner);

	UFUNCTION()
	void OnRep_TeamScores();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

};
