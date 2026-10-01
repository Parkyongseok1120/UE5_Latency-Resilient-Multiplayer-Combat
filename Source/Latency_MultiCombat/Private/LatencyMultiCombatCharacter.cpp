#include "LatencyMultiCombatCharacter.h"
#include "LatencyMultiCombatGameMode.h"
#include "LatencyMultiCombatGameState.h"
#include "LatencyMultiCombatPlayerState.h"
#include "Data/UCharacterDataAsset.h"
#include "Data/UWeaponDataAsset.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"

ALatencyMultiCombatCharacter::ALatencyMultiCombatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetOwnerNoSee(true);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(90.f);

	BodyProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyProxy"));
	BodyProxy->SetupAttachment(GetCapsuleComponent());
	BodyProxy->SetRelativeScale3D(FVector(0.68f, 0.68f, 1.76f));
	BodyProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyProxy->SetOwnerNoSee(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (BodyMesh.Succeeded()) BodyProxy->SetStaticMesh(BodyMesh.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded()) BodyProxy->SetMaterial(0, ShapeMaterial.Object);

	FirstPersonWeapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonWeapon"));
	FirstPersonWeapon->SetupAttachment(FirstPersonCamera);
	FirstPersonWeapon->SetRelativeLocation(FVector(35.f, 14.f, -12.f));
	FirstPersonWeapon->SetRelativeScale3D(FVector(0.45f, 0.08f, 0.08f));
	FirstPersonWeapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonWeapon->SetOnlyOwnerSee(true);
	FirstPersonWeapon->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WeaponMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (WeaponMesh.Succeeded()) FirstPersonWeapon->SetStaticMesh(WeaponMesh.Object);
	if (ShapeMaterial.Succeeded()) FirstPersonWeapon->SetMaterial(0, ShapeMaterial.Object);
}

void ALatencyMultiCombatCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyCharacterStats();
	ApplyWeaponStats();
	if (GetNetMode() != NM_DedicatedServer)
	{
		BodyMaterial = BodyProxy->CreateDynamicMaterialInstance(0);
		UpdateBodyColor();
	}
}

void ALatencyMultiCombatCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// PlayerState is assigned by possession, and may not exist in BeginPlay.
	if (HasAuthority())
		if (auto* PS = GetPlayerState<ALatencyMultiCombatPlayerState>())
			PS->InitializeForSpawn(CharacterData ? CharacterData->Stats : FCharacterStats());
}

FVector ALatencyMultiCombatCharacter::GetPawnViewLocation() const
{
	return FirstPersonCamera->GetComponentLocation();
}

