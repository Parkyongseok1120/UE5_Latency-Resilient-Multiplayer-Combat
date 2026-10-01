// Latency_MultiCombat - Player State (4v4 Deathmatch)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Data/CombatTypes.h"
#include "LatencyMultiCombatPlayerState.generated.h"

UCLASS()
class LATENCY_MULTICOMBAT_API ALatencyMultiCombatPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ALatencyMultiCombatPlayerState();

	// Team
	UPROPERTY(ReplicatedUsing = OnRep_Team, BlueprintReadOnly)
	ETeamSide Team;

	// Score
	UPROPERTY(ReplicatedUsing = OnRep_KillCount, BlueprintReadOnly)
	int32 KillCount;

	UPROPERTY(ReplicatedUsing = OnRep_KillCount, BlueprintReadOnly)
	int32 DeathCount;

	// Combat state (replicated for HUD / other players)
	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly)
	float CurrentHealth;

	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly)
	float CurrentArmor;

	// SSOT: Set from UCharacterDataAsset at spawn. No hardcoded values elsewhere.
	UPROPERTY(Replicated, BlueprintReadOnly)
	float MaxHealth;

	// Respawn
	UPROPERTY(Replicated, BlueprintReadOnly)
	bool bIsDead;

	// --- Replication Handlers ---
	UFUNCTION()
	void OnRep_Team();

	UFUNCTION()
	void OnRep_KillCount();

	UFUNCTION()
	void OnRep_Health();

	// --- Game Logic (FORCEINLINE: small, hot-path) ---
	FORCEINLINE void AddKill() { if (HasAuthority()) { KillCount++; ForceNetUpdate(); } }
	FORCEINLINE void AddDeath() { if (HasAuthority() && !bIsDead) { DeathCount++; bIsDead = true; ForceNetUpdate(); } }
	FORCEINLINE void SetTeam(ETeamSide NewTeam) { if (HasAuthority()) { Team = NewTeam; ForceNetUpdate(); } }
	void InitializeForSpawn(const FCharacterStats& Stats);
	void ApplyDamage(float Amount, AController* Damager);
	void Heal(float Amount);
	void ResetForRespawn();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	float LastDamageTime;
};
