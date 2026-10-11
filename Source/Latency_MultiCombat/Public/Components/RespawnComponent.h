#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "RespawnComponent.generated.h"

UCLASS()
class LATENCY_MULTICOMBAT_API URespawnComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	bool IsScheduled(AController* Controller) const;
	void Schedule(AController* Controller, float Delay);
	void Cancel(AController* Controller);
	void Respawn(AController* Controller);

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void HandleRespawnTimer(TWeakObjectPtr<AController> Controller);
	TMap<TWeakObjectPtr<AController>, FTimerHandle> Timers;
};
