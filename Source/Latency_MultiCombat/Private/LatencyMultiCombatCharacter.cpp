// Latency_MultiCombat - FPS Character Implementation
#include "LatencyMultiCombatCharacter.h"
#include "LatencyMultiCombatPlayerState.h"
#include "LatencyMultiCombatGameMode.h"
#include "Data/UCharacterDataAsset.h"
#include "Data/UWeaponDataAsset.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DamageEvents.h"
#include "CollisionQueryParams.h"
#include "Net/UnrealNetwork.h"

ALatencyMultiCombatCharacter::ALatencyMultiCombatCharacter()
	: CombatState(ECombatState::Idle), MagazineAmmo(30), ReserveAmmo(90)
	, LastFireTime(0.f), bIsReloading(false), ReloadEndTime(0.f), bWantsToFire(false), bIsADS(false)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true; SetReplicateMovement(true);
	GetCharacterMovement()->JumpZVelocity = 420.f;
}
void ALatencyMultiCombatCharacter::BeginPlay() { Super::BeginPlay(); ApplyCharacterStats(); ApplyWeaponStats(); }

void ALatencyMultiCombatCharacter::SetupPlayerInputComponent(UInputComponent* PIC)
{
	Super::SetupPlayerInputComponent(PIC);
	auto* E = Cast<UEnhancedInputComponent>(PIC); if (!E) return;
	if (IA_Move)   E->BindAction(IA_Move,   ETriggerEvent::Triggered, this, &ALatencyMultiCombatCharacter::Move);
	if (IA_Look)   E->BindAction(IA_Look,   ETriggerEvent::Triggered, this, &ALatencyMultiCombatCharacter::Look);
	if (IA_Jump)   E->BindAction(IA_Jump,   ETriggerEvent::Triggered, this, &ALatencyMultiCombatCharacter::Jump);
	if (IA_Shoot)  { E->BindAction(IA_Shoot, ETriggerEvent::Started, this, &ALatencyMultiCombatCharacter::ShootPressed);
	                 E->BindAction(IA_Shoot, ETriggerEvent::Completed, this, &ALatencyMultiCombatCharacter::ShootReleased); }
	if (IA_Reload) E->BindAction(IA_Reload, ETriggerEvent::Started, this, &ALatencyMultiCombatCharacter::ReloadStarted);
	if (IA_ADS)    { E->BindAction(IA_ADS,   ETriggerEvent::Started, this, &ALatencyMultiCombatCharacter::ADSStarted);
	                 E->BindAction(IA_ADS,   ETriggerEvent::Completed, this, &ALatencyMultiCombatCharacter::ADSEnded); }
}

void ALatencyMultiCombatCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ALatencyMultiCombatCharacter, CombatState); }

void ALatencyMultiCombatCharacter::Tick(float DT)
{
	Super::Tick(DT);
	if (bWantsToFire && CurrentWeapon && CurrentWeapon->Stats.bFullAuto) FireWeapon();
	if (bIsReloading && GetWorld()->GetTimeSeconds() >= ReloadEndTime) StopReload();
}

void ALatencyMultiCombatCharacter::Move(const FInputActionValue& V)
{
	const FVector2D M = V.Get<FVector2D>(); if (!Controller) return;
	const FRotator YR(0, Controller->GetControlRotation().Yaw, 0);
	AddMovementInput(YR.Vector(), M.Y);
	AddMovementInput(FRotationMatrix(YR).GetUnitAxis(EAxis::X), M.X);
	if (CombatState == ECombatState::Idle) SetCombatState(ECombatState::Moving);
}

void ALatencyMultiCombatCharacter::Look(const FInputActionValue& V)
{ const FVector2D L = V.Get<FVector2D>(); AddControllerYawInput(L.X*2.f); AddControllerPitchInput(L.Y*2.f); }

void ALatencyMultiCombatCharacter::Jump() { Super::Jump(); }
void ALatencyMultiCombatCharacter::ShootPressed(const FInputActionValue&) { bWantsToFire = true; FireWeapon(); }
void ALatencyMultiCombatCharacter::ShootReleased(const FInputActionValue&)
{ bWantsToFire = false; if (CombatState == ECombatState::Firing) SetCombatState(ECombatState::Idle); }
void ALatencyMultiCombatCharacter::ReloadStarted(const FInputActionValue&) { StartReload(); }
void ALatencyMultiCombatCharacter::ADSStarted(const FInputActionValue&) { bIsADS = true; SetCombatState(ECombatState::Aiming); }
void ALatencyMultiCombatCharacter::ADSEnded(const FInputActionValue&)
{ bIsADS = false; if (CombatState == ECombatState::Aiming) SetCombatState(ECombatState::Idle); }

