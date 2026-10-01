#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "LatencyMultiCombatCharacter.h"
#include "LatencyMultiCombatGameMode.h"
#include "LatencyMultiCombatPlayerController.h"
#include "LatencyMultiCombatPlayerState.h"
#include "Data/UWeaponDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

struct FFPSBaselineTestWorld
{
	UWorld* World;
	UGameInstance* GameInstance;
	FFPSBaselineTestWorld()
	{
		GameInstance = NewObject<UGameInstance>(GEngine);
		GameInstance->InitializeStandalone();
		World = GameInstance->GetWorld();
		const FURL URL(nullptr, TEXT("/Engine/Maps/Entry?game=/Script/Latency_MultiCombat.LatencyMultiCombatGameMode"), TRAVEL_Absolute);
		World->SetGameMode(URL);
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
	}
	~FFPSBaselineTestWorld()
	{
		World->EndPlay(EEndPlayReason::Quit);
		GameInstance->Shutdown();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
	ALatencyMultiCombatPlayerController* SpawnPlayer(const FVector& Location, ETeamSide Team)
	{
		auto* PC = World->SpawnActor<ALatencyMultiCombatPlayerController>();
		auto* PS = PC->GetPlayerState<ALatencyMultiCombatPlayerState>();
		PS->SetTeam(Team);
		auto* Weapon = NewObject<UWeaponDataAsset>(World);
		Weapon->Stats.BaseSpread = 0.f;
		auto* Pawn = World->SpawnActorDeferred<ALatencyMultiCombatCharacter>(
			ALatencyMultiCombatCharacter::StaticClass(), FTransform(Location));
		Pawn->CurrentWeapon = Weapon;
		UGameplayStatics::FinishSpawningActor(Pawn, FTransform(Location));
		PC->Possess(Pawn);
		return PC;
	}
	void Advance(float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += 0.05f)
		{
			// TimerManager ticks once per engine frame even when several worlds exist.
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 0.05f);
		}
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPSBaselineHealthAuthorityTest, "LatencyCombat.Baseline.HealthAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFPSBaselineHealthAuthorityTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	auto* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	auto* PS = PC->GetPlayerState<ALatencyMultiCombatPlayerState>();
	TestEqual(TEXT("Health initialized during possession"), PS->CurrentHealth, 100.f);
	PS->ApplyDamage(-25.f, nullptr);
	TestEqual(TEXT("Negative damage rejected"), PS->CurrentHealth, 100.f);
	PS->SetRole(ROLE_SimulatedProxy);
	PS->ApplyDamage(1000.f, nullptr);
	PS->AddKill();
	PS->SetTeam(ETeamSide::Blue);
	TestEqual(TEXT("Client cannot change health"), PS->CurrentHealth, 100.f);
	TestEqual(TEXT("Client cannot award kills"), PS->KillCount, 0);
	TestTrue(TEXT("Client cannot change team"), PS->Team == ETeamSide::Red);
	PS->SetRole(ROLE_Authority);
	PS->ApplyDamage(1000.f, nullptr);
	PS->ApplyDamage(1000.f, nullptr);
	TestEqual(TEXT("Death is counted once"), PS->DeathCount, 1);
	PS->Heal(100.f);
	TestEqual(TEXT("Dead player cannot heal"), PS->CurrentHealth, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPSBaselineFireReloadTest, "LatencyCombat.Baseline.FireAndReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFPSBaselineFireReloadTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	auto* ShooterPC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	auto* TargetPC = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Blue);
	auto* Shooter = Cast<ALatencyMultiCombatCharacter>(ShooterPC->GetPawn());
	auto* TargetPS = TargetPC->GetPlayerState<ALatencyMultiCombatPlayerState>();
	ShooterPC->SetControlRotation(FRotator::ZeroRotator);
	Shooter->FireWeapon();
	TestEqual(TEXT("First shot is immediately available"), Shooter->MagazineAmmo, 29);
	TestEqual(TEXT("Server hitscan reduces HP after armor absorption"), TargetPS->CurrentHealth, 87.5f);
	Shooter->FireWeapon();
	TestEqual(TEXT("Server rejects firing before interval"), Shooter->MagazineAmmo, 29);
	TestEqual(TEXT("Rejected shot does not damage target"), TargetPS->CurrentHealth, 87.5f);
	Shooter->StartReload();
	Shooter->StopReload();
	TestEqual(TEXT("Reload cannot be completed early"), Shooter->MagazineAmmo, 29);
	Shooter->FireWeapon();
	TestEqual(TEXT("Reload blocks shooting"), Shooter->MagazineAmmo, 29);
	Fixture.Advance(2.1f);
	TestEqual(TEXT("Server finishes reload"), Shooter->MagazineAmmo, 30);
	TestEqual(TEXT("Only missing rounds transferred"), Shooter->ReserveAmmo, 89);
	Shooter->MagazineAmmo = 29;
	Shooter->ReserveAmmo = 0;
	Shooter->StartReload();
	TestTrue(TEXT("No reload with empty reserve"), Shooter->CombatState != ECombatState::Reloading);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPSBaselineRespawnTest, "LatencyCombat.Baseline.SimultaneousRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFPSBaselineRespawnTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	auto* GM = Fixture.World->GetAuthGameMode<ALatencyMultiCombatGameMode>();
	GM->RespawnDelay = 0.1f;
	Fixture.World->SpawnActor<APlayerStart>(FVector(0, 0, 300), FRotator::ZeroRotator)->PlayerStartTag = TEXT("Red");
	Fixture.World->SpawnActor<APlayerStart>(FVector(1000, 0, 300), FRotator::ZeroRotator)->PlayerStartTag = TEXT("Blue");
	auto* RedPC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	auto* BluePC = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Blue);
	auto* Red = Cast<ALatencyMultiCombatCharacter>(RedPC->GetPawn());
	auto* Blue = Cast<ALatencyMultiCombatCharacter>(BluePC->GetPawn());
	Red->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	Blue->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	Red->Die(nullptr);
	const int32 DeadAmmo = Red->MagazineAmmo;
	Red->FireWeapon();
	TestEqual(TEXT("Dead player cannot fire"), Red->MagazineAmmo, DeadAmmo);
	Fixture.Advance(0.2f);
	TestTrue(TEXT("Red independently respawns"), RedPC->GetPawn() && RedPC->GetPawn() != Red);
	TestTrue(TEXT("Blue independently respawns"), BluePC->GetPawn() && BluePC->GetPawn() != Blue);
	TestEqual(TEXT("Red health reset"), RedPC->GetPlayerState<ALatencyMultiCombatPlayerState>()->CurrentHealth, 100.f);
	TestEqual(TEXT("Blue health reset"), BluePC->GetPlayerState<ALatencyMultiCombatPlayerState>()->CurrentHealth, 100.f);
	TestEqual(TEXT("Repeated death counted once"), RedPC->GetPlayerState<ALatencyMultiCombatPlayerState>()->DeathCount, 1);
	return true;
}
#endif
