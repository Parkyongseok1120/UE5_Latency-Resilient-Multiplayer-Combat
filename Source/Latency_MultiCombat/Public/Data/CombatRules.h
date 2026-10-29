#pragma once

#include "CoreMinimal.h"
#include "Data/CombatTypes.h"

class UCharacterDataAsset;
class UWeaponDataAsset;
class UWorld;

// Data assets author defaults; these functions are the shared runtime validation boundary.
struct FCombatRules
{
public:
	static constexpr float CentimetersPerMeter = 100.f;
	static constexpr float MovingSpeedSquared = 100.f;
	static LATENCY_MULTICOMBAT_API FCharacterStats NormalizeCharacterStats(const FCharacterStats& Stats);
	static LATENCY_MULTICOMBAT_API FCharacterStats ReadCharacterStats(const UCharacterDataAsset* Data);
	static LATENCY_MULTICOMBAT_API FWeaponStats ReadWeaponStats(const UWeaponDataAsset* Data);
	static LATENCY_MULTICOMBAT_API UWeaponDataAsset* ResolveWeaponData(
		UWeaponDataAsset* Override, const UCharacterDataAsset* CharacterData);
	static LATENCY_MULTICOMBAT_API float DamageAtRange(const FWeaponStats& Stats, float DistanceCm);
	static LATENCY_MULTICOMBAT_API bool IsMatchActive(const UWorld* World);
	static LATENCY_MULTICOMBAT_API FVector SanitizeVector(
		const FVector& Value, const FVector& Fallback, float MaxAbs = 10000.f);
	static LATENCY_MULTICOMBAT_API FRotator SanitizeRotator(const FRotator& Value, const FRotator& Fallback);
};