bool ALatencyMultiCombatCharacter::CanFire() const
{ return !bIsReloading && MagazineAmmo > 0 && CombatState != ECombatState::Dead; }

void ALatencyMultiCombatCharacter::FireWeapon()
{
	if (!CanFire() || !CurrentWeapon) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastFireTime < 1.f / FMath::Max(0.1f, CurrentWeapon->Stats.FireRate)) return;
	LastFireTime = Now; MagazineAmmo--; SetCombatState(ECombatState::Firing);
	FVector S = GetMuzzleLocation(), E2 = S + GetActorForwardVector() * CurrentWeapon->Stats.MaxRange * 100.f;
	AActor* Hit = nullptr;
	if (TraceHit(S, E2, Hit)) { if (auto* HC = Cast<ACharacter>(Hit)) HC->TakeDamage(CurrentWeapon->Stats.Damage, FDamageEvent(), GetController(), this); }
	if (MagazineAmmo <= 0) StartReload();
}

void ALatencyMultiCombatCharacter::StartReload()
{
	if (bIsReloading || !CurrentWeapon || MagazineAmmo >= CurrentWeapon->Stats.MagazineSize) return;
	bIsReloading = true; ReloadEndTime = GetWorld()->GetTimeSeconds() + CurrentWeapon->Stats.ReloadTime;
	SetCombatState(ECombatState::Reloading);
}

void ALatencyMultiCombatCharacter::StopReload()
{
	if (!CurrentWeapon) return;
	int32 T = FMath::Min(CurrentWeapon->Stats.MagazineSize - MagazineAmmo, ReserveAmmo);
	MagazineAmmo += T; ReserveAmmo -= T; bIsReloading = false; SetCombatState(ECombatState::Idle);
}

float ALatencyMultiCombatCharacter::TakeDamage(float Dmg, FDamageEvent const& DamageEvent, AController* Inst, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(Dmg, DamageEvent, Inst, DamageCauser);
	auto* PS = GetPlayerState<ALatencyMultiCombatPlayerState>();
	if (PS) { PS->ApplyDamage(ActualDamage, Inst); if (PS->bIsDead) Die(Inst); }
	return ActualDamage;
}

void ALatencyMultiCombatCharacter::Die(AController* Killer)
{
	SetCombatState(ECombatState::Dead); bWantsToFire = false;
	auto* MyPS = GetPlayerState<ALatencyMultiCombatPlayerState>();
	auto* GM = Cast<ALatencyMultiCombatGameMode>(GetWorld()->GetAuthGameMode());
	if (GM && MyPS) { auto* KPS = Killer ? Cast<ALatencyMultiCombatPlayerState>(Killer->GetPlayerState<ALatencyMultiCombatPlayerState>()) : nullptr; GM->HandlePlayerDeath(MyPS, KPS); }
	GetCharacterMovement()->DisableMovement();
}

void ALatencyMultiCombatCharacter::SetCombatState(ECombatState S) { if (CombatState != S) CombatState = S; }

void ALatencyMultiCombatCharacter::ApplyCharacterStats()
{
	if (!CharacterData) return;
	const auto& S = CharacterData->Stats;
	GetCharacterMovement()->MaxWalkSpeed = S.WalkSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = S.CrouchSpeed;
	GetCharacterMovement()->JumpZVelocity = S.JumpVelocity;
}

void ALatencyMultiCombatCharacter::ApplyWeaponStats()
{ if (CurrentWeapon) { MagazineAmmo = CurrentWeapon->Stats.MagazineSize; ReserveAmmo = CurrentWeapon->Stats.ReserveAmmo; } }

FVector ALatencyMultiCombatCharacter::GetMuzzleLocation() const
{ return GetActorLocation() + GetActorForwardVector()*30.f + FVector(0,0,60.f); }

bool ALatencyMultiCombatCharacter::TraceHit(FVector S, FVector E, AActor*& Out) const
{
	FCollisionQueryParams P(SCENE_QUERY_STAT(WeaponTrace), false, this); FHitResult H;
	if (GetWorld()->LineTraceSingleByChannel(H, S, E, ECC_Visibility, P)) { Out = H.GetActor(); return true; }
	return false;
}
