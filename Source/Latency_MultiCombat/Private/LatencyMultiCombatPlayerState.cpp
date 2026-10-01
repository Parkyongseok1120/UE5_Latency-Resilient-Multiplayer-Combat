// Latency_MultiCombat - Player State Implementation
#include "LatencyMultiCombatPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"

ALatencyMultiCombatPlayerState::ALatencyMultiCombatPlayerState()
	: Team(ETeamSide::Neutral)
	, KillCount(0)
	, DeathCount(0)
	, CurrentHealth(100.0f) // Authoritative spawn initialization occurs on possession.
	, CurrentArmor(0.0f)
	, MaxHealth(100.0f)     // Default; overridden by Data Asset at runtime
	, bIsDead(false)
	, LastDamageTime(0.0f)
{
}

void ALatencyMultiCombatPlayerState::BeginPlay()
{
	Super::BeginPlay();
}

void ALatencyMultiCombatPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALatencyMultiCombatPlayerState, Team);
	DOREPLIFETIME(ALatencyMultiCombatPlayerState, KillCount);
	DOREPLIFETIME(ALatencyMultiCombatPlayerState, DeathCount);
	DOREPLIFETIME(ALatencyMultiCombatPlayerState, CurrentHealth);
	DOREPLIFETIME(ALatencyMultiCombatPlayerState, CurrentArmor);
	DOREPLIFETIME(ALatencyMultiCombatPlayerState, MaxHealth);
	DOREPLIFETIME(ALatencyMultiCombatPlayerState, bIsDead);
}

void ALatencyMultiCombatPlayerState::OnRep_Team()
{
	// Broadcast to interested systems (HUD, etc.)
}

void ALatencyMultiCombatPlayerState::OnRep_KillCount()
{
	// Update HUD score display
}

void ALatencyMultiCombatPlayerState::OnRep_Health()
{
	// Update local health bar
}

// AddKill, AddDeath, SetTeam are FORCEINLINE in header

void ALatencyMultiCombatPlayerState::ApplyDamage(float Amount, AController* Damager)
{
	if (!HasAuthority() || bIsDead || !FMath::IsFinite(Amount) || Amount <= 0.f) return;

	// Armor absorbs 50% of damage
	float ArmorAbsorb = FMath::Min(CurrentArmor, Amount * 0.5f);
	CurrentArmor -= ArmorAbsorb;
	float HealthDamage = Amount - ArmorAbsorb;
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - HealthDamage);

	LastDamageTime = GetWorld()->GetTimeSeconds();

	if (CurrentHealth <= 0.0f)
	{
		AddDeath();
	}
	ForceNetUpdate();
}

void ALatencyMultiCombatPlayerState::Heal(float Amount)
{
	if (!HasAuthority() || bIsDead || !FMath::IsFinite(Amount) || Amount <= 0.f || CurrentHealth >= MaxHealth) return;
	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
	ForceNetUpdate();
}

void ALatencyMultiCombatPlayerState::ResetForRespawn()
{
	if (!HasAuthority()) return;
	bIsDead = false;
	CurrentHealth = MaxHealth;  // SSOT: from Data Asset
	CurrentArmor = 0.0f;
	ForceNetUpdate();
}

void ALatencyMultiCombatPlayerState::InitializeForSpawn(const FCharacterStats& Stats)
{
	if (!HasAuthority()) return;
	MaxHealth = FMath::IsFinite(Stats.MaxHealth) ? FMath::Max(1.f, Stats.MaxHealth) : 100.f;
	CurrentHealth = MaxHealth;
	CurrentArmor = FMath::IsFinite(Stats.MaxArmor) ? FMath::Max(0.f, Stats.MaxArmor) : 0.f;
	bIsDead = false;
	ForceNetUpdate();
}
