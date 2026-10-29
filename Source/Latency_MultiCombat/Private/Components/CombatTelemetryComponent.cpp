#include "Components/CombatTelemetryComponent.h"
#include "CombatCharacter.h"
#include "CombatGameState.h"
#include "CombatPlayerController.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Components/CombatTimeSyncComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Engine/World.h"

UCombatTelemetryComponent::UCombatTelemetryComponent()
{
	SetIsReplicatedByDefault(false);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	NetRing.SetNum(MaxNetSamples);
	ShotRing.SetNum(MaxShotSamples);
}

void UCombatTelemetryComponent::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC && PC->IsLocalController() && GetNetMode() != NM_DedicatedServer)
	{
		SetComponentTickEnabled(true);
		RefreshWeaponBinding();
	}
}

void UCombatTelemetryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	BindFireComponent(nullptr);
	Super::EndPlay(Reason);
}

void UCombatTelemetryComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	RefreshWeaponBinding();
	SampleNetworkMetrics(DeltaTime, false);
}

void UCombatTelemetryComponent::RefreshWeaponBinding()
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const ACombatCharacter* Character = PC ? Cast<ACombatCharacter>(PC->GetPawn()) : nullptr;
	const ACombatWeapon* Weapon = Character ? Character->GetWeapon() : nullptr;
	UWeaponFireComponent* DesiredFire = Weapon ? Weapon->Fire.Get() : nullptr;
	if (BoundFire.Get() != DesiredFire)
	{
		BindFireComponent(DesiredFire);
	}
}

void UCombatTelemetryComponent::BindFireComponent(UWeaponFireComponent* NewFire)
{
	if (UWeaponFireComponent* Previous = BoundFire.Get())
	{
		Previous->OnShotSubmitted.RemoveAll(this);
		Previous->OnShotResult.RemoveAll(this);
	}
	// 새 무기는 ShotId를 1번부터 다시 발급하므로 이전 무기의 미응답 타임스탬프는 비움.
	PendingShots.Reset();
	BoundFire = NewFire;
	if (NewFire)
	{
		NewFire->OnShotSubmitted.AddUObject(this, &UCombatTelemetryComponent::HandleShotSubmitted);
		NewFire->OnShotResult.AddUObject(this, &UCombatTelemetryComponent::HandleShotResult);
	}
}

void UCombatTelemetryComponent::SampleNetworkMetrics(float DeltaTime, bool bForceSample)
{
	RefreshExperiment();
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwner());
	const UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return;
	}
	const float ClampedFrameMs = FMath::Clamp(DeltaTime * 1000.f, 0.1f, 500.f);
	SmoothedFrameMs = FMath::Lerp(SmoothedFrameMs, ClampedFrameMs, 0.2f);

	const ACombatPlayerState* PS = PC->GetPlayerState<ACombatPlayerState>();
	CurrentPingMs = PS ? PS->GetPingInMilliseconds() : 0.f;
	const float SyncedRttMs = PC->TimeSync ? PC->TimeSync->GetRoundTripSeconds() * 1000.f : 0.f;
	CurrentRttMs = SyncedRttMs > 0.f ? SyncedRttMs : CurrentPingMs;
	if (!IsMeasuring())
	{
		return;
	}
	PeakRttMs = FMath::Max(PeakRttMs, CurrentRttMs);
	if (FMath::IsFinite(DeltaTime) && DeltaTime > 0.f)
	{
		FrameStats.Add(DeltaTime * 1000.f);
		FpsStats.Add(1.f / DeltaTime);
	}

	const double WorldTime = World->GetTimeSeconds();
	if (!bForceSample && WorldTime - LastNetSampleWorldTime < NetSampleInterval)
	{
		return;
	}
	LastNetSampleWorldTime = WorldTime;

	FCombatNetSample Sample;
	Sample.TimeSeconds = static_cast<float>(WorldTime);
	Sample.RttMs = CurrentRttMs;
	Sample.PingMs = CurrentPingMs;
	Sample.FrameMs = SmoothedFrameMs;
	// A network client's initial zero ping is an uninitialized reading, not a measured minimum.
	if (CurrentRttMs > 0.f || World->GetNetMode() == NM_Standalone)
	{
		RttStats.Add(CurrentRttMs);
	}
	PushNetSample(Sample);
}

