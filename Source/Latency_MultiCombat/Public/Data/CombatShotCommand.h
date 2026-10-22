#pragma once

#include "CoreMinimal.h"
#include "CombatShotCommand.generated.h"

UENUM()
enum class EShotRejection : uint8
{
	None,
	InvalidOwner,
	InvalidLife,
	InvalidSequence,
	InvalidAim,
	InvalidTime,
	TooOld,
	Future,
	OutOfOrderTime,
	CannotFire,
	FireRate,
	MissingHistory
};

USTRUCT()
struct FCombatShotCommand
{
	GENERATED_BODY()
	UPROPERTY()
	uint32 ShotId = 0;
	UPROPERTY()
	FGuid LifeId;
	UPROPERTY()
	double ServerFireTime = 0.0;
	UPROPERTY()
	FRotator AimRotation = FRotator::ZeroRotator;
};

USTRUCT()
struct FCombatShotResponse
{
	GENERATED_BODY()
	UPROPERTY()
	uint32 ShotId = 0;
	UPROPERTY()
	EShotRejection Rejection = EShotRejection::None;
	UPROPERTY()
	bool bDamagedPlayer = false;
	UPROPERTY()
	bool bUsedRewind = false;
	UPROPERTY()
	float ShotAgeSeconds = 0.f;
	UPROPERTY()
	float TargetAgeSeconds = 0.f;
};
