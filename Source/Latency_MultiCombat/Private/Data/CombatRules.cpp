#include "Data/CombatRules.h"
#include "Data/UCharacterDataAsset.h"
#include "Data/UWeaponDataAsset.h"
#include "CombatGameState.h"
#include "Engine/World.h"

FCharacterStats CombatRules::NormalizeCharacterStats(const FCharacterStats& Stats)
{
	FCharacterStats Result = Stats;
	const FCharacterStats Defaults;
	Result.MaxHealth = FMath::IsFinite(Stats.MaxHealth) ? FMath::Max(1.f, Stats.MaxHealth) : Defaults.MaxHealth;
	Result.MaxArmor = FMath::IsFinite(Stats.MaxArmor) ? FMath::Max(0.f, Stats.MaxArmor) : 0.f;
	Result.ArmorAbsorptionRatio = FMath::IsFinite(Stats.ArmorAbsorptionRatio)
		? FMath::Clamp(Stats.ArmorAbsorptionRatio, 0.f, 1.f)
		: Defaults.ArmorAbsorptionRatio;
	return Result;
}

FCharacterStats CombatRules::ReadCharacterStats(const UCharacterDataAsset* Data)
{
	return NormalizeCharacterStats(Data ? Data->Stats : FCharacterStats());
}

FWeaponStats CombatRules::ReadWeaponStats(const UWeaponDataAsset* Data)
{
	FWeaponStats Stats = Data ? Data->Stats : FWeaponStats();
	Stats.FireRate = FMath::Clamp(Stats.FireRate, 0.1f, 30.f);
	Stats.MagazineSize = FMath::Max(1, Stats.MagazineSize);
	Stats.ReserveAmmo = FMath::Max(0, Stats.ReserveAmmo);
	Stats.ReloadTime = FMath::Max(0.01f, Stats.ReloadTime);
	Stats.MaxRange = FMath::Max(0.f, Stats.MaxRange);
	Stats.MinDamageRatio = FMath::Clamp(Stats.MinDamageRatio, 0.f, 1.f);
	return Stats;
}

UWeaponDataAsset* CombatRules::ResolveWeaponData(UWeaponDataAsset* Override, const UCharacterDataAsset* CharacterData)
{
	if (Override)
	{
		return Override;
	}
	return CharacterData && !CharacterData->DefaultWeapon.IsNull() ? CharacterData->DefaultWeapon.LoadSynchronous()
																   : nullptr;
}

float CombatRules::DamageAtRange(const FWeaponStats& Stats, float DistanceCm)
{
	const float Falloff = FMath::Clamp((DistanceCm / CentimetersPerMeter - Stats.RangeFalloffStart) /
			FMath::Max(0.01f, Stats.RangeFalloffEnd - Stats.RangeFalloffStart),
		0.f, 1.f);
	return Stats.Damage * FMath::Lerp(1.f, Stats.MinDamageRatio, Falloff);
}

bool CombatRules::IsMatchActive(const UWorld* World)
{
	const ACombatGameState* GS = World ? World->GetGameState<ACombatGameState>() : nullptr;
	return !GS || !GS->bMatchEnded;
}
