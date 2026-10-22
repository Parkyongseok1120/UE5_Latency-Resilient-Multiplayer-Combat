#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "CombatCharacter.h"
#include "CombatGameMode.h"
#include "CombatGameState.h"
#include "CombatHUD.h"
#include "UObject/SoftObjectPath.h"
#include "CombatPlayerController.h"
#include "CombatPlayerState.h"
#include "Data/UWeaponDataAsset.h"
#include "Data/UCharacterDataAsset.h"
#include "Data/CombatRules.h"
#include "Camera/CameraComponent.h"
#include "CombatWeapon.h"
#include "Components/WeaponAmmoComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Components/HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Engine/LocalPlayer.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#include "CombatTestWorld.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPSBaselineHealthAuthorityTest, "LatencyCombat.Baseline.HealthAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFPSBaselineHealthAuthorityTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	ACombatPlayerState* PS = PC->GetPlayerState<ACombatPlayerState>();
	TestEqual(TEXT("Health initialized during possession"), PS->Health->GetHealth(), 100.f);
	PS->Health->ApplyDamage(-25.f);
	TestEqual(TEXT("Negative damage rejected"), PS->Health->GetHealth(), 100.f);
	PS->SetRole(ROLE_SimulatedProxy);
	PS->Health->ApplyDamage(1000.f);
	PS->AddKill();
	PS->SetTeam(ETeamSide::Blue);
	TestEqual(TEXT("Client cannot change health"), PS->Health->GetHealth(), 100.f);
	TestEqual(TEXT("Client cannot award kills"), PS->KillCount, 0);
	TestTrue(TEXT("Client cannot change team"), PS->Team == ETeamSide::Red);
	PS->SetRole(ROLE_Authority);
	PS->Health->ApplyDamage(1000.f);
	PS->Health->ApplyDamage(1000.f);
	TestEqual(TEXT("Death is counted once"), PS->DeathCount, 1);
	PS->Health->Heal(100.f);
	TestEqual(TEXT("Dead player cannot heal"), PS->Health->GetHealth(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPSBaselineFireReloadTest, "LatencyCombat.Baseline.FireAndReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFPSBaselineFireReloadTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* ShooterPC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	ACombatPlayerController* TargetPC = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Blue);
	ACombatCharacter* Shooter = Cast<ACombatCharacter>(ShooterPC->GetPawn());
	ACombatPlayerState* TargetPS = TargetPC->GetPlayerState<ACombatPlayerState>();
	ShooterPC->SetControlRotation(FRotator::ZeroRotator);
	Shooter->GetWeapon()->Fire->FireWeapon();
	TestEqual(TEXT("First shot is immediately available"), Shooter->GetWeapon()->Ammo->GetMagazineAmmo(), 29);
	TestEqual(TEXT("Server hitscan reduces HP after armor absorption"), TargetPS->Health->GetHealth(), 87.5f);
	Shooter->GetWeapon()->Fire->FireWeapon();
	TestEqual(TEXT("Server rejects firing before interval"), Shooter->GetWeapon()->Ammo->GetMagazineAmmo(), 29);
	TestEqual(TEXT("Rejected shot does not damage target"), TargetPS->Health->GetHealth(), 87.5f);
	Shooter->GetWeapon()->Ammo->StartReload();
	Shooter->GetWeapon()->Ammo->FinishReload();
	TestEqual(TEXT("Reload cannot be completed early"), Shooter->GetWeapon()->Ammo->GetMagazineAmmo(), 29);
	Shooter->GetWeapon()->Fire->FireWeapon();
	TestEqual(TEXT("Reload blocks shooting"), Shooter->GetWeapon()->Ammo->GetMagazineAmmo(), 29);
	Fixture.Advance(2.1f);
	TestEqual(TEXT("Server finishes reload"), Shooter->GetWeapon()->Ammo->GetMagazineAmmo(), 30);
	TestEqual(TEXT("Only missing rounds transferred"), Shooter->GetWeapon()->Ammo->GetReserveAmmo(), 89);
	FWeaponStats EmptyReserve;
	EmptyReserve.ReserveAmmo = 0;
	ACombatPlayerController* EmptyPC = Fixture.SpawnPlayer(FVector(-1000, 500, 300), ETeamSide::Red, EmptyReserve);
	ACombatCharacter* EmptyPawn = Cast<ACombatCharacter>(EmptyPC->GetPawn());
	EmptyPawn->GetWeapon()->Fire->FireWeapon();
	EmptyPawn->GetWeapon()->Ammo->StartReload();
	TestEqual(TEXT("Missing round with empty reserve"), EmptyPawn->GetWeapon()->Ammo->GetMagazineAmmo(),
		EmptyReserve.MagazineSize - 1);
	TestTrue(TEXT("No reload with empty reserve"), EmptyPawn->GetCombatState() != ECombatState::Reloading);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPSBaselineRespawnTest, "LatencyCombat.Baseline.SimultaneousRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFPSBaselineRespawnTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatGameMode* GM = Fixture.World->GetAuthGameMode<ACombatGameMode>();
	GM->RespawnDelay = 0.1f;
	Fixture.World->SpawnActor<APlayerStart>(FVector(0, 0, 300), FRotator::ZeroRotator)->PlayerStartTag = TEXT("Red");
	Fixture.World->SpawnActor<APlayerStart>(FVector(1000, 0, 300), FRotator::ZeroRotator)->PlayerStartTag =
		TEXT("Blue");
	ACombatPlayerController* RedPC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	ACombatPlayerController* BluePC = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Blue);
	ACombatCharacter* Red = Cast<ACombatCharacter>(RedPC->GetPawn());
	ACombatCharacter* Blue = Cast<ACombatCharacter>(BluePC->GetPawn());
	Red->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	Blue->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	Red->Die(nullptr);
	const int32 DeadAmmo = Red->GetWeapon()->Ammo->GetMagazineAmmo();
	Red->GetWeapon()->Fire->FireWeapon();
	TestEqual(TEXT("Dead player cannot fire"), Red->GetWeapon()->Ammo->GetMagazineAmmo(), DeadAmmo);
	Fixture.Advance(0.2f);
	TestTrue(TEXT("Red independently respawns"), RedPC->GetPawn() && RedPC->GetPawn() != Red);
	TestTrue(TEXT("Blue independently respawns"), BluePC->GetPawn() && BluePC->GetPawn() != Blue);
	TestEqual(TEXT("Red health reset"), RedPC->GetPlayerState<ACombatPlayerState>()->Health->GetHealth(), 100.f);
	TestEqual(TEXT("Blue health reset"), BluePC->GetPlayerState<ACombatPlayerState>()->Health->GetHealth(), 100.f);
	TestEqual(TEXT("Repeated death counted once"), RedPC->GetPlayerState<ACombatPlayerState>()->DeathCount, 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPSCoverTest, "LatencyCombat.Baseline.CoverAndFriendlyFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFPSCoverTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* ShooterPC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	ACombatPlayerController* TargetPC = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Red);
	ACombatCharacter* Shooter = Cast<ACombatCharacter>(ShooterPC->GetPawn());
	ACombatPlayerState* TargetPS = TargetPC->GetPlayerState<ACombatPlayerState>();
	ShooterPC->SetControlRotation(FRotator::ZeroRotator);
	Shooter->GetWeapon()->Fire->FireWeapon();
	TestEqual(TEXT("Friendly fire blocked"), TargetPS->Health->GetHealth(), 100.f);
	Fixture.Advance(0.2f);
	TargetPS->SetTeam(ETeamSide::Blue);
	AActor* Cover = Fixture.World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Cover);
	Cover->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(50, 200, 200));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Box->RegisterComponent();
	Cover->SetActorLocation(FVector(500, 0, 350));
	Shooter->GetWeapon()->Fire->FireWeapon();
	TestEqual(TEXT("Cover prevents damage"), TargetPS->Health->GetHealth(), 100.f);
	Cover->Destroy();
	Fixture.Advance(0.2f);
	Shooter->GetWeapon()->Fire->FireWeapon();
	TestTrue(TEXT("Uncovered enemy receives damage"), TargetPS->Health->GetHealth() < 100.f);
	return true;
}

