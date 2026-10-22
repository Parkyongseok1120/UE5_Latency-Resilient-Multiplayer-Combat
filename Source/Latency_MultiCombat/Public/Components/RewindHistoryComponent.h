#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "RewindHistoryComponent.generated.h"

struct FRewindFrame
{
	double Time = 0.0;
	FVector Location = FVector::ZeroVector;
	FVector ViewLocation = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FGuid LifeId;
	ETeamSide Team = ETeamSide::Neutral;
	float Radius = 0.f;
	float HalfHeight = 0.f;
	bool bAlive = false;
};

// 이력은 정해둔 개수만 보관하고, 리스폰이나 사망 경계는 보간하지 않음.
struct LATENCY_MULTICOMBAT_API FRewindHistoryBuffer
{
	void Reset(int32 Capacity);
	void Add(const FRewindFrame& Frame);
	bool Sample(double Time, FRewindFrame& OutFrame, float MaxInterpolationDistance) const;
	int32 Num() const { return Count; }

private:
	const FRewindFrame& At(int32 Index) const;
	TArray<FRewindFrame> Frames;
	int32 Head = 0;
	int32 Count = 0;
};

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class LATENCY_MULTICOMBAT_API URewindHistoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URewindHistoryComponent();
	void ResetHistory();
	void RecordNow();
	bool Sample(double Time, FRewindFrame& OutFrame);
	bool Capture(FRewindFrame& OutFrame) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	FRewindHistoryBuffer History;
	double LastRecordTime = -1.0;
};
