#include "Components/HealthComponent.h"
#include "Data/CombatRules.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	SetIsReplicatedByDefault(true);
	const FCharacterStats Defaults;
	CurrentHealth = MaxHealth = Defaults.MaxHealth;
	ArmorAbsorptionRatio = Defaults.ArmorAbsorptionRatio;
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, CurrentHealth);
	DOREPLIFETIME(UHealthComponent, CurrentArmor);
	DOREPLIFETIME(UHealthComponent, MaxHealth);
	DOREPLIFETIME(UHealthComponent, bIsDead);
}

void UHealthComponent::InitializeForSpawn(const FCharacterStats& Stats)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	const FCharacterStats Resolved = CombatRules::NormalizeCharacterStats(Stats);
	MaxHealth = Resolved.MaxHealth;
	SpawnArmor = Resolved.MaxArmor;
	ArmorAbsorptionRatio = Resolved.ArmorAbsorptionRatio;
	ResetForRespawn();
}

void UHealthComponent::ResetForRespawn()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	CurrentHealth = MaxHealth;
	CurrentArmor = SpawnArmor;
	bIsDead = false;
	GetOwner()->ForceNetUpdate();
}

void UHealthComponent::ApplyDamage(float Amount)
{
	if (!GetOwner()->HasAuthority() || bIsDead || !FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return;
	}
	const float Absorbed = FMath::Min(CurrentArmor, Amount * ArmorAbsorptionRatio);
	CurrentArmor -= Absorbed;
	CurrentHealth = FMath::Max(0.f, CurrentHealth - (Amount - Absorbed));
	if (CurrentHealth <= 0.f)
	{
		MarkDead();
	}
	GetOwner()->ForceNetUpdate();
}

void UHealthComponent::MarkDead()
{
	if (!GetOwner()->HasAuthority() || bIsDead)
	{
		return;
	}
	bIsDead = true;
	OnDeath.Broadcast();
	GetOwner()->ForceNetUpdate();
}

void UHealthComponent::Heal(float Amount)
{
	if (!GetOwner()->HasAuthority() || bIsDead || !FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return;
	}
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
	GetOwner()->ForceNetUpdate();
}
