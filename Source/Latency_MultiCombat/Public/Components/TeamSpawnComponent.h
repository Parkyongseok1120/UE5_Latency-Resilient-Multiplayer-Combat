#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TeamSpawnComponent.generated.h"

class APlayerController;

UCLASS()
class LATENCY_MULTICOMBAT_API UTeamSpawnComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	void AssignPlayerTeam(APlayerController* Player);
	AActor* ChooseTeamStart(AController* Player) const;
};
