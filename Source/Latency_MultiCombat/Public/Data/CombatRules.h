#pragma once

#include "CoreMinimal.h"
#include "Data/CombatTypes.h"

class UCharacterDataAsset;
class UWeaponDataAsset;
class UWorld;

// Data assets author defaults; these functions are the shared runtime validation boundary.
namespace CombatRules
{
constexpr float CentimetersPerMeter = 100.f;
constexpr float MovingSpeedSquared = 100.f;
LATENCY_MULTICOMBAT_API FCharacterStats NormalizeCharacterStats(const FCharacterStats& Stats);
LATENCY_MULTICOMBAT_API FCharacterStats ReadCharacterStats(const UCharacterDataAsset* Data);
LATENCY_MULTICOMBAT_API FWeaponStats ReadWeaponStats(const UWeaponDataAsset* Data);
LATENCY_MULTICOMBAT_API UWeaponDataAsset* ResolveWeaponData(
	UWeaponDataAsset* Override, const UCharacterDataAsset* CharacterData);
LATENCY_MULTICOMBAT_API float DamageAtRange(const FWeaponStats& Stats, float DistanceCm);
LATENCY_MULTICOMBAT_API bool IsMatchActive(const UWorld* World);
} // namespace CombatRules
