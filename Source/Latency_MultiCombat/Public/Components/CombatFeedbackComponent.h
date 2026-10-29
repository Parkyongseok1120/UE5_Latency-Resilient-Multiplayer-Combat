#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatFeedbackComponent.generated.h"

class ACombatCharacter;
class UCameraComponent;
class UCameraShakeBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Local camera presentation only. Reads replicated vitals; never writes gameplay state.
UCLASS(ClassGroup = (Combat))
class LATENCY_MULTICOMBAT_API UCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatFeedbackComponent();
	void PlayFireShake();
	void UpdateFeedback(float DeltaSeconds);
	float GetHitFlash() const { return HitFlash; }
	float GetLowHealthIntensity() const { return LowHealthIntensity; }
	bool HasActiveFireShake() const;
	bool HasDamageMaterial() const { return DamageMaterial != nullptr; }

	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Camera", meta = (ClampMin = "0", ClampMax = "3"))
	float FireShakeScale = 1.f;
	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Camera", meta = (ClampMin = "0", ClampMax = "1"))
	float ADSShakeMultiplier = 0.55f;
	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Damage", meta = (ClampMin = "0.1"))
	float HitFadeSpeed = 2.6f;
	UPROPERTY(EditDefaultsOnly, Category = "Feedback|Damage", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxGlitchIntensity = 0.85f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	void DetachCamera();
	void StopFireShake();
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DamageMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DamageInstance;
	TWeakObjectPtr<ACombatCharacter> ObservedPawn;
	TWeakObjectPtr<UCameraComponent> BoundCamera;
	TWeakObjectPtr<UCameraShakeBase> ActiveFireShake;
	float LastHealth = 0.f;
	float LastArmor = 0.f;
	float HitFlash = 0.f;
	float LowHealthIntensity = 0.f;
	bool bHasVitals = false;
};