void ALatencyMultiCombatCharacter::SetupPlayerInputComponent(UInputComponent* PIC)
{
	Super::SetupPlayerInputComponent(PIC);
	if (IA_Move && IA_Look && IA_Jump && IA_Shoot && IA_Reload && IA_ADS)
	{
		if (auto* E = Cast<UEnhancedInputComponent>(PIC))
		{
			E->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ALatencyMultiCombatCharacter::Move);
			E->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ALatencyMultiCombatCharacter::Look);
			E->BindAction(IA_Jump, ETriggerEvent::Started, this, &ALatencyMultiCombatCharacter::LegacyJump);
			E->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ALatencyMultiCombatCharacter::LegacyStopJump);
			E->BindAction(IA_Jump, ETriggerEvent::Canceled, this, &ALatencyMultiCombatCharacter::LegacyStopJump);
			E->BindAction(IA_Shoot, ETriggerEvent::Started, this, &ALatencyMultiCombatCharacter::ShootPressed);
			E->BindAction(IA_Shoot, ETriggerEvent::Completed, this, &ALatencyMultiCombatCharacter::ShootReleased);
			E->BindAction(IA_Shoot, ETriggerEvent::Canceled, this, &ALatencyMultiCombatCharacter::ShootReleased);
			E->BindAction(IA_Reload, ETriggerEvent::Started, this, &ALatencyMultiCombatCharacter::ReloadStarted);
			E->BindAction(IA_ADS, ETriggerEvent::Started, this, &ALatencyMultiCombatCharacter::ADSStarted);
			E->BindAction(IA_ADS, ETriggerEvent::Completed, this, &ALatencyMultiCombatCharacter::ADSEnded);
			E->BindAction(IA_ADS, ETriggerEvent::Canceled, this, &ALatencyMultiCombatCharacter::ADSEnded);
			return;
		}
	}
	PIC->BindAxis(TEXT("MoveForward"), this, &ALatencyMultiCombatCharacter::LegacyMoveForward);
	PIC->BindAxis(TEXT("MoveRight"), this, &ALatencyMultiCombatCharacter::LegacyMoveRight);
	PIC->BindAxis(TEXT("Turn"), this, &ALatencyMultiCombatCharacter::LegacyLookYaw);
	PIC->BindAxis(TEXT("LookUp"), this, &ALatencyMultiCombatCharacter::LegacyLookPitch);
	PIC->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ALatencyMultiCombatCharacter::LegacyJump);
	PIC->BindKey(EKeys::SpaceBar, IE_Released, this, &ALatencyMultiCombatCharacter::LegacyStopJump);
	PIC->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ALatencyMultiCombatCharacter::LegacyShoot);
	PIC->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ALatencyMultiCombatCharacter::LegacyStopShoot);
	PIC->BindKey(EKeys::R, IE_Pressed, this, &ALatencyMultiCombatCharacter::LegacyReload);
	PIC->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ALatencyMultiCombatCharacter::LegacyADSOn);
	PIC->BindKey(EKeys::RightMouseButton, IE_Released, this, &ALatencyMultiCombatCharacter::LegacyADSOff);
}

void ALatencyMultiCombatCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALatencyMultiCombatCharacter, CombatState);
	DOREPLIFETIME(ALatencyMultiCombatCharacter, bIsADS);
	DOREPLIFETIME_CONDITION(ALatencyMultiCombatCharacter, MagazineAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ALatencyMultiCombatCharacter, ReserveAmmo, COND_OwnerOnly);
}

void ALatencyMultiCombatCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsLocallyControlled())
	{
		if (bWantsToFire && WeaponStats.bFullAuto) FireWeapon();
		const float AimFOV = CharacterData ? CharacterData->Stats.AimDownSightFOV : FCharacterStats().AimDownSightFOV;
		const float TargetFOV = bIsADS && CombatState != ECombatState::Dead ? FMath::Clamp(AimFOV, 20.f, 120.f) : 90.f;
		FirstPersonCamera->SetFieldOfView(FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaTime, 12.f));
	}
	if (HasAuthority())
	{
		if (bIsReloading && GetWorld()->GetTimeSeconds() >= ReloadEndTime) StopReload();
		RefreshCombatState();
	}
	if (BodyMaterial) UpdateBodyColor();
}

void ALatencyMultiCombatCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller || CombatState == ECombatState::Dead) return;
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void ALatencyMultiCombatCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	LegacyLookYaw(Axis.X);
	LegacyLookPitch(Axis.Y);
}

void ALatencyMultiCombatCharacter::ShootPressed(const FInputActionValue&)
{
	if (CombatState == ECombatState::Dead) return;
	bWantsToFire = true;
	FireWeapon();
}

void ALatencyMultiCombatCharacter::ShootReleased(const FInputActionValue&) { bWantsToFire = false; }
void ALatencyMultiCombatCharacter::ReloadStarted(const FInputActionValue&) { StartReload(); }
void ALatencyMultiCombatCharacter::ADSStarted(const FInputActionValue&) { SetADS(true); }
void ALatencyMultiCombatCharacter::ADSEnded(const FInputActionValue&) { SetADS(false); }

bool ALatencyMultiCombatCharacter::IsMatchActive() const
{
	const auto* GS = GetWorld()->GetGameState<ALatencyMultiCombatGameState>();
	return !GS || !GS->bMatchEnded;
}

bool ALatencyMultiCombatCharacter::CanFire() const
{
	const auto* PS = GetPlayerState<ALatencyMultiCombatPlayerState>();
	return Controller && PS && !PS->bIsDead && !bIsReloading && MagazineAmmo > 0
		&& CombatState != ECombatState::Dead && CombatState != ECombatState::Reloading && IsMatchActive();
}

