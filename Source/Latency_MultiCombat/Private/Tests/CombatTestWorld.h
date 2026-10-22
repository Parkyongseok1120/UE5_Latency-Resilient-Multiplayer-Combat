#pragma once
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

struct FFPSBaselineTestWorld
{
	UWorld* World;
	UGameInstance* GameInstance;
	FFPSBaselineTestWorld()
	{
		GameInstance = NewObject<UGameInstance>(GEngine);
		GameInstance->InitializeStandalone();
		World = GameInstance->GetWorld();
		const FURL URL(
			nullptr, TEXT("/Engine/Maps/Entry?game=/Script/Latency_MultiCombat.CombatGameMode"), TRAVEL_Absolute);
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
	ACombatPlayerController* SpawnPlayer(const FVector& Location, ETeamSide Team,
		const FWeaponStats& WeaponStats = FWeaponStats(), UCharacterDataAsset* CharacterData = nullptr)
	{
		ACombatPlayerController* PC = World->SpawnActor<ACombatPlayerController>();
		ACombatPlayerState* PS = PC->GetPlayerState<ACombatPlayerState>();
		PS->SetTeam(Team);
		UWeaponDataAsset* Weapon = NewObject<UWeaponDataAsset>(World);
		Weapon->Stats = WeaponStats;
		Weapon->Stats.BaseSpread = 0.f;
		ACombatCharacter* Pawn =
			World->SpawnActorDeferred<ACombatCharacter>(ACombatCharacter::StaticClass(), FTransform(Location));
		Pawn->CharacterData = CharacterData ? CharacterData : NewObject<UCharacterDataAsset>(World);
		Pawn->CharacterData->DefaultWeapon = Weapon;
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