void UCombatTelemetryComponent::RefreshExperiment()
{
	const ACombatGameState* State = GetWorld() ? GetWorld()->GetGameState<ACombatGameState>() : nullptr;
	if (State && ExperimentRevision != State->Experiment.Revision)
	{
		ExperimentRevision = State->Experiment.Revision;
		ResetMeasurement();
		LocalCollectAfter = GetWorld()->GetTimeSeconds() + 1.5;
	}
}

bool UCombatTelemetryComponent::IsMeasuring() const
{
	const ACombatGameState* State = GetWorld() ? GetWorld()->GetGameState<ACombatGameState>() : nullptr;
	if (!State || State->Experiment.Revision == 0)
	{
		return true;
	}
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwner());
	return ExperimentRevision == State->Experiment.Revision && PC && PC->TimeSync && PC->TimeSync->IsReady() &&
		PC->TimeSync->GetServerTime() >= State->Experiment.CollectAfterServerTime &&
		GetWorld()->GetTimeSeconds() >= LocalCollectAfter;
}

void UCombatTelemetryComponent::ResetMeasurement()
{
	PendingShots.Reset();
	NetHead = NetCount = ShotHead = ShotCount = 0;
	TotalSubmitted = TotalAccepted = TotalRejected = TotalHits = 0;
	RttStats = FCombatMetricStats();
	FpsStats = FCombatMetricStats();
	FrameStats = FCombatMetricStats();
	AckStats = FCombatMetricStats();
	ShotAgeStats = FCombatMetricStats();
	TargetAgeStats = FCombatMetricStats();
	PeakRttMs = 0.f;
	LastNetSampleWorldTime = -100.0;
}

void UCombatTelemetryComponent::HandleShotSubmitted(uint32 ShotId)
{
	RefreshExperiment();
	if (!IsMeasuring())
	{
		return;
	}
	++TotalSubmitted;
	if (PendingShots.Num() >= MaxPendingShots)
	{
		PendingShots.RemoveAt(0);
	}
	FPendingShotEntry Entry;
	Entry.ShotId = ShotId;
	Entry.SubmittedPlatformSeconds = FPlatformTime::Seconds();
	Entry.ExperimentRevision = ExperimentRevision;
	PendingShots.Add(Entry);
}

void UCombatTelemetryComponent::HandleShotResult(const FCombatShotResponse& Response)
{
	RefreshExperiment();
	double AckMs = 0.0;
	bool bMatched = false;
	for (int32 Index = 0; Index < PendingShots.Num(); ++Index)
	{
		if (PendingShots[Index].ShotId == Response.ShotId)
		{
			bMatched = PendingShots[Index].ExperimentRevision == Response.ExperimentRevision &&
				Response.ExperimentRevision == ExperimentRevision;
			AckMs = FMath::Max(0.0, (FPlatformTime::Seconds() - PendingShots[Index].SubmittedPlatformSeconds) * 1000.0);
			PendingShots.RemoveAt(Index);
			break;
		}
	}
	// Old, duplicate and uncorrelated responses must not become zero-ms samples in a new interval.
	if (!bMatched || !IsMeasuring())
	{
		return;
	}

	FCombatShotTelemetry Record;
	Record.ShotId = Response.ShotId;
	Record.AckMs = static_cast<float>(AckMs);
	Record.ShotAgeMs = FMath::Max(0.f, Response.ShotAgeSeconds * 1000.f);
	Record.TargetAgeMs = FMath::Max(0.f, Response.TargetAgeSeconds * 1000.f);
	Record.Rejection = Response.Rejection;
	Record.bUsedRewind = Response.bUsedRewind;
	Record.bDamagedPlayer = Response.bDamagedPlayer;

	AckStats.Add(Record.AckMs);
	if (Response.Rejection == EShotRejection::None)
	{
		++TotalAccepted;
		ShotAgeStats.Add(Record.ShotAgeMs);
		TargetAgeStats.Add(Record.TargetAgeMs);
		if (Response.bDamagedPlayer)
		{
			++TotalHits;
		}
	}
	else
	{
		++TotalRejected;
	}

	PushShotSample(Record);
}

