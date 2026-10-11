#include "CombatPlayerState.h"
#include "Components/HealthComponent.h"
#include "Net/UnrealNetwork.h"

ACombatPlayerState::ACombatPlayerState() : Team(ETeamSide::Neutral), KillCount(0), DeathCount(0)
{
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->OnDeath.AddUObject(this, &ACombatPlayerState::HandleDeath);
}

void ACombatPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatPlayerState, Team);
	DOREPLIFETIME(ACombatPlayerState, KillCount);
	DOREPLIFETIME(ACombatPlayerState, DeathCount);
}

void ACombatPlayerState::HandleDeath()
{
	if (!HasAuthority())
	{
		return;
	}
	++DeathCount;
	ForceNetUpdate();
}
