#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatShotCommand.h"
#include "CombatTelemetryComponent.generated.h"

class UWeaponFireComponent;

struct FCombatNetSample
{
	float TimeSeconds = 0.f;
	float RttMs = 0.f;
	float PingMs = 0.f;
	float FrameMs = 0.f;
};

struct FCombatShotTelemetry
{
	uint32 ShotId = 0;
	float AckMs = 0.f;
	float ShotAgeMs = 0.f;
	float TargetAgeMs = 0.f;
	EShotRejection Rejection = EShotRejection::None;
	bool bUsedRewind = false;
	bool bDamagedPlayer = false;
};

struct FCombatTelemetrySummary
{
	int32 TotalSubmitted = 0;
	int32 TotalAccepted = 0;
	int32 TotalRejected = 0;
	int32 TotalHits = 0;
	float AverageAckMs = 0.f;
	float AverageShotAgeMs = 0.f;
	float AverageTargetAgeMs = 0.f;
	float CurrentRttMs = 0.f;
	float PeakRttMs = 0.f;
	float CurrentPingMs = 0.f;
	float CurrentFrameMs = 0.f;
	float CurrentFps = 0.f;
};

// 로컬 컨트롤러 전용 계측 수집기. Pawn/무기가 리스폰으로 바뀌어도 이력을 유지하고 델리게이트만 재연결함.
UCLASS(ClassGroup = (Combat))
class LATENCY_MULTICOMBAT_API UCombatTelemetryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxNetSamples = 120;
	static constexpr int32 MaxShotSamples = 20;
	static constexpr int32 MaxPendingShots = 32;
	static constexpr float NetSampleInterval = 0.05f;

	UCombatTelemetryComponent();

	void RefreshWeaponBinding();
	void SampleNetworkMetrics(float DeltaTime, bool bForceSample = false);

	int32 GetNetSampleCount() const { return NetCount; }
	const FCombatNetSample& GetNetSampleAt(int32 ChronologicalIndex) const;

	int32 GetShotSampleCount() const { return ShotCount; }
	const FCombatShotTelemetry& GetShotSampleAt(int32 ChronologicalIndex) const;
	bool GetLastShot(FCombatShotTelemetry& OutShot) const;

	FCombatTelemetrySummary GetSummary() const;
	static const TCHAR* RejectionToString(EShotRejection Rejection);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	struct FPendingShotEntry
	{
		uint32 ShotId = 0;
		double SubmittedPlatformSeconds = 0.0;
	};

	void BindFireComponent(UWeaponFireComponent* NewFire);
	void HandleShotSubmitted(uint32 ShotId);
	void HandleShotResult(const FCombatShotResponse& Response);
	void PushNetSample(const FCombatNetSample& Sample);
	void PushShotSample(const FCombatShotTelemetry& Sample);

	TWeakObjectPtr<UWeaponFireComponent> BoundFire;
	TArray<FCombatNetSample> NetRing;
	int32 NetHead = 0;
	int32 NetCount = 0;

	TArray<FCombatShotTelemetry> ShotRing;
	int32 ShotHead = 0;
	int32 ShotCount = 0;

	TArray<FPendingShotEntry> PendingShots;

	double LastNetSampleWorldTime = -100.0;
	int32 TotalSubmitted = 0;
	int32 TotalAccepted = 0;
	int32 TotalRejected = 0;
	int32 TotalHits = 0;
	double AccumulatedAckMs = 0.0;
	double AccumulatedShotAgeMs = 0.0;
	double AccumulatedTargetAgeMs = 0.0;
	int32 TotalResultCount = 0;
	float CurrentRttMs = 0.f;
	float PeakRttMs = 0.f;
	float CurrentPingMs = 0.f;
	float SmoothedFrameMs = 16.67f;
};
