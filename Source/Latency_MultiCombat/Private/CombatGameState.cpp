#include "CombatGameState.h"
#include "Components/RewindBenchmarkComponent.h"
#include "Components/CombatScenarioComponent.h"
#include "Net/UnrealNetwork.h"
#include "CombatPlayerState.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"

ACombatGameState::ACombatGameState()
	: RedTeamScore(0), BlueTeamScore(0), bMatchEnded(false), WinningTeam(ETeamSide::Neutral)
{
	RewindBenchmark = CreateDefaultSubobject<URewindBenchmarkComponent>(TEXT("RewindBenchmark"));
	ScenarioRunner = CreateDefaultSubobject<UCombatScenarioComponent>(TEXT("ScenarioRunner"));
}

void ACombatGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACombatGameState, RedTeamScore);
	DOREPLIFETIME(ACombatGameState, BlueTeamScore);
	DOREPLIFETIME(ACombatGameState, bMatchEnded);
	DOREPLIFETIME(ACombatGameState, WinningTeam);
	DOREPLIFETIME(ACombatGameState, LagCompensation);
	DOREPLIFETIME(ACombatGameState, Experiment);
}

bool ACombatGameState::SupportsLiveExperiments()
{
#if !UE_BUILD_SHIPPING && DO_ENABLE_NET_TEST
	return true;
#else
	return false;
#endif
}

void ACombatGameState::InitializeExperiment()
{
	if (!HasAuthority())
	{
		return;
	}
	Experiment.bRewindEnabled = LagCompensation.bEnabled;
#if DO_ENABLE_NET_TEST
	if (const UNetDriver* Driver = GetWorld()->GetNetDriver())
	{
		Experiment.PacketLagMs = Driver->PacketSimulationSettings.PktLag;
	}
#endif
	ForceNetUpdate();
}

bool ACombatGameState::SetExperiment(bool bRewindEnabled, int32 PacketLagMs, uint32 ExpectedRevision)
{
	if (!HasAuthority() || !SupportsLiveExperiments() || ExpectedRevision != Experiment.Revision ||
		!FCombatExperimentState::IsValidLag(PacketLagMs))
	{
		return false;
	}
	const int32 PreviousLag = Experiment.PacketLagMs;
	++Experiment.Revision;
	Experiment.bRewindEnabled = bRewindEnabled;
	Experiment.PacketLagMs = PacketLagMs;
	// Drain old delayed packets and allow a fresh clock sample before collecting the new interval.
	Experiment.CollectAfterServerTime = GetWorld()->GetTimeSeconds() + 2.0 +
		4.0 * FMath::Max(PreviousLag, PacketLagMs) / 1000.0;
	OnRep_Experiment();
	ForceNetUpdate();
	return true;
}

bool ACombatGameState::SetScenario(const FCombatScenarioSettings& Settings, uint32 ExpectedRevision)
{
	if (!HasAuthority() || !SupportsLiveExperiments() || ExpectedRevision != Experiment.Revision ||
		!Settings.IsValid() || ScenarioRunner->IsBusy())
	{
		return false;
	}
	Experiment.Scenario = Settings;
	return SetExperiment(LagCompensation.bEnabled, Experiment.PacketLagMs, ExpectedRevision);
}

void ACombatGameState::OnRep_Experiment()
{
	if (Experiment.Revision == 0)
	{
		return;
	}
	LagCompensation.bEnabled = Experiment.bRewindEnabled;
#if DO_ENABLE_NET_TEST
	if (UNetDriver* Driver = GetWorld()->GetNetDriver())
	{
		FPacketSimulationSettings Settings;
		Settings.PktLag = Experiment.PacketLagMs;
		Driver->SetPacketSimulationSettings(Settings);
	}
#endif
	UE_LOG(LogTemp, Display, TEXT("[LiveExperiment] Mode=%d Revision=%u Rewind=%d OneWayLagMs=%d"),
		GetNetMode(), Experiment.Revision, Experiment.bRewindEnabled, Experiment.PacketLagMs);
}

int32 ACombatGameState::GetTeamMemberCount(ETeamSide Team) const
{
	return GetTeamMembers(Team).Num();
}

TArray<ACombatPlayerState*> ACombatGameState::GetTeamMembers(ETeamSide Team) const
{
	TArray<ACombatPlayerState*> Members;
	// PlayerArray is maintained by the engine on both server and clients, including disconnects.
	for (APlayerState* Player : PlayerArray)
	{
		if (ACombatPlayerState* PS = Cast<ACombatPlayerState>(Player))
		{
			if (IsValid(PS) && !PS->IsInactive() && PS->Team == Team)
			{
				Members.Add(PS);
			}
		}
	}
	return Members;
}

void ACombatGameState::EndMatch(ETeamSide Winner)
{
	if (!HasAuthority() || bMatchEnded)
	{
		return;
	}
	bMatchEnded = true;
	WinningTeam = Winner;
	ForceNetUpdate();
}
