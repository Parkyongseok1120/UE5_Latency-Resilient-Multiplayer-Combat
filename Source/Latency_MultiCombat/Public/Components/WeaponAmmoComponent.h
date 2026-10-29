#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "WeaponAmmoComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FWeaponReloadChanged);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class LATENCY_MULTICOMBAT_API UWeaponAmmoComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWeaponAmmoComponent();
	void Initialize(const FWeaponStats& Stats);
	bool ConsumeRound();
	void StartReload();
	void FinishReload();
	void CancelReload();
	void RestoreExperimentAmmo(int32 Magazine, int32 Reserve);
	UFUNCTION(BlueprintPure, Category = "Ammo")
	int32 GetMagazineAmmo() const { return MagazineAmmo; }
	UFUNCTION(BlueprintPure, Category = "Ammo")
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	UFUNCTION(BlueprintPure, Category = "Ammo")
	bool IsReloading() const { return bIsReloading; }
	FWeaponReloadChanged OnReloadChanged;

protected:
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION(Server, Reliable)
	void ServerStartReload();
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ammo", meta = (AllowPrivateAccess = "true"))
	int32 MagazineAmmo = 0;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ammo", meta = (AllowPrivateAccess = "true"))
	int32 ReserveAmmo = 0;
	bool bIsReloading = false;
	float ReloadEndTime = 0.f;
};
