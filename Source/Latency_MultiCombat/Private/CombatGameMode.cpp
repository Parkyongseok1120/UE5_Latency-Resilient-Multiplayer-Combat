#include "CombatGameMode.h"
#include "Data/CombatNetworkDataAsset.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/HealthComponent.h"
#include "CombatGameState.h"
#include "CombatPlayerState.h"
#include "CombatCharacter.h"
#include "CombatPlayerController.h"
#include "CombatHUD.h"
#include "Components/RespawnComponent.h"
#include "Components/TeamSpawnComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

ACombatGameMode::ACombatGameMode()
{
	static ConstructorHelpers::FObjectFinderOptional<UCombatNetworkDataAsset> NetworkDefaults(
		TEXT("/Game/Data/DA_CombatNetwork.DA_CombatNetwork"), LOAD_NoWarn | LOAD_Quiet);
	NetworkData = NetworkDefaults.Get();
	PlayerStateClass = ACombatPlayerState::StaticClass();
	GameStateClass = ACombatGameState::StaticClass();
	DefaultPawnClass = ACombatCharacter::StaticClass();
	PlayerControllerClass = ACombatPlayerController::StaticClass();
	HUDClass = ACombatHUD::StaticClass();
	TeamSpawns = CreateDefaultSubobject<UTeamSpawnComponent>(TEXT("TeamSpawns"));
	Respawns = CreateDefaultSubobject<URespawnComponent>(TEXT("Respawns"));
}

void ACombatGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (ACombatGameState* State = GetDMGameState())
	{
		State->LagCompensation = (NetworkData ? NetworkData->LagCompensation : FLagCompensationSettings()).Normalized();
		if (FParse::Param(FCommandLine::Get(), TEXT("NoLagCompensation")))
		{
			State->LagCompensation.bEnabled = false;
		}
		State->ForceNetUpdate();
		State->InitializeExperiment();
	}
	UE_LOG(LogTemp, Display, TEXT("[DM] Server-authoritative FPS baseline started. TeamSize=%d"), TeamSize);
}

FString ACombatGameMode::InitNewPlayer(
	APlayerController* NewPlayer, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	// InitNewPlayer chooses and caches a start before PostLogin. Assign the team first.
	TeamSpawns->AssignPlayerTeam(NewPlayer);
	return Super::InitNewPlayer(NewPlayer, UniqueId, Options, Portal);
}

void ACombatGameMode::PostLogin(APlayerController* NewPlayer)
{
	TeamSpawns->AssignPlayerTeam(NewPlayer);
	Super::PostLogin(NewPlayer);
}

bool ACombatGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	// Re-evaluate team and occupancy on every restart, including simultaneous respawns.
	return false;
}

void ACombatGameMode::Logout(AController* Exiting)
{
	Respawns->Cancel(Exiting);
	if (Exiting)
	{
		if (ACombatPlayerState* PS = Exiting->GetPlayerState<ACombatPlayerState>())
		{
			PS->SetTeam(ETeamSide::Neutral);
		}
	}
	Super::Logout(Exiting);
}

AActor* ACombatGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* Start = TeamSpawns->ChooseTeamStart(Player))
	{
		return Start;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

bool ACombatGameMode::CanDamagePlayer(const ACombatPlayerState* Victim, AController* DamageInstigator) const
{
	const ACombatGameState* GS = GetDMGameState();
	if (!HasAuthority() || !Victim || Victim->Health->IsDead() || (GS && GS->bMatchEnded))
	{
		return false;
	}
	const ACombatPlayerState* Attacker =
		DamageInstigator ? DamageInstigator->GetPlayerState<ACombatPlayerState>() : nullptr;
	return !Attacker || Attacker == Victim || bAllowFriendlyFire || Victim->Team == ETeamSide::Neutral ||
		Attacker->Team != Victim->Team;
}

void ACombatGameMode::HandlePlayerDeath(ACombatPlayerState* DeadPlayer, ACombatPlayerState* Killer)
{
	if (!HasAuthority() || !DeadPlayer || !DeadPlayer->Health->IsDead())
	{
		return;
	}
	AController* Controller = Cast<AController>(DeadPlayer->GetOwner());
	// Pawn lifecycle and the respawn scheduler guard repeated death handling.
	if (Controller && Respawns->IsScheduled(Controller))
	{
		return;
	}
	ACombatGameState* GS = GetDMGameState();
	if (GS && GS->bMatchEnded)
	{
		return;
	}
	if (Killer && Killer != DeadPlayer && Killer->Team != DeadPlayer->Team)
	{
		Killer->AddKill();
		if (GS)
		{
			GS->AddKillToTeam(Killer->Team);
			if (GS->CheckWinCondition())
			{
				GS->EndMatch(GS->RedTeamScore >= GS->BlueTeamScore ? ETeamSide::Red : ETeamSide::Blue);
			}
		}
	}
	if (!Controller || (GS && GS->bMatchEnded))
	{
		return;
	}
	Respawns->Schedule(Controller, RespawnDelay);
}

ACombatGameState* ACombatGameMode::GetDMGameState() const
{
	return Cast<ACombatGameState>(GameState);
}
