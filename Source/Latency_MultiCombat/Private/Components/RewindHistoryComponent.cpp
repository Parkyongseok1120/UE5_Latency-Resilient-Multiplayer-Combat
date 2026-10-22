#include "Components/RewindHistoryComponent.h"
#include "CombatCharacter.h"
#include "CombatPlayerState.h"
#include "Components/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Data/CombatNetworkDataAsset.h"
#include "Engine/World.h"

void FRewindHistoryBuffer::Reset(int32 Capacity)
{
	Frames.SetNum(FMath::Clamp(Capacity, 2, 512));
	Head = 0;
	Count = 0;
}

const FRewindFrame& FRewindHistoryBuffer::At(int32 Index) const
{
	return Frames[(Head - Count + Index + Frames.Num()) % Frames.Num()];
}

void FRewindHistoryBuffer::Add(const FRewindFrame& Frame)
{
	if (Frames.IsEmpty() || !FMath::IsFinite(Frame.Time))
	{
		return;
	}
	if (Count > 0 && Frame.Time <= At(Count - 1).Time)
	{
		if (Frame.Time == At(Count - 1).Time)
		{
			Frames[(Head - 1 + Frames.Num()) % Frames.Num()] = Frame;
		}
		return;
	}
	Frames[Head] = Frame;
	Head = (Head + 1) % Frames.Num();
	Count = FMath::Min(Count + 1, Frames.Num());
}

bool FRewindHistoryBuffer::Sample(double Time, FRewindFrame& Out, float MaxDistance) const
{
	if (Count == 0 || !FMath::IsFinite(Time) || Time < At(0).Time || Time > At(Count - 1).Time)
	{
		return false;
	}
	for (int32 Index = Count - 1; Index >= 0; --Index)
	{
		const FRewindFrame& Before = At(Index);
		if (Time == Before.Time)
		{
			Out = Before;
			return true;
		}
		if (Before.Time < Time && Index + 1 < Count)
		{
			const FRewindFrame& After = At(Index + 1);
			if (Before.LifeId != After.LifeId || Before.Team != After.Team || Before.bAlive != After.bAlive ||
				FVector::DistSquared(Before.Location, After.Location) > FMath::Square(MaxDistance))
			{
				return false;
			}
			const double Alpha = (Time - Before.Time) / (After.Time - Before.Time);
			Out = Before;
			Out.Time = Time;
			Out.Location = FMath::Lerp(Before.Location, After.Location, Alpha);
			Out.ViewLocation = FMath::Lerp(Before.ViewLocation, After.ViewLocation, Alpha);
			Out.Velocity = FMath::Lerp(Before.Velocity, After.Velocity, Alpha);
			Out.Radius = FMath::Lerp(Before.Radius, After.Radius, Alpha);
			Out.HalfHeight = FMath::Lerp(Before.HalfHeight, After.HalfHeight, Alpha);
			return true;
		}
	}
	return false;
}

URewindHistoryComponent::URewindHistoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void URewindHistoryComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(GetOwner()->HasAuthority());
	ResetHistory();
}

void URewindHistoryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	History.Reset(2);
	Super::EndPlay(Reason);
}

void URewindHistoryComponent::ResetHistory()
{
	const FLagCompensationSettings Settings = FLagCompensationSettings::Get(GetWorld());
	History.Reset(
		FMath::CeilToInt((Settings.MaxRewindSeconds + Settings.MaxTargetViewDelay) / Settings.RecordInterval) + 3);
	LastRecordTime = -1.0;
	RecordNow();
}

bool URewindHistoryComponent::Capture(FRewindFrame& Out) const
{
	const ACombatCharacter* Character = Cast<ACombatCharacter>(GetOwner());
	if (!Character || !Character->HasAuthority() || !Character->GetLifeId().IsValid())
	{
		return false;
	}
	const ACombatPlayerState* PS = Character->GetPlayerState<ACombatPlayerState>();
	Out.Time = GetWorld()->GetTimeSeconds();
	Out.Location = Character->GetActorLocation();
	Out.ViewLocation = Character->GetPawnViewLocation();
	Out.Velocity = Character->GetVelocity();
	Out.LifeId = Character->GetLifeId();
	Out.Team = PS ? PS->Team : ETeamSide::Neutral;
	Out.Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	Out.HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Out.bAlive =
		PS && Character->GetController() && !PS->Health->IsDead() && Character->GetCombatState() != ECombatState::Dead;
	return true;
}

void URewindHistoryComponent::RecordNow()
{
	FRewindFrame Frame;
	if (Capture(Frame))
	{
		History.Add(Frame);
		LastRecordTime = Frame.Time;
	}
}

bool URewindHistoryComponent::Sample(double Time, FRewindFrame& Out)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}
	// RPC가 먼저 들어와도 현재 프레임까지는 기록해둠.
	RecordNow();
	return History.Sample(Time, Out, FLagCompensationSettings::Get(GetWorld()).MaxInterpolationDistance);
}

void URewindHistoryComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (GetWorld()->GetTimeSeconds() - LastRecordTime >= FLagCompensationSettings::Get(GetWorld()).RecordInterval)
	{
		RecordNow();
	}
}
