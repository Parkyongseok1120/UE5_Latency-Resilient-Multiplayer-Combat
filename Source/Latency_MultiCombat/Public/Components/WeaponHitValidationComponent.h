#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatShotCommand.h"
#include "WeaponHitValidationComponent.generated.h"

struct FRewindFrame;

UCLASS()
class LATENCY_MULTICOMBAT_API UWeaponHitValidationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	// 실제 캐릭터는 안 움직이고, 과거 캡슐에 맞았는지만 계산함.
	static bool IntersectCapsule(
		const FVector& Start, const FVector& Direction, double Range, const FRewindFrame& Frame, double& OutDistance);
	EShotRejection Resolve(const FCombatShotCommand& Command, FHitResult& OutHit, FVector& OutStart, FVector& OutEnd,
		FVector& OutDirection, FCombatShotResponse& Response);
	float ApplyValidatedDamage(const FHitResult& Hit, const FVector& Direction) const;

private:
	TWeakObjectPtr<AActor> ValidatedTarget;
	FGuid ValidatedLife;
	uint32 SpreadSequence = 0;
};
