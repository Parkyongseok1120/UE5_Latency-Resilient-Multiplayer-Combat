// Player State (4v4 Deathmatch)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Data/CombatTypes.h"
#include "CombatPlayerState.generated.h"

class UHealthComponent;

UCLASS()
class LATENCY_MULTICOMBAT_API ACombatPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ACombatPlayerState();

	// Team
	UPROPERTY(Replicated, BlueprintReadOnly)
	ETeamSide Team;

	// Score
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 KillCount;

	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 DeathCount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UHealthComponent> Health;
	// Server-authoritative team and score updates.
	FORCEINLINE void AddKill()
	{
		if (HasAuthority())
		{
			KillCount++;
			ForceNetUpdate();
		}
	}
	FORCEINLINE void SetTeam(ETeamSide NewTeam)
	{
		if (HasAuthority())
		{
			Team = NewTeam;
			ForceNetUpdate();
		}
	}

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void HandleDeath();
};
