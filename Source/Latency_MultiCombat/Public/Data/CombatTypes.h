// Latency_MultiCombat - Consolidated Combat Data Types (SSOT)
#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

// ─────────────────────────────────────────────
// Enums
// ─────────────────────────────────────────────

UENUM(BlueprintType)
enum class ECombatState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Moving		UMETA(DisplayName = "Moving"),
	Aiming		UMETA(DisplayName = "Aiming"),
	Firing		UMETA(DisplayName = "Firing"),
	Reloading	UMETA(DisplayName = "Reloading"),
	Dead		UMETA(DisplayName = "Dead")
};

UENUM(BlueprintType)
enum class ETeamSide : uint8
{
	Neutral	UMETA(DisplayName = "Neutral"),
	Red		UMETA(DisplayName = "Red Team"),
	Blue	UMETA(DisplayName = "Blue Team")
};

// ─────────────────────────────────────────────
// Weapon Stats
// ─────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Damage")
	float Damage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Fire")
	float FireRate = 8.0f; // shots per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo")
	int32 MagazineSize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Ammo")
	int32 ReserveAmmo = 90;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Fire")
	float ReloadTime = 2.0f; // seconds

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Range")
	float MaxRange = 150.0f; // meters

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Spread")
	float BaseSpread = 1.5f; // degrees

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Spread")
	float MovingSpreadMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Mode")
	bool bFullAuto = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Special")
	float HeadshotMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Special")
	float RangeFalloffStart = 50.0f; // meters where falloff begins

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Special")
	float RangeFalloffEnd = 120.0f; // meters where min damage applies

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Special")
	float MinDamageRatio = 0.5f; // damage at max range as ratio of base
};

// ─────────────────────────────────────────────
// Character Stats (SSOT: Data Asset is the single source)
// ─────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FCharacterStats
{
	GENERATED_BODY()

	// Health & Armor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Health")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Health")
	float MaxArmor = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Health")
	float HealthRegenRate = 5.0f; // HP per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Health")
	float HealthRegenDelay = 3.0f; // seconds after last damage before regen starts

	// Movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
	float RunSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
	float CrouchSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
	float AirControl = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
	float JumpVelocity = 420.0f;

	// Combat
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Combat")
	float AimDownSightFOV = 45.0f; // zoom FOV when ADS

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Combat")
	float HipFireSpreadMultiplier = 1.5f;
};
