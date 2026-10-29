#include "Data/CombatRules.h"
#include "Data/UCharacterDataAsset.h"
#include "Data/UWeaponDataAsset.h"
#include "CombatGameState.h"
#include "Engine/World.h"

static float ClampCombatValue(float Value, float Minimum, float Maximum, float Fallback)
{
	return FMath::Clamp(FMath::IsFinite(Value) ? Value : Fallback, Minimum, Maximum);
}

FCharacterStats FCombatRules::NormalizeCharacterStats(const FCharacterStats& Stats)
{
	FCharacterStats Result = Stats;
	const FCharacterStats Defaults;
	Result.MaxHealth = FMath::IsFinite(Stats.MaxHealth) ? FMath::Max(1.f, Stats.MaxHealth) : Defaults.MaxHealth;
	Result.MaxArmor = FMath::IsFinite(Stats.MaxArmor) ? FMath::Max(0.f, Stats.MaxArmor) : 0.f;
	Result.ArmorAbsorptionRatio = FMath::IsFinite(Stats.ArmorAbsorptionRatio)
		? FMath::Clamp(Stats.ArmorAbsorptionRatio, 0.f, 1.f)
		: Defaults.ArmorAbsorptionRatio;
	// 에셋 수치가 잘못 들어와도 이동이나 조준까지 깨지지는 않게 막아둠.
	Result.WalkSpeed = ClampCombatValue(Stats.WalkSpeed, 0.f, 10000.f, Defaults.WalkSpeed);
	Result.RunSpeed = ClampCombatValue(Stats.RunSpeed, 0.f, 10000.f, Defaults.RunSpeed);
	Result.CrouchSpeed = ClampCombatValue(Stats.CrouchSpeed, 0.f, 10000.f, Defaults.CrouchSpeed);
	Result.JumpVelocity = ClampCombatValue(Stats.JumpVelocity, 0.f, 10000.f, Defaults.JumpVelocity);
	Result.AirControl = ClampCombatValue(Stats.AirControl, 0.f, 1.f, Defaults.AirControl);
	Result.AimDownSightFOV = ClampCombatValue(Stats.AimDownSightFOV, 20.f, 120.f, Defaults.AimDownSightFOV);
	Result.HipFireSpreadMultiplier =
		ClampCombatValue(Stats.HipFireSpreadMultiplier, 0.f, 10.f, Defaults.HipFireSpreadMultiplier);
	return Result;
}

FCharacterStats FCombatRules::ReadCharacterStats(const UCharacterDataAsset* Data)
{
	return NormalizeCharacterStats(Data ? Data->Stats : FCharacterStats());
}

FWeaponStats FCombatRules::ReadWeaponStats(const UWeaponDataAsset* Data)
{
	FWeaponStats Stats = Data ? Data->Stats : FWeaponStats();
	const FWeaponStats Defaults;
	Stats.Damage = ClampCombatValue(Stats.Damage, 0.f, 100000.f, Defaults.Damage);
	Stats.FireRate = ClampCombatValue(Stats.FireRate, 0.1f, 30.f, Defaults.FireRate);
	Stats.MagazineSize = FMath::Max(1, Stats.MagazineSize);
	Stats.ReserveAmmo = FMath::Max(0, Stats.ReserveAmmo);
	Stats.ReloadTime = ClampCombatValue(Stats.ReloadTime, 0.01f, 600.f, Defaults.ReloadTime);
	Stats.MaxRange = ClampCombatValue(Stats.MaxRange, 0.f, 100000.f, Defaults.MaxRange);
	Stats.BaseSpread = ClampCombatValue(Stats.BaseSpread, 0.f, 45.f, Defaults.BaseSpread);
	Stats.MovingSpreadMultiplier =
		ClampCombatValue(Stats.MovingSpreadMultiplier, 0.f, 10.f, Defaults.MovingSpreadMultiplier);
	Stats.HeadshotMultiplier = ClampCombatValue(Stats.HeadshotMultiplier, 0.f, 100.f, Defaults.HeadshotMultiplier);
	Stats.RangeFalloffStart =
		ClampCombatValue(Stats.RangeFalloffStart, 0.f, Stats.MaxRange, Defaults.RangeFalloffStart);
	Stats.RangeFalloffEnd = ClampCombatValue(Stats.RangeFalloffEnd, Stats.RangeFalloffStart,
		FMath::Max(Stats.RangeFalloffStart, Stats.MaxRange), Defaults.RangeFalloffEnd);
	Stats.MinDamageRatio = ClampCombatValue(Stats.MinDamageRatio, 0.f, 1.f, Defaults.MinDamageRatio);
	return Stats;
}

UWeaponDataAsset* FCombatRules::ResolveWeaponData(UWeaponDataAsset* Override, const UCharacterDataAsset* CharacterData)
{
	if (Override)
	{
		return Override;
	}
	return CharacterData && !CharacterData->DefaultWeapon.IsNull() ? CharacterData->DefaultWeapon.LoadSynchronous()
																   : nullptr;
}

float FCombatRules::DamageAtRange(const FWeaponStats& Stats, float DistanceCm)
{
	const float Falloff = FMath::Clamp((DistanceCm / CentimetersPerMeter - Stats.RangeFalloffStart) /
			FMath::Max(0.01f, Stats.RangeFalloffEnd - Stats.RangeFalloffStart),
		0.f, 1.f);
	return Stats.Damage * FMath::Lerp(1.f, Stats.MinDamageRatio, Falloff);
}

bool FCombatRules::IsMatchActive(const UWorld* World)
{
	const ACombatGameState* GS = World ? World->GetGameState<ACombatGameState>() : nullptr;
	return !GS || !GS->bMatchEnded;
}

FVector FCombatRules::SanitizeVector(const FVector& Value, const FVector& Fallback, float MaxAbs)
{
	if (!FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y) || !FMath::IsFinite(Value.Z))
	{
		return Fallback;
	}
	return FVector(FMath::Clamp(Value.X, -MaxAbs, MaxAbs), FMath::Clamp(Value.Y, -MaxAbs, MaxAbs),
		FMath::Clamp(Value.Z, -MaxAbs, MaxAbs));
}

FRotator FCombatRules::SanitizeRotator(const FRotator& Value, const FRotator& Fallback)
{
	if (!FMath::IsFinite(Value.Pitch) || !FMath::IsFinite(Value.Yaw) || !FMath::IsFinite(Value.Roll))
	{
		return Fallback;
	}
	return FRotator(FRotator::NormalizeAxis(Value.Pitch), FRotator::NormalizeAxis(Value.Yaw),
		FRotator::NormalizeAxis(Value.Roll));
}
