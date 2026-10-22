#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatShotCommand.h"
#include "RewindBenchmarkComponent.generated.h"

class ACombatCharacter;
class UWeaponFireComponent;

// 테스트 옵션을 켰을 때만 실행함. 이동 경로와 발사 간격은 양쪽 조건이 같음.
UCLASS()
class LATENCY_MULTICOMBAT_API URewindBenchmarkComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URewindBenchmarkComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	UPROPERTY(Replicated)
	double Epoch = 0.0;
#if !UE_BUILD_SHIPPING
	void TickServer();
	void TickClient(float DeltaTime);
	void ShotResult(const FCombatShotResponse& Response);
	void Finish(bool bSuccess, const TCHAR* Reason);
	TWeakObjectPtr<ACombatCharacter> Target;
	TWeakObjectPtr<UWeaponFireComponent> BoundFire;
	FVector TargetAnchor = FVector::ZeroVector;
	double StartedAt = 0.0;
	double SubmittedAt = 0.0;
	uint32 PendingShotId = 0;
	int32 Samples = 0;
	int32 DesiredSamples = 24;
	int32 ExpectedHits = 0;
	int32 ActualHits = 0;
	int32 FalsePositive = 0;
	int32 FalseNegative = 0;
	int32 Rejected = 0;
	int32 FrameCount = 0;
	double FrameSeconds = 0.0;
	double GPUSeconds = 0.0;
	int32 GPUSamples = 0;
	float RTT = 0.f;
	float TargetSpeed = 0.f;
	FVector ObservedTarget = FVector::ZeroVector;
	bool bExpectedHit = false;
	bool bFinished = false;
	bool bPending = false;
	bool bSawMovement = false;
	bool bSawDamage = false;
	bool bCaptured = false;
#endif
};
