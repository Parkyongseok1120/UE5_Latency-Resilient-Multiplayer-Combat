#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/CombatTypes.h"
#include "CombatGameMode.generated.h"

class ACombatGameState;
class ACombatPlayerState;
class UTeamSpawnComponent;
class URespawnComponent;
class UCombatNetworkDataAsset;

UCLASS()
class LATENCY_MULTICOMBAT_API ACombatGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ACombatGameMode();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Network")
	TObjectPtr<UCombatNetworkDataAsset> NetworkData;
	UPROPERTY(EditDefaultsOnly, Category = "Deathmatch", meta = (ClampMin = "1"))
	int32 TeamSize = 4;
	UPROPERTY(EditDefaultsOnly, Category = "Deathmatch", meta = (ClampMin = "0.1"))
	float RespawnDelay = 3.f;
	UPROPERTY(EditDefaultsOnly, Category = "Deathmatch")
	bool bAllowFriendlyFire = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deathmatch")
	TObjectPtr<UTeamSpawnComponent> TeamSpawns;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deathmatch")
	TObjectPtr<URespawnComponent> Respawns;

	virtual void BeginPlay() override;
	virtual FString InitNewPlayer(APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	bool CanDamagePlayer(const ACombatPlayerState* Victim, AController* DamageInstigator) const;
	void HandlePlayerDeath(ACombatPlayerState* DeadPlayer, ACombatPlayerState* Killer);

protected:
	ACombatGameState* GetDMGameState() const;
};
