#include "Components/TeamSpawnComponent.h"
#include "Engine/World.h"
#include "CombatGameMode.h"
#include "CombatGameState.h"
#include "CombatPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"

void UTeamSpawnComponent::AssignPlayerTeam(APlayerController* Player)
{
	if (!GetOwner()->HasAuthority() || !Player)
	{
		return;
	}
	ACombatPlayerState* PS = Player->GetPlayerState<ACombatPlayerState>();
	ACombatGameState* GS = GetWorld()->GetGameState<ACombatGameState>();
	if (!PS || !GS || PS->Team != ETeamSide::Neutral)
	{
		return;
	}
	const ETeamSide Team = GS->GetTeamMemberCount(ETeamSide::Red) <= GS->GetTeamMemberCount(ETeamSide::Blue)
		? ETeamSide::Red
		: ETeamSide::Blue;
	PS->SetTeam(Team);
}

AActor* UTeamSpawnComponent::ChooseTeamStart(AController* Player) const
{
	const ACombatPlayerState* PS = Player ? Player->GetPlayerState<ACombatPlayerState>() : nullptr;
	const FName Tag = PS && PS->Team == ETeamSide::Red ? FName(TEXT("Red"))
		: PS && PS->Team == ETeamSide::Blue			   ? FName(TEXT("Blue"))
													   : NAME_None;
	if (Tag.IsNone())
	{
		return nullptr;
	}
	ACombatGameMode* GM = Cast<ACombatGameMode>(GetOwner());
	const UClass* PawnClass = GM ? GM->GetDefaultPawnClassForController(Player) : nullptr;
	const ACharacter* DefaultPawn = PawnClass ? Cast<ACharacter>(PawnClass->GetDefaultObject()) : nullptr;
	if (!DefaultPawn)
	{
		return nullptr;
	}
	const UCapsuleComponent* Capsule = DefaultPawn->GetCapsuleComponent();
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->PlayerStartTag != Tag)
		{
			continue;
		}
		FCollisionQueryParams Params;
		if (Player && Player->GetPawn())
		{
			Params.AddIgnoredActor(Player->GetPawn());
		}
		if (!GetWorld()->OverlapBlockingTestByChannel(It->GetActorLocation(), FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
				Params))
		{
			Starts.Add(*It);
		}
	}
	return Starts.IsEmpty() ? nullptr : Starts[FMath::RandRange(0, Starts.Num() - 1)];
}
