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
	void ApplyAssets();
	bool UsesSkeletalBody() const { return bUsesSkeletalBody; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	void ApplyTeamVisuals(ETeamSide Team, bool bDead);
	UPROPERTY()
	TObjectPtr<UStaticMesh> DefaultBodyMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultBodyMaterial;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMaterials;
	FName ConfiguredColorParameter = TEXT("Paint Tint");
	ETeamSide DisplayedTeam = ETeamSide::Neutral;
	bool bDisplayedDead = false;
	bool bHasDisplayedTeam = false;
	bool bUsesSkeletalBody = false;
};
