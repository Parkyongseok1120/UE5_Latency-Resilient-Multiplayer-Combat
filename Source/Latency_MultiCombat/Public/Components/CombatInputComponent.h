#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "CombatInputComponent.generated.h"

class UCombatInputDataAsset;
class UInputComponent;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class ACombatCharacter;
class ACombatWeapon;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class LATENCY_MULTICOMBAT_API UCombatInputComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCombatInputComponent();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UCombatInputDataAsset> InputData;
	void BindInput(UInputComponent* PlayerInputComponent);

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCombatInputDataAsset> ActiveInput;
	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
	void ReleaseBindings();
	ACombatCharacter* GetCharacter() const;
	ACombatWeapon* GetWeapon() const;
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void JumpStarted();
	void JumpEnded();
	void FireStarted();
	void FireEnded();
	void FireModeStarted();
	void ReloadStarted();
	void ADSStarted();
	void ADSEnded();
};