void ALatencyMultiCombatCharacter::FireWeapon()
{
	if (!CanFire()) return;
	if (HasAuthority())
	{
		HandleServerFire(GetBaseAimRotation());
		return;
	}
	if (!IsLocallyControlled()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastLocalFireTime < 1.f / WeaponStats.FireRate) return;
	LastLocalFireTime = Now;
	ServerFire(GetBaseAimRotation());
}

void ALatencyMultiCombatCharacter::ServerFire_Implementation(FRotator AimRotation)
{
	HandleServerFire(AimRotation);
}

void ALatencyMultiCombatCharacter::HandleServerFire(const FRotator& AimRotation)
{
	if (!HasAuthority() || !CanFire() || AimRotation.ContainsNaN()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime + KINDA_SMALL_NUMBER < 1.f / WeaponStats.FireRate) return;
	LastFireTime = Now;
	--MagazineAmmo;
	SetCombatState(ECombatState::Firing);

	const FVector Start = GetPawnViewLocation();
	const FRotator Aim(FMath::Clamp(FRotator::NormalizeAxis(AimRotation.Pitch), -89.f, 89.f),
		FRotator::NormalizeAxis(AimRotation.Yaw), 0.f);
	float Spread = WeaponStats.BaseSpread;
	if (!bIsADS) Spread *= CharacterData ? CharacterData->Stats.HipFireSpreadMultiplier : FCharacterStats().HipFireSpreadMultiplier;
	if (GetVelocity().SizeSquared2D() > 100.f) Spread *= WeaponStats.MovingSpreadMultiplier;
	const FVector Direction = FMath::VRandCone(Aim.Vector(), FMath::DegreesToRadians(FMath::Clamp(Spread, 0.f, 45.f)));
	const FVector End = Start + Direction * WeaponStats.MaxRange * 100.f;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BaselineWeaponTrace), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (bHit && Hit.GetActor())
	{
		// Weapon range/falloff are authored in meters; Unreal traces use centimeters.
		const float DistanceMeters = Hit.Distance / 100.f;
		const float Falloff = FMath::Clamp((DistanceMeters - WeaponStats.RangeFalloffStart)
			/ FMath::Max(0.01f, WeaponStats.RangeFalloffEnd - WeaponStats.RangeFalloffStart), 0.f, 1.f);
		const float Damage = WeaponStats.Damage * FMath::Lerp(1.f, WeaponStats.MinDamageRatio, Falloff);
		const float Applied = UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Direction, Hit, Controller, this, nullptr);
		if (Applied > 0.f && Cast<ALatencyMultiCombatCharacter>(Hit.GetActor())) ClientConfirmHit();
	}
	MulticastShot(Start, bHit ? Hit.ImpactPoint : End, bHit);
	ForceNetUpdate();
	if (MagazineAmmo == 0) StartReload();
}

void ALatencyMultiCombatCharacter::StartReload()
{
	if (!HasAuthority())
	{
		if (IsLocallyControlled() && CombatState != ECombatState::Dead && CombatState != ECombatState::Reloading)
			ServerStartReload();
		return;
	}
	if (bIsReloading || CombatState == ECombatState::Dead || !IsMatchActive()
		|| MagazineAmmo >= WeaponStats.MagazineSize || ReserveAmmo <= 0) return;
	bIsReloading = true;
	ReloadEndTime = GetWorld()->GetTimeSeconds() + WeaponStats.ReloadTime;
	SetCombatState(ECombatState::Reloading);
}

void ALatencyMultiCombatCharacter::ServerStartReload_Implementation() { StartReload(); }

void ALatencyMultiCombatCharacter::StopReload()
{
	if (!HasAuthority() || !bIsReloading || CombatState == ECombatState::Dead
		|| GetWorld()->GetTimeSeconds() < ReloadEndTime) return;
	const int32 Transfer = FMath::Min(WeaponStats.MagazineSize - MagazineAmmo, ReserveAmmo);
	MagazineAmmo += Transfer;
	ReserveAmmo -= Transfer;
	bIsReloading = false;
	SetCombatState(bIsADS ? ECombatState::Aiming : ECombatState::Idle);
	ForceNetUpdate();
}

