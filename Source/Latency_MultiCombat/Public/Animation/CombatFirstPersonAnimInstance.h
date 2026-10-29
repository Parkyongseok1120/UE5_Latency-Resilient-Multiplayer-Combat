#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CombatFirstPersonAnimInstance.generated.h"

// Shares the evaluated body pose with owner-only arms, including fingers and reload motion.
UCLASS(Transient)
class LATENCY_MULTICOMBAT_API UCombatFirstPersonAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
