#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
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
	UPROPERTY(EditDefaultsOnly, Category = "Deathmatch", meta = (ClampMin = "1"))
	int32 TeamSize = 4;
	UPROPERTY(EditDefaultsOnly, Category = "Deathmatch", meta = (ClampMin = "0.1"))
	float RespawnDelay = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deathmatch")
	bool bAllowFriendlyFire = false;
	UPROPERTY(EditDefaultsOnly, Category = "Class")
	TSubclassOf<ACharacter> DefaultCharacterClass;

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	bool CanDamagePlayer(const ALatencyMultiCombatPlayerState* Victim, AController* DamageInstigator) const;
	void HandlePlayerDeath(ALatencyMultiCombatPlayerState* DeadPlayer, ALatencyMultiCombatPlayerState* Killer);
	void RespawnPlayer(AController* Controller);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	ETeamSide AssignTeam();
	ALatencyMultiCombatGameState* GetDMGameState() const;

private:
	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;
};
