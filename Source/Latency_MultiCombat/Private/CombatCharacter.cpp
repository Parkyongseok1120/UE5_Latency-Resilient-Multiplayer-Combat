#include "CombatCharacter.h"
#include "Components/RewindHistoryComponent.h"
#include "CombatGameMode.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Components/CharacterPresentationComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Components/WeaponAmmoComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Data/CombatRules.h"
#include "Components/CombatInputComponent.h"
#include "Components/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Data/UCharacterDataAsset.h"

ACombatCharacter::ACombatCharacter()
{
	static ConstructorHelpers::FObjectFinderOptional<UCharacterDataAsset> DefaultCharacterData(
		TEXT("/Game/Data/DA_DefaultCharacter.DA_DefaultCharacter"), LOAD_NoWarn | LOAD_Quiet);
	CharacterData = DefaultCharacterData.Get();
	bReplicates = true;
	SetReplicateMovement(true);
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetRelativeLocationAndRotation(
		CharacterData ? CharacterData->MeshLocationOffset : FVector(0.f, 0.f, -90.f),
		CharacterData ? CharacterData->MeshRotationOffset : FRotator(0.f, -90.f, 0.f));
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(FirstPersonCamera);
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonArms->SetCastShadow(false);
	FirstPersonArms->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonArms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	BodyProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyProxy"));
	BodyProxy->SetupAttachment(GetCapsuleComponent());
	Presentation = CreateDefaultSubobject<UCharacterPresentationComponent>(TEXT("Presentation"));
	CombatInput = CreateDefaultSubobject<UCombatInputComponent>(TEXT("CombatInput"));
	RewindHistory = CreateDefaultSubobject<URewindHistoryComponent>(TEXT("RewindHistory"));
	WeaponClass = ACombatWeapon::StaticClass();
}

void ACombatCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Presentation->ApplyAssets();
}

void ACombatCharacter::BeginPlay()
{
	if (HasAuthority())
	{
		LifeId = FGuid::NewGuid();
	}
	Super::BeginPlay();
	const FCharacterStats Stats = GetCharacterStats();
	GetCharacterMovement()->MaxWalkSpeed = Stats.WalkSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = Stats.CrouchSpeed;
	GetCharacterMovement()->JumpZVelocity = Stats.JumpVelocity;
	GetCharacterMovement()->AirControl = Stats.AirControl;
	if (HasAuthority() && WeaponClass)
	{
		Weapon = GetWorld()->SpawnActorDeferred<ACombatWeapon>(WeaponClass, FirstPersonCamera->GetComponentTransform(),
			this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Weapon)
		{
			Weapon->WeaponData = FCombatRules::ResolveWeaponData(Weapon->WeaponData, CharacterData);
			UGameplayStatics::FinishSpawningActor(Weapon, FirstPersonCamera->GetComponentTransform());
			Weapon->AttachToComponent(FirstPersonCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			OnRep_Weapon();
		}
	}
}

void ACombatCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (ACombatWeapon* Previous = BoundWeapon.Get())
	{
		Previous->Fire->OnStateChanged.RemoveAll(this);
	}
	if (HasAuthority() && IsValid(Weapon))
	{
		Weapon->Destroy();
	}
	Super::EndPlay(Reason);
}

void ACombatCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACombatCharacter, Weapon);
	DOREPLIFETIME(ACombatCharacter, LifeId);
}

void ACombatCharacter::OnRep_Weapon()
{
	if (ACombatWeapon* Previous = BoundWeapon.Get())
	{
		Previous->Fire->OnStateChanged.RemoveAll(this);
	}
	BoundWeapon = Weapon;
	if (Weapon)
	{
		Weapon->Presentation->AttachToCharacter();
		Weapon->Fire->OnStateChanged.AddUObject(this, &ACombatCharacter::HandleCombatStateChanged);
		HandleCombatStateChanged(Weapon->Fire->GetCombatState());
	}
}

void ACombatCharacter::HandleCombatStateChanged(ECombatState State)
{
	if (State != ECombatState::Dead)
	{
		return;
	}
	StopJumping();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Presentation->StartDeathRagdoll();
}

void ACombatCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (HasAuthority())
	{
		if (ACombatPlayerState* PS = GetPlayerState<ACombatPlayerState>())
		{
			PS->Health->InitializeForSpawn(GetCharacterStats());
			RewindHistory->ResetHistory();
		}
	}
}

FCharacterStats ACombatCharacter::GetCharacterStats() const
{
	return FCombatRules::ReadCharacterStats(CharacterData);
}
FVector ACombatCharacter::GetPawnViewLocation() const
{
	return FirstPersonCamera->GetComponentLocation();
}
ECombatState ACombatCharacter::GetCombatState() const
{
	return Weapon ? Weapon->Fire->GetCombatState() : ECombatState::Idle;
}
float ACombatCharacter::TakeDamage(
	float Damage, const FDamageEvent& Event, AController* DamageInstigator, AActor* Causer)
{
	if (!HasAuthority() || !FMath::IsFinite(Damage) || Damage <= 0.f || bDeathHandled)
	{
		return 0.f;
	}

	ACombatPlayerState* PS = GetPlayerState<ACombatPlayerState>();
	const ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>();

	if (!PS || !GM || !GM->CanDamagePlayer(PS, DamageInstigator))
	{
		return 0.f;
	}

	const float Actual = Super::TakeDamage(Damage, Event, DamageInstigator, Causer);

	if (!FMath::IsFinite(Actual) || Actual <= 0.f)
	{
		return 0.f;
	}

	PS->Health->ApplyDamage(Actual);

	if (PS->Health->IsDead())
	{
		Die(DamageInstigator);
	}
	return Actual;
}

void ACombatCharacter::Die(AController* Killer)
{
	if (!HasAuthority() || bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;
	if (Weapon)
	{
		Weapon->Fire->MarkDead();
	}
	else
	{
		HandleCombatStateChanged(ECombatState::Dead);
	}
	RewindHistory->RecordNow();
	if (ACombatGameMode* GM = GetWorld()->GetAuthGameMode<ACombatGameMode>())
	{
		ACombatPlayerState* KillerPS = Killer ? Killer->GetPlayerState<ACombatPlayerState>() : nullptr;
		GM->HandlePlayerDeath(GetPlayerState<ACombatPlayerState>(), KillerPS);
	}
}

void ACombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CombatInput->BindInput(PlayerInputComponent);
}
