#include "Components/WeaponPresentationComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "CombatCharacter.h"
#include "CombatWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Data/CombatRules.h"
#include "Data/UCharacterDataAsset.h"
#include "Data/UWeaponDataAsset.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

UWeaponPresentationComponent::UWeaponPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	DefaultMesh = Mesh.Object;
	DefaultMaterial = Material.Object;
}

void UWeaponPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	if (ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner()))
	{
		Weapon->Fire->OnStateChanged.AddUObject(this, &UWeaponPresentationComponent::HandleCombatStateChanged);
		HandleCombatStateChanged(Weapon->Fire->GetCombatState());
	}
}

void UWeaponPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner()))
	{
		Weapon->Fire->OnStateChanged.RemoveAll(this);
	}
	Super::EndPlay(Reason);
}

void UWeaponPresentationComponent::ApplyAssets()
{
	ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	if (!Weapon)
	{
		return;
	}
	const UWeaponDataAsset* Data = Weapon->WeaponData.Get();
	USkeletalMesh* LoadedSkeletal =
		Data && !Data->WeaponSkeletalMesh.IsNull() ? Data->WeaponSkeletalMesh.LoadSynchronous() : nullptr;
	bUsesSkeletalWeapon = LoadedSkeletal != nullptr;

	if (bUsesSkeletalWeapon)
	{
		Weapon->FirstPersonMesh->SetSkeletalMesh(LoadedSkeletal);
		Weapon->ThirdPersonMesh->SetSkeletalMesh(LoadedSkeletal);
		Weapon->FirstPersonMesh->SetRelativeScale3D(FVector::OneVector);
		Weapon->ThirdPersonMesh->SetRelativeScale3D(FVector::OneVector);
		Weapon->FirstPersonMesh->SetRelativeLocation(
			FCombatRules::SanitizeVector(Data->FirstPersonLocationOffset, FVector(38.f, 16.f, -18.f)));
		Weapon->FirstPersonMesh->SetRelativeRotation(
			FCombatRules::SanitizeRotator(Data->FirstPersonRotationOffset, FRotator(0.f, -90.f, 0.f)));
		Weapon->WeaponMesh->SetStaticMesh(nullptr);
		Weapon->ThirdPersonStaticMesh->SetStaticMesh(nullptr);
	}
	else
	{
		Weapon->FirstPersonMesh->SetSkeletalMesh(nullptr);
		Weapon->ThirdPersonMesh->SetSkeletalMesh(nullptr);
		const bool bCustomStatic = Data && !Data->WeaponMesh.IsNull();
		UStaticMesh* StaticAsset = bCustomStatic ? Data->WeaponMesh.LoadSynchronous() : DefaultMesh.Get();
		Weapon->WeaponMesh->SetStaticMesh(StaticAsset);
		Weapon->ThirdPersonStaticMesh->SetStaticMesh(StaticAsset);
		if (!bCustomStatic)
		{
			Weapon->WeaponMesh->SetMaterial(0, DefaultMaterial);
			Weapon->ThirdPersonStaticMesh->SetMaterial(0, DefaultMaterial);
		}
		const FVector FallbackScale = bCustomStatic ? FVector::OneVector : FVector(0.45f, 0.08f, 0.08f);
		Weapon->WeaponMesh->SetRelativeScale3D(FallbackScale);
		Weapon->ThirdPersonStaticMesh->SetRelativeScale3D(FallbackScale);
		const FVector FirstPersonLoc = bCustomStatic && Data
			? FCombatRules::SanitizeVector(Data->FirstPersonLocationOffset, FVector(35.f, 14.f, -12.f))
			: FVector(35.f, 14.f, -12.f);
		const FRotator FirstPersonRot = bCustomStatic && Data
			? FCombatRules::SanitizeRotator(Data->FirstPersonRotationOffset, FRotator::ZeroRotator)
			: FRotator::ZeroRotator;
		Weapon->WeaponMesh->SetRelativeLocation(FirstPersonLoc);
		Weapon->WeaponMesh->SetRelativeRotation(FirstPersonRot);
	}

	FireSound = Data && !Data->FireSound.IsNull() && GetNetMode() != NM_DedicatedServer
		? Data->FireSound.LoadSynchronous()
		: nullptr;
	CharacterFireMontage = nullptr;
	CharacterReloadMontage = nullptr;
	bReloadPresentation = false;
	if (Data && GetNetMode() != NM_DedicatedServer && !Data->CharacterAnimationSlot.IsNone())
	{
		if (UAnimSequence* FireAnimation = Data->CharacterFireAnimation.LoadSynchronous())
		{
			CharacterFireMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
				FireAnimation, Data->CharacterAnimationSlot, 0.025f, 0.08f);
		}
		if (UAnimSequence* ReloadAnimation = Data->CharacterReloadAnimation.LoadSynchronous())
		{
			CharacterReloadMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
				ReloadAnimation, Data->CharacterAnimationSlot, 0.08f, 0.12f);
		}
	}
	RecoilWeight = 0.f;
	SetComponentTickEnabled(false);
	FirstPersonRestLocation = bUsesSkeletalWeapon ? Weapon->FirstPersonMesh->GetRelativeLocation()
												  : Weapon->WeaponMesh->GetRelativeLocation();
	FirstPersonRestRotation = bUsesSkeletalWeapon ? Weapon->FirstPersonMesh->GetRelativeRotation()
												  : Weapon->WeaponMesh->GetRelativeRotation();

	AttachToCharacter();
	HandleCombatStateChanged(Weapon->Fire->GetCombatState());
}