void ALatencyMultiCombatCharacter::SetADS(bool bNewADS)
{
	if (CombatState == ECombatState::Dead) return;
	bIsADS = bNewADS;
	if (!HasAuthority() && IsLocallyControlled()) ServerSetADS(bNewADS);
}

void ALatencyMultiCombatCharacter::ServerSetADS_Implementation(bool bNewADS) { SetADS(bNewADS); }

void ALatencyMultiCombatCharacter::RefreshCombatState()
{
	if (CombatState == ECombatState::Dead || bIsReloading) return;
	if (GetWorld()->GetTimeSeconds() - LastFireTime < 1.f / WeaponStats.FireRate) return;
	SetCombatState(bIsADS ? ECombatState::Aiming : (GetVelocity().SizeSquared2D() > 100.f ? ECombatState::Moving : ECombatState::Idle));
}

float ALatencyMultiCombatCharacter::TakeDamage(float Damage, const FDamageEvent& Event, AController* DamageInstigator, AActor* Causer)
{
	if (!HasAuthority() || !FMath::IsFinite(Damage) || Damage <= 0.f || CombatState == ECombatState::Dead) return 0.f;
	auto* PS = GetPlayerState<ALatencyMultiCombatPlayerState>();
	const auto* GM = GetWorld()->GetAuthGameMode<ALatencyMultiCombatGameMode>();
	if (!PS || !GM || !GM->CanDamagePlayer(PS, DamageInstigator)) return 0.f;
	const float Actual = Super::TakeDamage(Damage, Event, DamageInstigator, Causer);
	if (!FMath::IsFinite(Actual) || Actual <= 0.f) return 0.f;
	PS->ApplyDamage(Actual, DamageInstigator);
	if (PS->bIsDead) Die(DamageInstigator);
	return Actual;
}

void ALatencyMultiCombatCharacter::Die(AController* Killer)
{
	if (!HasAuthority() || CombatState == ECombatState::Dead) return;
	bIsReloading = false;
	bWantsToFire = false;
	bIsADS = false;
	SetCombatState(ECombatState::Dead);
	if (auto* GM = GetWorld()->GetAuthGameMode<ALatencyMultiCombatGameMode>())
	{
		auto* KillerPS = Killer ? Killer->GetPlayerState<ALatencyMultiCombatPlayerState>() : nullptr;
		GM->HandlePlayerDeath(GetPlayerState<ALatencyMultiCombatPlayerState>(), KillerPS);
	}
}

void ALatencyMultiCombatCharacter::SetCombatState(ECombatState NewState)
{
	if (!HasAuthority() || CombatState == NewState) return;
	CombatState = NewState;
	OnRep_CombatState();
	ForceNetUpdate();
}

void ALatencyMultiCombatCharacter::OnRep_CombatState()
{
	if (CombatState == ECombatState::Dead)
	{
		bWantsToFire = false;
		bIsADS = false;
		StopJumping();
		GetCharacterMovement()->DisableMovement();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FirstPersonWeapon->SetVisibility(false);
	}
}

void ALatencyMultiCombatCharacter::MulticastShot_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, bool bHit)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 0.08f, 0, 1.f);
	if (bHit) DrawDebugPoint(GetWorld(), End, 8.f, FColor::Orange, false, 0.15f);
	if (CurrentWeapon && !CurrentWeapon->FireSound.IsNull())
		UGameplayStatics::PlaySoundAtLocation(this, CurrentWeapon->FireSound.LoadSynchronous(), Start);
}

void ALatencyMultiCombatCharacter::ClientConfirmHit_Implementation()
{
	LastHitConfirmationTime = GetWorld()->GetTimeSeconds();
}

