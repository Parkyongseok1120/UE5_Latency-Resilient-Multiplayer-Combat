#include "Components/CharacterPresentationComponent.h"
#include "CombatCharacter.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Components/WeaponFireComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
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
	ACombatCharacter* Character = Cast<ACombatCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	Character->FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	Character->FirstPersonCamera->bUsePawnControlRotation = true;
	Character->FirstPersonCamera->SetFieldOfView(90.f);
	Character->BodyProxy->SetRelativeScale3D(FVector(0.68f, 0.68f, 1.76f));
	Character->BodyProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Character->BodyProxy->SetOwnerNoSee(true);
	Character->BodyProxy->SetStaticMesh(DefaultBodyMesh);
	Character->BodyProxy->SetMaterial(0, DefaultBodyMaterial);
	if (Character->CharacterData && !Character->CharacterData->CharacterSkelMesh.IsNull())
	{
		Character->GetMesh()->SetSkeletalMesh(Character->CharacterData->CharacterSkelMesh.LoadSynchronous());
		Character->BodyProxy->SetVisibility(false);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		BodyMaterial = Character->BodyProxy->CreateDynamicMaterialInstance(0);
	}
	else
	{
		SetComponentTickEnabled(false);
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
	if (!BodyMaterial || (bHasDisplayedTeam && Team == DisplayedTeam && bDead == bDisplayedDead))
	{
		return;
	}
	bHasDisplayedTeam = true;
	DisplayedTeam = Team;
	bDisplayedDead = bDead;
	const FLinearColor Color = bDead ? FLinearColor(0.15f, 0.15f, 0.15f)
		: Team == ETeamSide::Red	 ? FLinearColor(0.85f, 0.05f, 0.05f)
		: Team == ETeamSide::Blue	 ? FLinearColor(0.05f, 0.2f, 0.9f)
									 : FLinearColor::Gray;
	BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
}
