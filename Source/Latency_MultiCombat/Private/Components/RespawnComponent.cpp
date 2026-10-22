#include "Components/RespawnComponent.h"
#include "Components/HealthComponent.h"
#include "CombatGameMode.h"
#include "CombatPlayerState.h"
#include "Data/CombatRules.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

bool URespawnComponent::IsScheduled(AController* Controller) const
{
	return Timers.Contains(Controller);
}

void URespawnComponent::Schedule(AController* Controller, float Delay)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Controller) || IsScheduled(Controller))
	{
		return;
	}
	const TWeakObjectPtr<AController> WeakController(Controller);
	FTimerHandle& Handle = Timers.FindOrAdd(WeakController);
	GetWorld()->GetTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateUObject(this, &URespawnComponent::HandleRespawnTimer, WeakController),
		FMath::Max(0.1f, Delay), false);
}

void URespawnComponent::HandleRespawnTimer(TWeakObjectPtr<AController> Controller)
{
	Timers.Remove(Controller);
	if (Controller.IsValid())
	{
		Respawn(Controller.Get());
	}
}

void URespawnComponent::Cancel(AController* Controller)
{
	if (FTimerHandle* Handle = Timers.Find(Controller))
	{
		GetWorld()->GetTimerManager().ClearTimer(*Handle);
		Timers.Remove(Controller);
	}
}

void URespawnComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& Pair : Timers)
	{
		GetWorld()->GetTimerManager().ClearTimer(Pair.Value);
	}
	Timers.Empty();
	Super::EndPlay(Reason);
}

void URespawnComponent::Respawn(AController* Controller)
{
	ACombatGameMode* GM = Cast<ACombatGameMode>(GetOwner());
	if (!GM || !GM->HasAuthority() || !IsValid(Controller) || !FCombatRules::IsMatchActive(GetWorld()))
	{
		return;
	}
	const ACombatPlayerState* PS = Controller->GetPlayerState<ACombatPlayerState>();
	if (!PS || !PS->Health->IsDead())
	{
		return;
	}
	APawn* OldPawn = Controller->GetPawn();
	Controller->UnPossess();
	if (OldPawn)
	{
		OldPawn->Destroy();
	}
	GM->RestartPlayer(Controller);
}
