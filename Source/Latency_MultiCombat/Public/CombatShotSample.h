#pragma once
#include "CoreMinimal.h"

// A sequential benchmark sample. Timestamps use only the shooter's monotonic clock.
struct FCombatShotSample
{
	uint32 ShotId = 0;
	double StartedAt = 0.0;
	float HealthBefore = 0.f;
	float HealthAfter = 0.f;
	float RangeCm = 0.f;
	double FeedbackMs = -1.0;
	double HitConfirmMs = -1.0;
	double HealthMs = -1.0;
	bool bHit = false;
	bool Feedback(uint32 Id, bool bDamage, double Now)
	{
		if (Id != ShotId || FeedbackMs >= 0.0 || Now < StartedAt)
		{
			return false;
		}
		FeedbackMs = (Now - StartedAt) * 1000.0;
		bHit = bDamage;
		return true;
	}
	bool Confirm(uint32 Id, double Now)
	{
		if (Id != ShotId || HitConfirmMs >= 0.0 || Now < StartedAt)
		{
			return false;
		}
		HitConfirmMs = (Now - StartedAt) * 1000.0;
		return true;
	}
	void ObserveHealth(float Health, double Now)
	{
		if (HealthMs < 0.0 && Health < HealthBefore && Now >= StartedAt)
		{
			HealthMs = (Now - StartedAt) * 1000.0;
			HealthAfter = Health;
		}
	}
	bool IsComplete() const { return FeedbackMs >= 0.0 && (!bHit || (HitConfirmMs >= 0.0 && HealthMs >= 0.0)); }
};
