#include "CombatPlayerController.h"
#include "CombatBenchmarkComponent.h"
#include "Components/CombatSmokeComponent.h"

ACombatPlayerController::ACombatPlayerController()
{
	Benchmark = CreateDefaultSubobject<UCombatBenchmarkComponent>(TEXT("CombatBenchmark"));
	Smoke = CreateDefaultSubobject<UCombatSmokeComponent>(TEXT("CombatSmoke"));
}

void ACombatPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}
