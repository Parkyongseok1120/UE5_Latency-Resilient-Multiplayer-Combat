#include "Components/CombatTelemetryComponent.h"
#include "CombatCharacter.h"
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
	PeakRttMs = FMath::Max(PeakRttMs, CurrentRttMs);

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
	PushNetSample(Sample);
}

void UCombatTelemetryComponent::HandleShotSubmitted(uint32 ShotId)
{
	++TotalSubmitted;
	if (PendingShots.Num() >= MaxPendingShots)
	{
		PendingShots.RemoveAt(0);
	}
	FPendingShotEntry Entry;
	Entry.ShotId = ShotId;
	Entry.SubmittedPlatformSeconds = FPlatformTime::Seconds();
	PendingShots.Add(Entry);
}

void UCombatTelemetryComponent::HandleShotResult(const FCombatShotResponse& Response)
{
	double AckMs = 0.0;
	for (int32 Index = 0; Index < PendingShots.Num(); ++Index)
	{
		if (PendingShots[Index].ShotId == Response.ShotId)
		{
			AckMs = FMath::Max(0.0, (FPlatformTime::Seconds() - PendingShots[Index].SubmittedPlatformSeconds) * 1000.0);
			PendingShots.RemoveAt(Index);
			break;
		}
	}

	FCombatShotTelemetry Record;
	Record.ShotId = Response.ShotId;
	Record.AckMs = static_cast<float>(AckMs);
	Record.ShotAgeMs = FMath::Max(0.f, Response.ShotAgeSeconds * 1000.f);
	Record.TargetAgeMs = FMath::Max(0.f, Response.TargetAgeSeconds * 1000.f);
	Record.Rejection = Response.Rejection;
	Record.bUsedRewind = Response.bUsedRewind;
	Record.bDamagedPlayer = Response.bDamagedPlayer;

	++TotalResultCount;
	AccumulatedAckMs += Record.AckMs;
	if (Response.Rejection == EShotRejection::None)
	{
		++TotalAccepted;
		AccumulatedShotAgeMs += Record.ShotAgeMs;
		AccumulatedTargetAgeMs += Record.TargetAgeMs;
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
	Summary.TotalSubmitted = TotalSubmitted;
	Summary.TotalAccepted = TotalAccepted;
	Summary.TotalRejected = TotalRejected;
	Summary.TotalHits = TotalHits;
	Summary.AverageAckMs = TotalResultCount > 0 ? static_cast<float>(AccumulatedAckMs / TotalResultCount) : 0.f;
	Summary.AverageShotAgeMs = TotalAccepted > 0 ? static_cast<float>(AccumulatedShotAgeMs / TotalAccepted) : 0.f;
	Summary.AverageTargetAgeMs = TotalAccepted > 0 ? static_cast<float>(AccumulatedTargetAgeMs / TotalAccepted) : 0.f;
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
			return TEXT("OK");
		case EShotRejection::InvalidOwner:
			return TEXT("InvalidOwner");
		case EShotRejection::InvalidLife:
			return TEXT("StaleLife");
		case EShotRejection::InvalidSequence:
			return TEXT("DupOrOldSeq");
		case EShotRejection::InvalidAim:
			return TEXT("InvalidAim");
		case EShotRejection::InvalidTime:
			return TEXT("InvalidTime");
		case EShotRejection::TooOld:
			return TEXT("TooOld(>MaxRewind)");
		case EShotRejection::Future:
			return TEXT("FutureTime");
		case EShotRejection::OutOfOrderTime:
			return TEXT("OutOfOrderTime");
		case EShotRejection::CannotFire:
			return TEXT("BlockedState");
		case EShotRejection::FireRate:
			return TEXT("FireRateLimit");
		case EShotRejection::MissingHistory:
			return TEXT("MissingHistory");
		default:
			return TEXT("Unknown");
	}
}
