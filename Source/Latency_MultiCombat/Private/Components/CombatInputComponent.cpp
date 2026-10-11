#include "Components/CombatInputComponent.h"
#include "CombatCharacter.h"
#include "CombatWeapon.h"
#include "Components/WeaponFireComponent.h"
#include "Components/WeaponAmmoComponent.h"
#include "Data/CombatInputDataAsset.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

UCombatInputComponent::UCombatInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatInputComponent::BindInput(UInputComponent* PlayerInputComponent)
{
	ReleaseBindings();
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	ACombatCharacter* Character = GetCharacter();
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer()
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())
		: nullptr;
	if (!EnhancedInput || !Subsystem)
	{
		return;
	}
	ActiveInput = InputData;
	if (!ActiveInput || !ActiveInput->IsComplete())
	{
		if (InputData)
		{
			UE_LOG(LogTemp, Warning, TEXT("Incomplete combat input asset on %s; using built-in mappings"),
				*GetOwner()->GetName());
		}
		ActiveInput = NewObject<UCombatInputDataAsset>(this);
		ActiveInput->CreateDefaultMappings();
	}
	BoundInput = EnhancedInput;
	InputSubsystem = Subsystem;
	Subsystem->AddMappingContext(ActiveInput->MappingContext, 0);
	EnhancedInput->BindAction(ActiveInput->Move, ETriggerEvent::Triggered, this, &UCombatInputComponent::Move);
	EnhancedInput->BindAction(ActiveInput->Look, ETriggerEvent::Triggered, this, &UCombatInputComponent::Look);
	EnhancedInput->BindAction(ActiveInput->Jump, ETriggerEvent::Started, this, &UCombatInputComponent::JumpStarted);
	EnhancedInput->BindAction(ActiveInput->Jump, ETriggerEvent::Completed, this, &UCombatInputComponent::JumpEnded);
	EnhancedInput->BindAction(ActiveInput->Jump, ETriggerEvent::Canceled, this, &UCombatInputComponent::JumpEnded);
	EnhancedInput->BindAction(ActiveInput->Fire, ETriggerEvent::Started, this, &UCombatInputComponent::FireStarted);
	EnhancedInput->BindAction(ActiveInput->Fire, ETriggerEvent::Completed, this, &UCombatInputComponent::FireEnded);
	EnhancedInput->BindAction(ActiveInput->Fire, ETriggerEvent::Canceled, this, &UCombatInputComponent::FireEnded);
	EnhancedInput->BindAction(ActiveInput->Reload, ETriggerEvent::Started, this, &UCombatInputComponent::ReloadStarted);
	EnhancedInput->BindAction(ActiveInput->ADS, ETriggerEvent::Started, this, &UCombatInputComponent::ADSStarted);
	EnhancedInput->BindAction(ActiveInput->ADS, ETriggerEvent::Completed, this, &UCombatInputComponent::ADSEnded);
	EnhancedInput->BindAction(ActiveInput->ADS, ETriggerEvent::Canceled, this, &UCombatInputComponent::ADSEnded);
}

void UCombatInputComponent::ReleaseBindings()
{
	if (UEnhancedInputComponent* Input = BoundInput.Get())
	{
		Input->ClearBindingsForObject(this);
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = InputSubsystem.Get(); Subsystem && ActiveInput)
	{
		Subsystem->RemoveMappingContext(ActiveInput->MappingContext);
	}
	BoundInput.Reset();
	InputSubsystem.Reset();
	ActiveInput = nullptr;
	FireEnded();
	ADSEnded();
}

void UCombatInputComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseBindings();
	Super::EndPlay(Reason);
}

ACombatCharacter* UCombatInputComponent::GetCharacter() const
{
	return Cast<ACombatCharacter>(GetOwner());
}
ACombatWeapon* UCombatInputComponent::GetWeapon() const
{
	const ACombatCharacter* Character = GetCharacter();
	return Character ? Character->GetWeapon() : nullptr;
}

void UCombatInputComponent::Move(const FInputActionValue& Value)
{
	ACombatCharacter* Character = GetCharacter();
	if (!Character || !Character->GetController() || Character->GetCombatState() == ECombatState::Dead)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, Character->GetController()->GetControlRotation().Yaw, 0.f);
	Character->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	Character->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}
void UCombatInputComponent::Look(const FInputActionValue& Value)
{
	if (ACombatCharacter* Character = GetCharacter())
	{
		const FVector2D Axis = Value.Get<FVector2D>();
		Character->AddControllerYawInput(Axis.X);
		Character->AddControllerPitchInput(Axis.Y);
	}
}
void UCombatInputComponent::JumpStarted()
{
	if (ACombatCharacter* Character = GetCharacter(); Character && Character->GetCombatState() != ECombatState::Dead)
	{
		Character->Jump();
	}
}
void UCombatInputComponent::JumpEnded()
{
	if (ACombatCharacter* Character = GetCharacter())
	{
		Character->StopJumping();
	}
}
void UCombatInputComponent::FireStarted()
{
	if (ACombatWeapon* Weapon = GetWeapon())
	{
		Weapon->Fire->SetTriggerHeld(true);
	}
}
void UCombatInputComponent::FireEnded()
{
	if (ACombatWeapon* Weapon = GetWeapon(); IsValid(Weapon))
	{
		Weapon->Fire->SetTriggerHeld(false);
	}
}
void UCombatInputComponent::ReloadStarted()
{
	if (ACombatWeapon* Weapon = GetWeapon())
	{
		Weapon->Ammo->StartReload();
	}
}
void UCombatInputComponent::ADSStarted()
{
	if (ACombatWeapon* Weapon = GetWeapon())
	{
		Weapon->Fire->SetADS(true);
	}
}
void UCombatInputComponent::ADSEnded()
{
	if (ACombatWeapon* Weapon = GetWeapon(); IsValid(Weapon))
	{
		Weapon->Fire->SetADS(false);
	}
}