struct FInitialSpawnObserver
{
	ACombatPlayerController* JoiningPC = nullptr;
	TArray<ETeamSide> TeamsAtSpawn;
	void HandleActorSpawned(AActor* Actor)
	{
		if (Cast<ACombatCharacter>(Actor) && JoiningPC)
		{
			TeamsAtSpawn.Add(JoiningPC->GetPlayerState<ACombatPlayerState>()->Team);
		}
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFPSInitialTeamSpawnTest, "LatencyCombat.Baseline.InitialTeamSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFPSInitialTeamSpawnTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatGameMode* GM = Fixture.World->GetAuthGameMode<ACombatGameMode>();
	Fixture.World->SpawnActor<APlayerStart>(FVector(-900, 0, 100), FRotator::ZeroRotator)->PlayerStartTag = TEXT("Red");
	Fixture.World->SpawnActor<APlayerStart>(FVector(900, 0, 100), FRotator(0, 180, 0))->PlayerStartTag = TEXT("Blue");
	ACombatPlayerController* RedPC = Fixture.World->SpawnActor<ACombatPlayerController>();
	RedPC->Player = NewObject<ULocalPlayer>(GEngine);
	FInitialSpawnObserver Observer;
	Observer.JoiningPC = RedPC;
	const FDelegateHandle SpawnObserver = Fixture.World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateRaw(&Observer, &FInitialSpawnObserver::HandleActorSpawned));
	TestTrue(TEXT("Red login initialization succeeds"),
		GM->InitNewPlayer(RedPC, FUniqueNetIdRepl(), TEXT(""), TEXT("")).IsEmpty());
	TestTrue(TEXT("Initial red cached start uses team"),
		RedPC->StartSpot.IsValid() && RedPC->StartSpot->GetActorLocation().X < 0.f);
	GM->PostLogin(RedPC);
	ACombatPlayerController* BluePC = Fixture.World->SpawnActor<ACombatPlayerController>();
	BluePC->Player = NewObject<ULocalPlayer>(GEngine);
	Observer.JoiningPC = BluePC;
	TestTrue(TEXT("Blue login initialization succeeds"),
		GM->InitNewPlayer(BluePC, FUniqueNetIdRepl(), TEXT(""), TEXT("")).IsEmpty());
	TestTrue(TEXT("Initial blue cached start uses team"),
		BluePC->StartSpot.IsValid() && BluePC->StartSpot->GetActorLocation().X > 0.f);
	GM->PostLogin(BluePC);
	Fixture.World->RemoveOnActorSpawnedHandler(SpawnObserver);
	TestEqual(TEXT("Both initial spawn events observed"), Observer.TeamsAtSpawn.Num(), 2);
	if (Observer.TeamsAtSpawn.Num() == 2)
	{
		TestTrue(TEXT("Red team exists when pawn is spawned"), Observer.TeamsAtSpawn[0] == ETeamSide::Red);
		TestTrue(TEXT("Blue team exists when pawn is spawned"), Observer.TeamsAtSpawn[1] == ETeamSide::Blue);
	}
	TestTrue(TEXT("First player assigned red"), RedPC->GetPlayerState<ACombatPlayerState>()->Team == ETeamSide::Red);
	TestTrue(
		TEXT("Second player assigned blue"), BluePC->GetPlayerState<ACombatPlayerState>()->Team == ETeamSide::Blue);
	APawn* RedPawn = RedPC->GetPawn();
	APawn* BluePawn = BluePC->GetPawn();
	if (TestNotNull(TEXT("First PostLogin spawns pawn"), RedPawn))
	{
		TestTrue(TEXT("Initial red spawn uses assigned team"), RedPawn->GetActorLocation().X < 0.f);
	}
	if (TestNotNull(TEXT("Second PostLogin spawns pawn"), BluePawn))
	{
		TestTrue(TEXT("Initial blue spawn uses assigned team"), BluePawn->GetActorLocation().X > 0.f);
	}
	// A stale start on the opposing side must not override team selection at restart.
	BluePC->StartSpot = RedPC->StartSpot;
	AActor* RestartStart = GM->FindPlayerStart(BluePC);
	TestTrue(TEXT("Restart ignores cached opposing start"), RestartStart && RestartStart->GetActorLocation().X > 0.f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponOwnershipTest, "LatencyCombat.Structure.WeaponOwnershipAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWeaponOwnershipTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	ACombatCharacter* Pawn = Cast<ACombatCharacter>(PC->GetPawn());
	ACombatWeapon* Weapon = Pawn->GetWeapon();
	if (!TestNotNull(TEXT("Pawn has a separate weapon actor"), Weapon))
	{
		return false;
	}
	TestTrue(TEXT("Weapon ownership routes RPCs through pawn"), Weapon->GetOwner() == Pawn);
	TestTrue(TEXT("Weapon instigator is the pawn"), Weapon->GetInstigator() == Pawn);
	TestTrue(TEXT("Weapon is attached to pawn camera"),
		Weapon->GetRootComponent()->GetAttachParent() == Pawn->FirstPersonCamera);
	TestTrue(TEXT("Weapon actor replicates"), Weapon->GetIsReplicated());
	TestTrue(TEXT("Fire and ammo are replicated components"),
		Weapon->Fire->GetIsReplicated() && Weapon->Ammo->GetIsReplicated());
	const int32 AmmoBefore = Weapon->Ammo->GetMagazineAmmo();
	Weapon->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client cannot consume authoritative ammo"), Weapon->Ammo->ConsumeRound());
	Weapon->Ammo->Initialize(FWeaponStats());
	TestEqual(TEXT("Client cannot initialize authoritative ammo"), Weapon->Ammo->GetMagazineAmmo(), AmmoBefore);
	Weapon->SetRole(ROLE_Authority);
	Pawn->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	TestTrue(TEXT("Death reaches weapon component"), Weapon->Fire->GetCombatState() == ECombatState::Dead);
	TestFalse(TEXT("Dead weapon cannot fire"), Weapon->Fire->CanFire());
	const TWeakObjectPtr<ACombatWeapon> OldWeapon(Weapon);
	Pawn->Destroy();
	TestFalse(TEXT("Weapon lifetime ends with pawn"), OldWeapon.IsValid());
	UClass* ExistingBlueprint =
		StaticLoadClass(APawn::StaticClass(), nullptr, TEXT("/Game/Blueprints/Character/BP_Default.BP_Default_C"));
	TestNotNull(TEXT("Existing Pawn Blueprint remains loadable"), ExistingBlueprint);
	struct FClassRedirectExpectation
	{
		const TCHAR* OldPath;
		UClass* Expected;
	};
	const FClassRedirectExpectation Redirects[] = {
		{TEXT("/Script/Latency_MultiCombat.LatencyMultiCombatCharacter"), ACombatCharacter::StaticClass()},
		{TEXT("/Script/Latency_MultiCombat.LatencyMultiCombatGameMode"), ACombatGameMode::StaticClass()},
		{TEXT("/Script/Latency_MultiCombat.LatencyMultiCombatGameState"), ACombatGameState::StaticClass()},
		{TEXT("/Script/Latency_MultiCombat.LatencyMultiCombatPlayerState"), ACombatPlayerState::StaticClass()},
		{TEXT("/Script/Latency_MultiCombat.LatencyMultiCombatPlayerController"),
			ACombatPlayerController::StaticClass()},
		{TEXT("/Script/Latency_MultiCombat.LatencyMultiCombatHUD"), ACombatHUD::StaticClass()}};
	for (const FClassRedirectExpectation& Redirect : Redirects)
	{
		FSoftClassPath Path(Redirect.OldPath);
		TestTrue(
			FString::Printf(TEXT("Native class redirect exists: %s"), Redirect.OldPath), Path.FixupCoreRedirects());
		TestTrue(FString::Printf(TEXT("Redirected native class loads: %s"), Redirect.OldPath),
			Path.TryLoadClass<UObject>() == Redirect.Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatDataAssetTest, "LatencyCombat.Structure.DataAssetSources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatDataAssetTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	UCharacterDataAsset* Data = NewObject<UCharacterDataAsset>(Fixture.World);
	Data->Stats.MaxHealth = 240.f;
	Data->Stats.MaxArmor = 80.f;
	Data->Stats.ArmorAbsorptionRatio = 0.25f;
	FWeaponStats Stats;
	Stats.MagazineSize = 9;
	Stats.ReserveAmmo = 12;
	Stats.ReloadTime = 0.25f;
	Stats.Damage = 40.f;
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red, Stats, Data);
	ACombatCharacter* Pawn = Cast<ACombatCharacter>(PC->GetPawn());
	ACombatPlayerState* PS = PC->GetPlayerState<ACombatPlayerState>();
	TestEqual(TEXT("Health capacity comes from character asset"), PS->Health->GetMaxHealth(), 240.f);
	TestEqual(TEXT("Armor comes from character asset"), PS->Health->GetArmor(), 80.f);
	PS->Health->ApplyDamage(40.f);
	TestEqual(TEXT("Armor absorption uses asset ratio"), PS->Health->GetHealth(), 210.f);
	TestEqual(TEXT("Armor consumes absorbed damage"), PS->Health->GetArmor(), 70.f);
	PS->Health->Heal(1000.f);
	TestEqual(TEXT("Healing respects asset capacity"), PS->Health->GetHealth(), 240.f);
	PS->Health->ResetForRespawn();
	TestEqual(TEXT("Reset restores configured spawn armor"), PS->Health->GetArmor(), 80.f);
	TestEqual(TEXT("Weapon capacity comes from weapon asset"), Pawn->GetWeapon()->Ammo->GetMagazineAmmo(), 9);
	Pawn->GetWeapon()->Fire->FireWeapon();
	Pawn->GetWeapon()->Ammo->StartReload();
	Pawn->GetWeapon()->Ammo->FinishReload();
	TestEqual(TEXT("Custom reload cannot complete early"), Pawn->GetWeapon()->Ammo->GetMagazineAmmo(), 8);
	Fixture.Advance(0.35f);
	TestEqual(TEXT("Custom reload completes in ammo component"), Pawn->GetWeapon()->Ammo->GetMagazineAmmo(), 9);
	TestEqual(TEXT("Custom reload conserves ammo"), Pawn->GetWeapon()->Ammo->GetReserveAmmo(), 11);
	TestEqual(TEXT("Shared damage rule uses centimeter conversion"), FCombatRules::DamageAtRange(Stats, 5000.f), 40.f);
	TestEqual(TEXT("Shared damage rule applies minimum ratio"), FCombatRules::DamageAtRange(Stats, 12000.f), 20.f);
	return true;
}
#endif