FName UWeaponPresentationComponent::ResolveGripSocket(
	const ACombatCharacter* Character, const UWeaponDataAsset* Data) const
{
	const USkeletalMeshComponent* BodyMesh = Character ? Character->GetMesh() : nullptr;
	if (!BodyMesh || !BodyMesh->GetSkeletalMeshAsset())
	{
		return NAME_None;
	}
	if (Data && !Data->ThirdPersonAttachSocketName.IsNone() &&
		BodyMesh->DoesSocketExist(Data->ThirdPersonAttachSocketName))
	{
		return Data->ThirdPersonAttachSocketName;
	}
	const UCharacterDataAsset* CharData = Character->CharacterData.Get();
	if (CharData && !CharData->WeaponAttachSocketName.IsNone() &&
		BodyMesh->DoesSocketExist(CharData->WeaponAttachSocketName))
	{
		return CharData->WeaponAttachSocketName;
	}
	if (BodyMesh->DoesSocketExist(TEXT("HandGrip_R")))
	{
		return TEXT("HandGrip_R");
	}
	if (BodyMesh->DoesSocketExist(TEXT("hand_r")))
	{
		return TEXT("hand_r");
	}
	return NAME_None;
}

void UWeaponPresentationComponent::AttachToCharacter()
{
	ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	ACombatCharacter* Character = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	if (!Weapon || !Character)
	{
		return;
	}
	if (Weapon->GetRootComponent()->GetAttachParent() != Character->FirstPersonCamera)
	{
		Weapon->AttachToComponent(
			Character->FirstPersonCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	// Keep the raised eye line above the view weapon and its attached hands.
	Weapon->SetActorRelativeLocation(FVector(0.f, 0.f, -4.f));

	const UWeaponDataAsset* Data = Weapon->WeaponData.Get();
	USkeletalMeshComponent* BodyMesh = Character->GetMesh();
	if (BodyMesh && BodyMesh->GetSkeletalMeshAsset())
	{
		const FName GripSocket = ResolveGripSocket(Character, Data);
		Weapon->ThirdPersonMesh->AttachToComponent(
			BodyMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocket);
		Weapon->ThirdPersonStaticMesh->AttachToComponent(
			BodyMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocket);
		const FVector LocOffset = Data
			? FCombatRules::SanitizeVector(Data->ThirdPersonLocationOffset, FVector::ZeroVector)
			: FVector::ZeroVector;
		const FRotator RotOffset = Data
			? FCombatRules::SanitizeRotator(Data->ThirdPersonRotationOffset, FRotator::ZeroRotator)
			: FRotator::ZeroRotator;
		Weapon->ThirdPersonMesh->SetRelativeLocation(LocOffset);
		Weapon->ThirdPersonMesh->SetRelativeRotation(RotOffset);
		Weapon->ThirdPersonStaticMesh->SetRelativeLocation(LocOffset);
		Weapon->ThirdPersonStaticMesh->SetRelativeRotation(RotOffset);
	}
	else
	{
		Weapon->ThirdPersonMesh->AttachToComponent(
			Character->GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Weapon->ThirdPersonStaticMesh->AttachToComponent(
			Character->GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Weapon->ThirdPersonMesh->SetRelativeLocation(FVector(28.f, 18.f, 20.f));
		Weapon->ThirdPersonMesh->SetRelativeRotation(FRotator::ZeroRotator);
		Weapon->ThirdPersonStaticMesh->SetRelativeLocation(FVector(28.f, 18.f, 20.f));
		Weapon->ThirdPersonStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

void UWeaponPresentationComponent::HandleCombatStateChanged(ECombatState NewState)
{
	UpdateVisibility(NewState != ECombatState::Dead);
	ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	if (!Weapon)
	{
		return;
	}
	ACombatCharacter* Character = Weapon->GetOwningCharacter();
	UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (NewState == ECombatState::Reloading && !bReloadPresentation)
	{
		bReloadPresentation = true;
		ReloadVisualStartedAt = GetWorld()->GetTimeSeconds();
		ReloadVisualDuration = FMath::Max(0.1f, Weapon->GetStats().ReloadTime);
		if (Anim && CharacterReloadMontage)
		{
			Anim->Montage_Play(CharacterReloadMontage, CharacterReloadMontage->GetPlayLength() / ReloadVisualDuration);
		}
		SetComponentTickEnabled(true);
	}
	else if (NewState != ECombatState::Reloading && bReloadPresentation)
	{
		bReloadPresentation = false;
		if (Anim && CharacterReloadMontage)
		{
			Anim->Montage_Stop(0.10f, CharacterReloadMontage);
		}
		if (Character && Character->IsLocallyControlled())
		{
			ApplyFirstPersonRecoil();
		}
	}
}

void UWeaponPresentationComponent::UpdateVisibility(bool bVisible)
{
	ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	if (!Weapon)
	{
		return;
	}
	Weapon->FirstPersonMesh->SetVisibility(bVisible && bUsesSkeletalWeapon);
	Weapon->ThirdPersonMesh->SetVisibility(bVisible && bUsesSkeletalWeapon);
	Weapon->WeaponMesh->SetVisibility(bVisible && !bUsesSkeletalWeapon);
	Weapon->ThirdPersonStaticMesh->SetVisibility(bVisible && !bUsesSkeletalWeapon);
}

FVector UWeaponPresentationComponent::ResolveVisualShotOrigin(const FVector& AuthoritativeStart) const
{
	const ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	const ACombatCharacter* Character = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	if (!Weapon || !Character)
	{
		return AuthoritativeStart;
	}
	const bool bLocalViewer = Character->IsLocallyControlled();
	const UWeaponDataAsset* Data = Weapon->WeaponData.Get();
	const FName SocketName = Data && !Data->MuzzleSocketName.IsNone() ? Data->MuzzleSocketName : FName(TEXT("Muzzle"));
	if (bUsesSkeletalWeapon)
	{
		const USkeletalMeshComponent* ActiveMesh = bLocalViewer ? Weapon->FirstPersonMesh : Weapon->ThirdPersonMesh;
		if (ActiveMesh && ActiveMesh->GetSkeletalMeshAsset())
		{
			if (ActiveMesh->DoesSocketExist(SocketName))
			{
				return ActiveMesh->GetSocketLocation(SocketName);
			}
			if (ActiveMesh->DoesSocketExist(TEXT("MuzzleFlash")))
			{
				return ActiveMesh->GetSocketLocation(TEXT("MuzzleFlash"));
			}
			return ActiveMesh->GetComponentLocation();
		}
	}
	const UStaticMeshComponent* FallbackMesh = bLocalViewer ? Weapon->WeaponMesh : Weapon->ThirdPersonStaticMesh;
	return FallbackMesh ? FallbackMesh->GetComponentLocation() : AuthoritativeStart;
}

void UWeaponPresentationComponent::PlayShot(const FVector& Start, const FVector& End, bool bHit)
{
	ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	ACombatCharacter* Character = Weapon ? Weapon->GetOwningCharacter() : nullptr;
	if (!Character || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance(); Anim && CharacterFireMontage)
	{
		Anim->Montage_Play(CharacterFireMontage);
	}
	if (Character->IsLocallyControlled())
	{
		RecoilWeight = 1.f;
		SetComponentTickEnabled(true);
		ApplyFirstPersonRecoil();
	}
	const FVector VisualOrigin = ResolveVisualShotOrigin(Start);
	DrawDebugLine(GetWorld(), VisualOrigin, End, FColor::Yellow, false, 0.08f, 0, 1.f);
	if (bHit)
	{
		DrawDebugPoint(GetWorld(), End, 8.f, FColor::Orange, false, 0.15f);
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, VisualOrigin);
	}
}

void UWeaponPresentationComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	RecoilWeight = FMath::FInterpTo(RecoilWeight, 0.f, DeltaTime, 18.f);
	if (RecoilWeight < 0.001f)
	{
		RecoilWeight = 0.f;
		SetComponentTickEnabled(bReloadPresentation);
	}
	ApplyFirstPersonRecoil();
}

void UWeaponPresentationComponent::ApplyFirstPersonRecoil()
{
	ACombatWeapon* Weapon = Cast<ACombatWeapon>(GetOwner());
	if (!Weapon)
	{
		return;
	}
	USceneComponent* ViewMesh = bUsesSkeletalWeapon ? static_cast<USceneComponent*>(Weapon->FirstPersonMesh.Get())
													: static_cast<USceneComponent*>(Weapon->WeaponMesh.Get());
	const FQuat RecoilRotation = FRotator(2.f * RecoilWeight, 0.f, 0.f).Quaternion();
	const float Progress = bReloadPresentation
		? FMath::Clamp((GetWorld()->GetTimeSeconds() - ReloadVisualStartedAt) / ReloadVisualDuration, 0.f, 1.f)
		: 0.f;
	const float ReloadWeight = bReloadPresentation ? FMath::Sin(Progress * PI) : 0.f;
	const FVector ReloadOffset(-3.f * ReloadWeight, -4.f * ReloadWeight, 2.f * ReloadWeight);
	const FQuat ReloadRotation = FRotator(12.f * ReloadWeight, -15.f * ReloadWeight, 30.f * ReloadWeight).Quaternion();
	ViewMesh->SetRelativeLocationAndRotation(
		FirstPersonRestLocation + ReloadOffset + FVector(-3.f, 0.f, 0.7f) * RecoilWeight,
		ReloadRotation * RecoilRotation * FirstPersonRestRotation.Quaternion());
}

void UWeaponPresentationComponent::ConfirmHit()
{
	LastHitConfirmationTime = GetWorld()->GetTimeSeconds();
}
