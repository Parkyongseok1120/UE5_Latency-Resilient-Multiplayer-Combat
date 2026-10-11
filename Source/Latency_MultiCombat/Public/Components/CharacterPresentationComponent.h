#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "CharacterPresentationComponent.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class LATENCY_MULTICOMBAT_API UCharacterPresentationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCharacterPresentationComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	UPROPERTY()
	TObjectPtr<UStaticMesh> DefaultBodyMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultBodyMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
	ETeamSide DisplayedTeam = ETeamSide::Neutral;
	bool bDisplayedDead = false;
	bool bHasDisplayedTeam = false;
};
