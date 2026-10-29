#include "Components/CharacterPresentationComponent.h"
#include "CombatCharacter.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Components/WeaponFireComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Data/CombatRules.h"
#include "Data/UCharacterDataAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UCharacterPresentationComponent::UCharacterPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	DefaultBodyMesh = Mesh.Object;
	DefaultBodyMaterial = Material.Object;
}

void UCharacterPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyAssets();
	ACombatCharacter* Character = Cast<ACombatCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	BodyMaterials.Reset();
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bUsesSkeletalBody)
		{
			const int32 SlotCount = Character->GetMesh()->GetNumMaterials();
			for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
			{
				if (UMaterialInstanceDynamic* DynamicMat =
						Character->GetMesh()->CreateDynamicMaterialInstance(SlotIndex))
				{
					BodyMaterials.Add(DynamicMat);
				}
			}
		}
		else if (UMaterialInstanceDynamic* ProxyMat = Character->BodyProxy->CreateDynamicMaterialInstance(0))
		{
			BodyMaterials.Add(ProxyMat);
		}
	}
	else
	{
		SetComponentTickEnabled(false);
	}

	// Reattach weapons that replicated before the character mesh was ready.
	if (ACombatWeapon* Weapon = Character->GetWeapon())
	{
		Weapon->Presentation->AttachToCharacter();
	}
}

void UCharacterPresentationComponent::ApplyAssets()
{
	ACombatCharacter* Character = Cast<ACombatCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	Character->FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	Character->FirstPersonCamera->bUsePawnControlRotation = true;
	Character->FirstPersonCamera->SetFieldOfView(90.f);

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	CharacterMesh->SetOwnerNoSee(true);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CharacterMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

	Character->BodyProxy->SetRelativeScale3D(FVector(0.68f, 0.68f, 1.76f));
	Character->BodyProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Character->BodyProxy->SetOwnerNoSee(true);
	Character->BodyProxy->SetStaticMesh(DefaultBodyMesh);
	Character->BodyProxy->SetMaterial(0, DefaultBodyMaterial);
	Character->BodyProxy->SetVisibility(true);
	Character->BodyProxy->SetHiddenInGame(false);

	const UCharacterDataAsset* Data = Character->CharacterData.Get();
	if (Data && !Data->TeamColorParameterName.IsNone())
	{
		ConfiguredColorParameter = Data->TeamColorParameterName;
	}

	bUsesSkeletalBody = false;
	if (Data && !Data->CharacterSkelMesh.IsNull())
	{
		if (USkeletalMesh* LoadedMesh = Data->CharacterSkelMesh.LoadSynchronous())
		{
			bUsesSkeletalBody = true;
			CharacterMesh->SetSkeletalMesh(LoadedMesh);
			CharacterMesh->SetRelativeLocation(
				FCombatRules::SanitizeVector(Data->MeshLocationOffset, FVector(0.f, 0.f, -90.f)));
			CharacterMesh->SetRelativeRotation(
				FCombatRules::SanitizeRotator(Data->MeshRotationOffset, FRotator(0.f, -90.f, 0.f)));
			// Network smoothing restores these cached offsets on simulated proxies.
			Character->CacheInitialMeshOffset(
				CharacterMesh->GetRelativeLocation(), CharacterMesh->GetRelativeRotation());
			CharacterMesh->SetVisibility(true);
			Character->BodyProxy->SetVisibility(false);
			Character->BodyProxy->SetHiddenInGame(true);
			if (GetNetMode() != NM_DedicatedServer && !Data->CharacterAnimClass.IsNull())
			{
				if (UClass* AnimClass = Data->CharacterAnimClass.LoadSynchronous())
				{
					CharacterMesh->SetAnimInstanceClass(AnimClass);
				}
			}
		}
	}
	if (!bUsesSkeletalBody)
	{
		CharacterMesh->SetSkeletalMesh(nullptr);
		CharacterMesh->SetVisibility(false);
	}
}

void UCharacterPresentationComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	const ACombatCharacter* Character = Cast<ACombatCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	const bool bDead = Character->GetCombatState() == ECombatState::Dead;
	if (Character->IsLocallyControlled())
	{
		const ACombatWeapon* Weapon = Character->GetWeapon();
		const bool bADS = Weapon && Weapon->Fire->IsADS();
		const float TargetFOV =
			bADS && !bDead ? FMath::Clamp(Character->GetCharacterStats().AimDownSightFOV, 20.f, 120.f) : 90.f;
		Character->FirstPersonCamera->SetFieldOfView(
			FMath::FInterpTo(Character->FirstPersonCamera->FieldOfView, TargetFOV, DeltaTime, 12.f));
	}
	const ACombatPlayerState* PS = Character->GetPlayerState<ACombatPlayerState>();
	const ETeamSide Team = PS ? PS->Team : ETeamSide::Neutral;
	if (bHasDisplayedTeam && Team == DisplayedTeam && bDead == bDisplayedDead)
	{
		return;
	}
	ApplyTeamVisuals(Team, bDead);
}

void UCharacterPresentationComponent::ApplyTeamVisuals(ETeamSide Team, bool bDead)
{
	bHasDisplayedTeam = true;
	DisplayedTeam = Team;
	bDisplayedDead = bDead;

	const ACombatCharacter* Character = Cast<ACombatCharacter>(GetOwner());
	if (Character && bUsesSkeletalBody)
	{
		Character->GetMesh()->bPauseAnims = bDead;
	}

	const FLinearColor Color = bDead ? FLinearColor(0.15f, 0.15f, 0.15f)
		: Team == ETeamSide::Red	 ? FLinearColor(0.85f, 0.08f, 0.08f)
		: Team == ETeamSide::Blue	 ? FLinearColor(0.08f, 0.25f, 0.92f)
									 : FLinearColor(0.55f, 0.55f, 0.55f);

	for (UMaterialInstanceDynamic* Mat : BodyMaterials)
	{
		if (!Mat)
		{
			continue;
		}
		if (!ConfiguredColorParameter.IsNone())
		{
			Mat->SetVectorParameterValue(ConfiguredColorParameter, Color);
		}
		Mat->SetVectorParameterValue(TEXT("Paint Tint"), Color);
		Mat->SetVectorParameterValue(TEXT("Tint"), Color);
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
	}
}
