#include "Components/CombatFeedbackComponent.h"
#include "Camera/CombatFireCameraShake.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CombatCharacter.h"
#include "CombatPlayerController.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Components/HealthComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UCombatFeedbackComponent::UCombatFeedbackComponent()
{
	SetIsReplicatedByDefault(false);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(
		TEXT("/Game/UI/Materials/M_CombatDamage.M_CombatDamage"));
	DamageMaterial = Material.Object;
}

void UCombatFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	SetComponentTickEnabled(PC && PC->IsLocalController() && GetNetMode() != NM_DedicatedServer);
}

void UCombatFeedbackComponent::StopFireShake()
{
	if (const APlayerController* PC = Cast<APlayerController>(GetOwner());
		PC && PC->PlayerCameraManager && ActiveFireShake.IsValid())
	{
		PC->PlayerCameraManager->StopCameraShake(ActiveFireShake.Get(), true);
	}
	ActiveFireShake.Reset();
}

bool UCombatFeedbackComponent::HasActiveFireShake() const
{
	return ActiveFireShake.IsValid() && !ActiveFireShake->IsFinished();
}

void UCombatFeedbackComponent::PlayFireShake()
{
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwner());
	const ACombatCharacter* Pawn = PC ? Cast<ACombatCharacter>(PC->GetPawn()) : nullptr;
	if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager || GetNetMode() == NM_DedicatedServer || !Pawn ||
		Pawn->GetCombatState() == ECombatState::Dead)
	{
		return;
	}
	const bool bADS = Pawn->GetWeapon() && Pawn->GetWeapon()->Fire->IsADS();
	UpdateFeedback(0.f);
	const float Scale = FMath::Max(0.f, FireShakeScale) * (bADS ? FMath::Clamp(ADSShakeMultiplier, 0.f, 1.f) : 1.f);
	if (Scale > 0.f)
	{
		ActiveFireShake = PC->PlayerCameraManager->StartCameraShake(UCombatFireCameraShake::StaticClass(), Scale);
	}
}

void UCombatFeedbackComponent::DetachCamera()
{
	if (UCameraComponent* Camera = BoundCamera.Get())
	{
		Camera->PostProcessSettings.WeightedBlendables.Array.RemoveAll(
			[this](const FWeightedBlendable& Blend) { return Blend.Object == DamageInstance; });
	}
	BoundCamera.Reset();
	DamageInstance = nullptr;
}

void UCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopFireShake();
	DetachCamera();
	Super::EndPlay(Reason);
}

void UCombatFeedbackComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	UpdateFeedback(DeltaTime);
}

void UCombatFeedbackComponent::UpdateFeedback(float DeltaSeconds)
{
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	ACombatCharacter* Pawn = Cast<ACombatCharacter>(PC->GetPawn());
	if (Pawn != ObservedPawn.Get())
	{
		StopFireShake();
		DetachCamera();
		ObservedPawn = Pawn;
		bHasVitals = false;
		HitFlash = LowHealthIntensity = 0.f;
	}
	const ACombatPlayerState* PS = PC->GetPlayerState<ACombatPlayerState>();
	const UHealthComponent* Health = PS ? PS->Health.Get() : nullptr;
	if (!Pawn || !Health || Health->GetMaxHealth() <= 0.f)
	{
		return;
	}
	HitFlash = FMath::Max(0.f, HitFlash - FMath::Max(0.f, DeltaSeconds) * HitFadeSpeed);
	const float CurrentHealth = Health->GetHealth();
	const float CurrentArmor = Health->GetArmor();
	if (bHasVitals &&
		(CurrentHealth < LastHealth - KINDA_SMALL_NUMBER || CurrentArmor < LastArmor - KINDA_SMALL_NUMBER))
	{
		HitFlash = 1.f;
	}
	LastHealth = CurrentHealth;
	LastArmor = CurrentArmor;
	bHasVitals = true;
	const float Ratio = FMath::Clamp(CurrentHealth / Health->GetMaxHealth(), 0.f, 1.f);
	const float Critical = FMath::Clamp((0.5f - Ratio) * 2.f, 0.f, 1.f);
	const float Target = Health->IsDead() || Ratio > 0.5f
		? 0.f
		: FMath::Lerp(0.22f, 1.f, FMath::Pow(Critical, 0.8f)) * FMath::Clamp(MaxGlitchIntensity, 0.f, 1.f);
	LowHealthIntensity = FMath::FInterpTo(LowHealthIntensity, Target, DeltaSeconds, 7.f);
	if (Health->IsDead())
	{
		StopFireShake();
	}
	if (!DamageInstance && DamageMaterial)
	{
		BoundCamera = Pawn->FirstPersonCamera;
		DamageInstance = UMaterialInstanceDynamic::Create(DamageMaterial, this);
		BoundCamera->PostProcessSettings.WeightedBlendables.Array.Emplace(0.f, DamageInstance);
	}
	if (DamageInstance && BoundCamera.IsValid())
	{
		DamageInstance->SetScalarParameterValue(TEXT("HitFlash"), HitFlash);
		DamageInstance->SetScalarParameterValue(TEXT("LowHealth"), LowHealthIntensity);
		DamageInstance->SetScalarParameterValue(TEXT("EffectTime"), GetWorld()->GetTimeSeconds());
		for (FWeightedBlendable& Blend : BoundCamera->PostProcessSettings.WeightedBlendables.Array)
		{
			if (Blend.Object == DamageInstance)
			{
				Blend.Weight = HitFlash > 0.001f || LowHealthIntensity > 0.001f ? 1.f : 0.f;
			}
		}
	}
}
