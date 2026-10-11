#include "Components/CombatSmokeComponent.h"
#include "CombatWeapon.h"
#include "Components/WeaponFireComponent.h"
#include "Components/HealthComponent.h"
#include "CombatCharacter.h"
#include "CombatPlayerController.h"
#include "CombatPlayerState.h"
#include "CombatGameState.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

UCombatSmokeComponent::UCombatSmokeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCombatSmokeComponent::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	const ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwner());
	if (PC && PC->IsLocalController() && FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineSmoke")))
	{
		bSmokeShooter = FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineShooter"));
		SmokeStartTime = GetWorld()->GetTimeSeconds();
		SetComponentTickEnabled(true);
	}
#endif
}

void UCombatSmokeComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
#if !UE_BUILD_SHIPPING
	if (!bSmokeFinished)
	{
		TickSmoke();
	}
#endif
}

#if !UE_BUILD_SHIPPING
void UCombatSmokeComponent::TickSmoke()
{
	ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetOwner());
	if (!PC)
	{
		return;
	}
	if (GetWorld()->GetTimeSeconds() - SmokeStartTime > 60.f)
	{
		UE_LOG(LogTemp, Error, TEXT("[FPSBaselineSmoke] FAIL timeout Damage=%d Death=%d"), bSmokeSawDamage,
			bSmokeSawDeath);
		bSmokeFinished = true;
		FPlatformMisc::RequestExit(false);
		return;
	}
	ACombatGameState* GS = GetWorld()->GetGameState<ACombatGameState>();
	ACombatPlayerState* MyPS = PC->GetPlayerState<ACombatPlayerState>();
	ACombatCharacter* CombatCharacter = Cast<ACombatCharacter>(PC->GetPawn());
	if (!GS || !MyPS || !CombatCharacter || !CombatCharacter->GetWeapon() || GS->PlayerArray.Num() < 2 ||
		MyPS->Team == ETeamSide::Neutral)
	{
		return;
	}

	ACombatPlayerState* Victim = nullptr;
	ACombatPlayerState* Shooter = nullptr;
	for (APlayerState* ListedPlayer : GS->PlayerArray)
	{
		ACombatPlayerState* PS = Cast<ACombatPlayerState>(ListedPlayer);
		if (!PS)
		{
			continue;
		}
		if (PS->Team == ETeamSide::Blue)
		{
			Victim = PS;
		}
		if (PS->Team == ETeamSide::Red)
		{
			Shooter = PS;
		}
	}
	if (!Victim || !Shooter || !Victim->GetPawn() || !Shooter->GetPawn())
	{
		return;
	}
	if (SmokeLastHealth != Victim->Health->GetHealth())
	{
		UE_LOG(LogTemp, Display,
			TEXT("[FPSBaselineSmoke] %s observes Blue HP=%.1f Armor=%.1f Deaths=%d RedKills=%d Score=%d:%d"),
			bSmokeShooter ? TEXT("Shooter") : TEXT("Observer"), Victim->Health->GetHealth(), Victim->Health->GetArmor(),
			Victim->DeathCount, Shooter->KillCount, GS->RedTeamScore, GS->BlueTeamScore);
		SmokeLastHealth = Victim->Health->GetHealth();
	}
	if (Victim->Health->GetHealth() > 0.f && Victim->Health->GetHealth() < Victim->Health->GetMaxHealth())
	{
		bSmokeSawDamage = true;
	}
	if (Victim->Health->IsDead() && Victim->DeathCount == 1)
	{
		bSmokeSawDeath = true;
	}
	if (bSmokeSawDamage && bSmokeSawDeath && !Victim->Health->IsDead() &&
		Victim->Health->GetHealth() == Victim->Health->GetMaxHealth() && Victim->DeathCount == 1 &&
		Shooter->KillCount == 1 && GS->RedTeamScore == 1 && GS->BlueTeamScore == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("[FPSBaselineSmoke] PASS %s: replicated HP, death, score and respawn agree"),
			bSmokeShooter ? TEXT("Shooter") : TEXT("Observer"));
		bSmokeFinished = true;
		FPlatformMisc::RequestExit(false);
		return;
	}
	// First client is Red; the second is Blue. No test-only damage/teleport RPC is exposed.
	if (bSmokeShooter && MyPS == Shooter && !bSmokeSawDeath && Victim->DeathCount == 0 &&
		GetWorld()->GetTimeSeconds() - SmokeStartTime > 5.f)
	{
		const FVector Target = Victim->GetPawn()->GetActorLocation() + FVector(0.f, 0.f, 30.f);
		PC->SetControlRotation((Target - CombatCharacter->GetPawnViewLocation()).Rotation());
		CombatCharacter->GetWeapon()->Fire->FireWeapon();
	}
}
#endif
