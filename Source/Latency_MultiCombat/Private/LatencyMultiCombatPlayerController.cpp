// Latency_MultiCombat - Player Controller Implementation
#include "LatencyMultiCombatPlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"

ALatencyMultiCombatPlayerController::ALatencyMultiCombatPlayerController()
{
}

void ALatencyMultiCombatPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetupEnhancedInput();
}

void ALatencyMultiCombatPlayerController::SetupEnhancedInput()
{
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC) return;

	// Mapping context is configured via Enhanced Input User Settings (Project Settings > Input)
	// or assigned through the PlayerController Blueprint's CombatMappingContext property.
	UE_LOG(LogTemp, Display, TEXT("[Input] Enhanced Input ready. Configure mapping contexts in Project Settings > Input."));

	// NOTE: No BindAction calls here by design.
	// All action→function bindings are handled in ALatencyMultiCombatCharacter::SetupPlayerInputComponent().
	// Key→action mappings are 100% editor-driven via the InputMappingContext asset above.
}