void ALatencyMultiCombatCharacter::ApplyCharacterStats()
{
	const FCharacterStats Stats = CharacterData ? CharacterData->Stats : FCharacterStats();
	GetCharacterMovement()->MaxWalkSpeed = Stats.WalkSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = Stats.CrouchSpeed;
	GetCharacterMovement()->JumpZVelocity = Stats.JumpVelocity;
	GetCharacterMovement()->AirControl = Stats.AirControl;
	if (CharacterData && !CharacterData->CharacterSkelMesh.IsNull())
	{
		GetMesh()->SetSkeletalMesh(CharacterData->CharacterSkelMesh.LoadSynchronous());
		BodyProxy->SetVisibility(false);
	}
}

void ALatencyMultiCombatCharacter::ApplyWeaponStats()
{
	if (!CurrentWeapon && CharacterData && !CharacterData->DefaultWeapon.IsNull())
		CurrentWeapon = CharacterData->DefaultWeapon.LoadSynchronous();
	WeaponStats = CurrentWeapon ? CurrentWeapon->Stats : FWeaponStats();
	WeaponStats.FireRate = FMath::Clamp(WeaponStats.FireRate, 0.1f, 30.f);
	WeaponStats.MagazineSize = FMath::Max(1, WeaponStats.MagazineSize);
	WeaponStats.ReserveAmmo = FMath::Max(0, WeaponStats.ReserveAmmo);
	WeaponStats.ReloadTime = FMath::Max(0.01f, WeaponStats.ReloadTime);
	WeaponStats.MaxRange = FMath::Max(0.f, WeaponStats.MaxRange);
	WeaponStats.MinDamageRatio = FMath::Clamp(WeaponStats.MinDamageRatio, 0.f, 1.f);
	if (HasAuthority())
	{
		MagazineAmmo = WeaponStats.MagazineSize;
		ReserveAmmo = WeaponStats.ReserveAmmo;
	}
	if (CurrentWeapon && !CurrentWeapon->WeaponMesh.IsNull())
	{
		FirstPersonWeapon->SetStaticMesh(CurrentWeapon->WeaponMesh.LoadSynchronous());
		FirstPersonWeapon->SetRelativeScale3D(FVector::OneVector);
	}
}

void ALatencyMultiCombatCharacter::UpdateBodyColor()
{
	const auto* PS = GetPlayerState<ALatencyMultiCombatPlayerState>();
	const ETeamSide Team = PS ? PS->Team : ETeamSide::Neutral;
	const bool bDead = CombatState == ECombatState::Dead;
	if (!BodyMaterial || (Team == DisplayedTeam && bDead == bDisplayedDead)) return;
	DisplayedTeam = Team;
	bDisplayedDead = bDead;
	const FLinearColor Color = bDead ? FLinearColor(0.15f, 0.15f, 0.15f)
		: Team == ETeamSide::Red ? FLinearColor(0.85f, 0.05f, 0.05f)
		: Team == ETeamSide::Blue ? FLinearColor(0.05f, 0.2f, 0.9f) : FLinearColor::Gray;
	BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
}

void ALatencyMultiCombatCharacter::LegacyMoveForward(float Value)
{
	if (Controller && CombatState != ECombatState::Dead)
		AddMovementInput(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector(), Value);
}
void ALatencyMultiCombatCharacter::LegacyMoveRight(float Value)
{
	if (Controller && CombatState != ECombatState::Dead)
		AddMovementInput(FRotationMatrix(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::Y), Value);
}
void ALatencyMultiCombatCharacter::LegacyLookYaw(float Value) { AddControllerYawInput(Value); }
void ALatencyMultiCombatCharacter::LegacyLookPitch(float Value) { AddControllerPitchInput(Value); }
void ALatencyMultiCombatCharacter::LegacyJump() { if (CombatState != ECombatState::Dead) Jump(); }
void ALatencyMultiCombatCharacter::LegacyStopJump() { StopJumping(); }
void ALatencyMultiCombatCharacter::LegacyShoot() { ShootPressed(FInputActionValue()); }
void ALatencyMultiCombatCharacter::LegacyStopShoot() { ShootReleased(FInputActionValue()); }
void ALatencyMultiCombatCharacter::LegacyReload() { StartReload(); }
void ALatencyMultiCombatCharacter::LegacyADSOn() { SetADS(true); }
void ALatencyMultiCombatCharacter::LegacyADSOff() { SetADS(false); }
