#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatSmokeComponent.generated.h"

// Opt-in development automation; uses the same gameplay entry points as player input.
UCLASS()
class LATENCY_MULTICOMBAT_API UCombatSmokeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatSmokeComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
#if !UE_BUILD_SHIPPING
	void TickSmoke();
	bool bSmokeShooter = false;
	bool bSmokeSawDamage = false;
	bool bSmokeSawDeath = false;
	bool bSmokeFinished = false;
	float SmokeStartTime = 0.f;
	float SmokeLastHealth = -1.f;
#endif
};