void UCombatTelemetryComponent::PushNetSample(const FCombatNetSample& Sample)
{
	if (NetRing.Num() != MaxNetSamples)
	{
		NetRing.SetNum(MaxNetSamples);
	}
	const int32 WriteIndex = (NetHead + NetCount) % MaxNetSamples;
	NetRing[WriteIndex] = Sample;
	if (NetCount < MaxNetSamples)
	{
		++NetCount;
	}
	else
	{
		NetHead = (NetHead + 1) % MaxNetSamples;
	}
}

void UCombatTelemetryComponent::PushShotSample(const FCombatShotTelemetry& Sample)
{
	if (ShotRing.Num() != MaxShotSamples)
	{
		ShotRing.SetNum(MaxShotSamples);
	}
	const int32 WriteIndex = (ShotHead + ShotCount) % MaxShotSamples;
	ShotRing[WriteIndex] = Sample;
	if (ShotCount < MaxShotSamples)
	{
		++ShotCount;
	}
	else
	{
		ShotHead = (ShotHead + 1) % MaxShotSamples;
	}
}

const FCombatNetSample& UCombatTelemetryComponent::GetNetSampleAt(int32 ChronologicalIndex) const
{
	static const FCombatNetSample EmptySample;
	if (NetCount <= 0 || ChronologicalIndex < 0 || ChronologicalIndex >= NetCount)
	{
		return EmptySample;
	}
	return NetRing[(NetHead + ChronologicalIndex) % MaxNetSamples];
}

const FCombatShotTelemetry& UCombatTelemetryComponent::GetShotSampleAt(int32 ChronologicalIndex) const
{
	static const FCombatShotTelemetry EmptyShot;
	if (ShotCount <= 0 || ChronologicalIndex < 0 || ChronologicalIndex >= ShotCount)
	{
		return EmptyShot;
	}
	return ShotRing[(ShotHead + ChronologicalIndex) % MaxShotSamples];
}

bool UCombatTelemetryComponent::GetLastShot(FCombatShotTelemetry& OutShot) const
{
	if (ShotCount <= 0)
	{
		return false;
	}
	OutShot = GetShotSampleAt(ShotCount - 1);
	return true;
}

FCombatTelemetrySummary UCombatTelemetryComponent::GetSummary() const
{
	FCombatTelemetrySummary Summary;
	Summary.Rtt = RttStats;
	Summary.Fps = FpsStats;
	Summary.Frame = FrameStats;
	Summary.Ack = AckStats;
	Summary.ShotAge = ShotAgeStats;
	Summary.TargetAge = TargetAgeStats;
	Summary.TotalSubmitted = TotalSubmitted;
	Summary.TotalAccepted = TotalAccepted;
	Summary.TotalRejected = TotalRejected;
	Summary.TotalHits = TotalHits;
	Summary.AverageAckMs = AckStats.Average();
	Summary.AverageShotAgeMs = ShotAgeStats.Average();
	Summary.AverageTargetAgeMs = TargetAgeStats.Average();
	Summary.CurrentRttMs = CurrentRttMs;
	Summary.PeakRttMs = PeakRttMs;
	Summary.CurrentPingMs = CurrentPingMs;
	Summary.CurrentFrameMs = SmoothedFrameMs;
	Summary.CurrentFps = SmoothedFrameMs > 0.1f ? 1000.f / SmoothedFrameMs : 0.f;
	return Summary;
}

const TCHAR* UCombatTelemetryComponent::RejectionToString(EShotRejection Rejection)
{
	switch (Rejection)
	{
		case EShotRejection::None:
			return TEXT("정상");
		case EShotRejection::InvalidOwner:
			return TEXT("소유권 오류");
		case EShotRejection::InvalidLife:
			return TEXT("이전 생명 요청");
		case EShotRejection::InvalidSequence:
			return TEXT("중복·이전 요청");
		case EShotRejection::InvalidAim:
			return TEXT("조준값 오류");
		case EShotRejection::InvalidTime:
			return TEXT("시각값 오류");
		case EShotRejection::TooOld:
			return TEXT("되감기 범위 초과");
		case EShotRejection::Future:
			return TEXT("미래 시각 요청");
		case EShotRejection::OutOfOrderTime:
			return TEXT("시각 순서 오류");
		case EShotRejection::CannotFire:
			return TEXT("사격 불가 상태");
		case EShotRejection::FireRate:
			return TEXT("사격 간격 제한");
		case EShotRejection::MissingHistory:
			return TEXT("위치 이력 없음");
		default:
			return TEXT("알 수 없음");
	}
}
