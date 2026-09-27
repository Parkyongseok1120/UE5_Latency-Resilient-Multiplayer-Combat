// Latency_MultiCombat - Game Mode (4v4 Deathmatch)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/CombatTypes.h"
#include "LatencyMultiCombatGameMode.generated.h"

class ALatencyMultiCombatGameState;
class ALatencyMultiCombatPlayerState;
class ACharacter;

UCLASS()
class LATENCY_MULTICOMBAT_API ALatencyMultiCombatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALatencyMultiCombatGameMode();

	// --- Config (set in Blueprint subclass or here) ---
	UPROPERTY(EditDefaultsOnly, Category = "Deathmatch")
	int32 TeamSize = 4; // 4v4

	UPROPERTY(EditDefaultsOnly, Category = "Deathmatch")
	float RespawnDelay = 3.0f;

	// Override class defaults (PlayerStateClass & GameStateClass inherited from AGameModeBase)
	UPROPERTY(EditDefaultsOnly, Category = "Class")
	TSubclassOf<ACharacter> DefaultCharacterClass;

	// --- Game Flow ---
	virtual void BeginPlay() override;
	AActor* ChoosePlayerStart_Implementation(AController* Player);

	// Called when a player dies (from Character)
	void HandlePlayerDeath(ALatencyMultiCombatPlayerState* DeadPlayer, ALatencyMultiCombatPlayerState* Killer);

	// Respawn logic
	void RespawnPlayer(AController* Controller);

protected:
	// Assign team based on current member counts (balanced 4v4)
	ETeamSide AssignTeam();

	ALatencyMultiCombatGameState* GetDMGameState() const;

	FTimerHandle RespawnTimerHandle;
};
