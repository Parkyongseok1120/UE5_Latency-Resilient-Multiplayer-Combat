#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "HealthComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FHealthDeath);

// Lives on PlayerState so vitals survive pawn replacement. All writes are server-only.
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class LATENCY_MULTICOMBAT_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHealthComponent();
	void InitializeForSpawn(const FCharacterStats& Stats);
	void ApplyDamage(float Amount);
	void Heal(float Amount);
	void ResetForRespawn();
	void MarkDead();
	// 실험에서 임시 체력을 사용한 뒤 기존 생존 상태로 복구한다. 사망한 플레이어를 되살리지는 않는다.
	void RestoreExperimentVitals(const FCharacterStats& Stats, float Health, float Armor);
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return CurrentHealth; }
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetArmor() const { return CurrentArmor; }
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }
	FHealthDeath OnDeath;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float CurrentArmor = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float MaxHealth = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;
	float SpawnArmor = 0.f;
	float ArmorAbsorptionRatio = 0.f;
};
