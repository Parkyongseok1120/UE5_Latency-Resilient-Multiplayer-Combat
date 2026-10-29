#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CombatAnimInstance.generated.h"

USTRUCT(BlueprintType)
struct FCombatAimBone
{
	GENERATED_BODY()

	FCombatAimBone() = default;
	FCombatAimBone(FName InBone, float InWeight) : Bone(InBone), Weight(InWeight) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim")
	FName Bone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aim", meta = (ClampMin = "0", ClampMax = "1"))
	float Weight = 0.f;
};

// Native parent of ABP_CombatRifle. Corrects its evaluated pose after locomotion, foot IK and fire slots.
UCLASS(Transient, Blueprintable)
class LATENCY_MULTICOMBAT_API UCombatAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UCombatAnimInstance();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Aim")
	bool bEnableAimIK = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Aim", meta = (ClampMin = "0", ClampMax = "89"))
	float MaxAimPitch = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Aim", meta = (ClampMin = "0"))
	float AimInterpolationSpeed = 12.f;

	// Actor-space barrel direction in MF_Rifle_Idle_ADS with the default HandGrip_R attachment.
	// Remove this authored bias before applying view pitch; tune when replacing the pose or weapon grip.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Aim")
	FRotator ReferenceAimRotation = FRotator(11.886f, 8.224f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|Aim")
	TArray<FCombatAimBone> AimBones;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|LeftHandIK")
	bool bEnableLeftHandIK = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|LeftHandIK")
	FName RightHandBone = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|LeftHandIK")
	FName LeftHandBone = TEXT("hand_l");

	// Character space: behind, left and below the upper arm, to keep the elbow outside the torso.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|LeftHandIK")
	FVector LeftElbowHintOffset = FVector(-10.f, -45.f, -20.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|LeftHandIK", meta = (ClampMin = "0"))
	float IKInterpolationSpeed = 16.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|Aim")
	float AimPitch = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|LeftHandIK")
	float LeftHandIKAlpha = 0.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Combat|LeftHandIK")
	FTransform LeftHandTargetInRightHandSpace = FTransform::Identity;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
