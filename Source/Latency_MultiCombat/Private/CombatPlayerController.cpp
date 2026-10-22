#include "CombatPlayerController.h"
#include "Components/CombatTimeSyncComponent.h"
#include "CombatBenchmarkComponent.h"
#include "Components/CombatSmokeComponent.h"

ACombatPlayerController::ACombatPlayerController()
{
	TimeSync = CreateDefaultSubobject<UCombatTimeSyncComponent>(TEXT("TimeSync"));
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
