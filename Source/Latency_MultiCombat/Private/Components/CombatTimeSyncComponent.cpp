#include "Components/CombatTimeSyncComponent.h"
#include "Data/CombatNetworkDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UCombatTimeSyncComponent::UCombatTimeSyncComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}

bool UCombatTimeSyncComponent::IsReady() const
{
	return GetOwner()->HasAuthority() || bReady;
}

double UCombatTimeSyncComponent::GetServerTime() const
{
	return GetWorld()->GetTimeSeconds() + (GetOwner()->HasAuthority() ? 0.0 : Offset);
}

void UCombatTimeSyncComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || PC->HasAuthority() || !PC->IsLocalController())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastRequestAt >= FLagCompensationSettings::Get(GetWorld()).ClockSyncInterval)
	{
		LastRequestAt = Now;
		ServerRequestTime(++RequestSequence, Now);
	}
}

void UCombatTimeSyncComponent::ServerRequestTime_Implementation(uint32 RequestId, double SentAt)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (FMath::IsFinite(SentAt) && Now - LastServerRequestAt >= 0.1)
	{
		LastServerRequestAt = Now;
		ClientReceiveTime(RequestId, SentAt, Now);
	}
}

void UCombatTimeSyncComponent::ClientReceiveTime_Implementation(uint32 RequestId, double SentAt, double ServerTime)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const double RTT = Now - SentAt;
	if (RequestId != RequestSequence || SentAt != LastRequestAt || !FMath::IsFinite(ServerTime) || RTT < 0.0 ||
		RTT > 3.0)
	{
		return;
	}
	RoundTripSeconds = RTT;
	Offset = ServerTime + RTT * 0.5 - Now;
	bReady = true;
}
