#include "LatencyMultiCombatGameMode.h"
#include "LatencyMultiCombatGameState.h"
#include "LatencyMultiCombatPlayerState.h"
#include "LatencyMultiCombatCharacter.h"
#include "LatencyMultiCombatPlayerController.h"
#include "LatencyMultiCombatHUD.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"

ALatencyMultiCombatGameMode::ALatencyMultiCombatGameMode()
{
	PlayerStateClass = ALatencyMultiCombatPlayerState::StaticClass();
	GameStateClass = ALatencyMultiCombatGameState::StaticClass();
	DefaultPawnClass = ALatencyMultiCombatCharacter::StaticClass();
	PlayerControllerClass = ALatencyMultiCombatPlayerController::StaticClass();
	HUDClass = ALatencyMultiCombatHUD::StaticClass();
	DefaultCharacterClass = ALatencyMultiCombatCharacter::StaticClass();
}

void ALatencyMultiCombatGameMode::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Display, TEXT("[DM] Server-authoritative FPS baseline started. TeamSize=%d"), TeamSize);
}

void ALatencyMultiCombatGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (!NewPlayer) return;
	if (auto* PS = NewPlayer->GetPlayerState<ALatencyMultiCombatPlayerState>())
		if (PS->Team == ETeamSide::Neutral)
			if (auto* GS = GetDMGameState()) GS->AddPlayerToTeam(PS, AssignTeam());
}

void ALatencyMultiCombatGameMode::Logout(AController* Exiting)
{
	if (FTimerHandle* Handle = RespawnTimers.Find(Exiting))
	{
		GetWorldTimerManager().ClearTimer(*Handle);
		RespawnTimers.Remove(Exiting);
	}
	if (Exiting)
		if (auto* PS = Exiting->GetPlayerState<ALatencyMultiCombatPlayerState>()) PS->SetTeam(ETeamSide::Neutral);
	Super::Logout(Exiting);
}

void ALatencyMultiCombatGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	for (auto& Pair : RespawnTimers) GetWorldTimerManager().ClearTimer(Pair.Value);
	RespawnTimers.Empty();
	Super::EndPlay(Reason);
}

UClass* ALatencyMultiCombatGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	return DefaultCharacterClass ? DefaultCharacterClass.Get() : Super::GetDefaultPawnClassForController_Implementation(InController);
}

AActor* ALatencyMultiCombatGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const auto* PS = Player ? Player->GetPlayerState<ALatencyMultiCombatPlayerState>() : nullptr;
	const FName Tag = PS && PS->Team == ETeamSide::Red ? FName(TEXT("Red"))
		: PS && PS->Team == ETeamSide::Blue ? FName(TEXT("Blue")) : NAME_None;
	if (!Tag.IsNone())
	{
		TArray<APlayerStart*> Starts;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag != Tag) continue;
			// Keep simultaneous respawns from occupying the same capsule.
			FCollisionQueryParams Params;
			if (Player && Player->GetPawn()) Params.AddIgnoredActor(Player->GetPawn());
			if (!GetWorld()->OverlapBlockingTestByChannel(It->GetActorLocation(), FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(34.f, 88.f), Params))
				Starts.Add(*It);
		}
		if (!Starts.IsEmpty()) return Starts[FMath::RandRange(0, Starts.Num() - 1)];
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

ETeamSide ALatencyMultiCombatGameMode::AssignTeam()
{
	const auto* GS = GetDMGameState();
	if (!GS) return ETeamSide::Red;
	return GS->GetTeamMemberCount(ETeamSide::Red) <= GS->GetTeamMemberCount(ETeamSide::Blue)
		? ETeamSide::Red : ETeamSide::Blue;
}

bool ALatencyMultiCombatGameMode::CanDamagePlayer(const ALatencyMultiCombatPlayerState* Victim, AController* DamageInstigator) const
{
	const auto* GS = GetDMGameState();
	if (!HasAuthority() || !Victim || Victim->bIsDead || (GS && GS->bMatchEnded)) return false;
	const auto* Attacker = DamageInstigator ? DamageInstigator->GetPlayerState<ALatencyMultiCombatPlayerState>() : nullptr;
	return !Attacker || Attacker == Victim || bAllowFriendlyFire
		|| Victim->Team == ETeamSide::Neutral || Attacker->Team != Victim->Team;
}

void ALatencyMultiCombatGameMode::HandlePlayerDeath(ALatencyMultiCombatPlayerState* DeadPlayer, ALatencyMultiCombatPlayerState* Killer)
{
	if (!HasAuthority() || !DeadPlayer || !DeadPlayer->bIsDead) return;
	auto* Controller = Cast<AController>(DeadPlayer->GetOwner());
	// The character guards repeated Die calls; the timer also makes this handler idempotent.
	if (Controller && RespawnTimers.Contains(Controller)) return;
	auto* GS = GetDMGameState();
	if (GS && GS->bMatchEnded) return;
	if (Killer && Killer != DeadPlayer && Killer->Team != DeadPlayer->Team)
	{
		Killer->AddKill();
		if (GS)
		{
			GS->AddKillToTeam(Killer->Team);
			if (GS->CheckWinCondition())
				GS->EndMatch(GS->RedTeamScore >= GS->BlueTeamScore ? ETeamSide::Red : ETeamSide::Blue);
		}
	}
	if (!Controller || (GS && GS->bMatchEnded)) return;
	const TWeakObjectPtr<AController> WeakController(Controller);
	FTimerHandle& Handle = RespawnTimers.FindOrAdd(WeakController);
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
	{
		RespawnTimers.Remove(WeakController);
		if (WeakController.IsValid()) RespawnPlayer(WeakController.Get());
	}), FMath::Max(0.1f, RespawnDelay), false);
}

void ALatencyMultiCombatGameMode::RespawnPlayer(AController* Controller)
{
	if (!HasAuthority() || !IsValid(Controller)) return;
	const auto* GS = GetDMGameState();
	auto* PS = Controller->GetPlayerState<ALatencyMultiCombatPlayerState>();
	if (!PS || !PS->bIsDead || (GS && GS->bMatchEnded)) return;
	APawn* OldPawn = Controller->GetPawn();
	Controller->UnPossess();
	if (OldPawn) OldPawn->Destroy();
	// Standard restart honors default pawn classes, spawn collision and client possession.
	// The new character initializes health/armor in PossessedBy; scores stay in PlayerState.
	RestartPlayer(Controller);
}

ALatencyMultiCombatGameState* ALatencyMultiCombatGameMode::GetDMGameState() const
{
	return Cast<ALatencyMultiCombatGameState>(GameState);
}
