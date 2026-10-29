#include "Components/CombatScenarioComponent.h"
#include "CombatCharacter.h"
#include "CombatGameState.h"
#include "CombatPlayerController.h"
#include "CombatPlayerState.h"
#include "CombatWeapon.h"
#include "Components/CombatTelemetryComponent.h"
#include "Components/CombatTimeSyncComponent.h"
#include "Components/HealthComponent.h"
#include "Components/RewindHistoryComponent.h"
#include "Components/WeaponAmmoComponent.h"
#include "Components/WeaponFireComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

UCombatScenarioComponent::UCombatScenarioComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatScenarioComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatScenarioComponent, State);
}

bool UCombatScenarioComponent::IsBusy() const
{
	return State.Stage == ECombatScenarioStage::Preparing || State.Stage == ECombatScenarioStage::Running ||
		State.Stage == ECombatScenarioStage::Stopping;
}

bool UCombatScenarioComponent::IsParticipant(const ACombatPlayerController* PC) const
{
	return PC && IsBusy() && (PC->PlayerState == State.Shooter || PC->PlayerState == State.Target);
}

FString UCombatScenarioComponent::Start(ACombatPlayerController* Requester)
{
	ACombatGameState* GS = Cast<ACombatGameState>(GetOwner());
	if (!GS || !GS->HasAuthority() || !ACombatGameState::SupportsLiveExperiments() || IsBusy() || !Requester ||
		!GS->Experiment.Scenario.IsValid() || GS->bMatchEnded)
	{
		return TEXT("지금은 실험을 시작할 수 없습니다.");
	}
	if (!FCombatExperimentState::IsValidLag(GS->Experiment.PacketLagMs))
	{
		return TEXT("먼저 HUD에서 50ms 단위 지연을 선택해 주세요.");
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("FPSRewindBenchmark")) ||
		FParse::Param(FCommandLine::Get(), TEXT("FPSBaselineBenchmark")))
	{
		return TEXT("별도 벤치마크 실행 중에는 시작할 수 없습니다.");
	}
	ACombatPlayerState* Shooter = Requester->GetPlayerState<ACombatPlayerState>();
	ACombatPlayerState* Target = nullptr;
	if (!Shooter || Shooter->Team == ETeamSide::Neutral || GS->PlayerArray.Num() != 2)
	{
		return TEXT("서로 다른 팀의 플레이어 2명이 필요합니다.");
	}
	for (APlayerState* PS : GS->PlayerArray)
	{
		ACombatPlayerState* Candidate = Cast<ACombatPlayerState>(PS);
		if (Candidate && Candidate != Shooter && Candidate->Team != Shooter->Team && Candidate->Team != ETeamSide::Neutral)
		{
			Target = Candidate;
		}
	}
	Participants.Reset();
	for (ACombatPlayerState* PS : {Shooter, Target})
	{
		ACombatCharacter* Pawn = PS ? Cast<ACombatCharacter>(PS->GetPawn()) : nullptr;
		if (!Pawn || !Pawn->GetWeapon() || PS->Health->IsDead() || Pawn->GetWeapon()->Ammo->IsReloading())
		{
			Participants.Reset();
			return TEXT("두 플레이어의 생존·무기·재장전 상태를 확인해 주세요.");
		}
		FParticipantSnapshot& Snapshot = Participants.AddDefaulted_GetRef();
		Snapshot.Pawn = Pawn;
		Snapshot.Transform = Pawn->GetActorTransform();
		Snapshot.Aim = Pawn->GetControlRotation();
		Snapshot.Stats = Pawn->GetCharacterStats();
		Snapshot.Stats.MaxHealth = PS->Health->GetMaxHealth();
		Snapshot.Health = PS->Health->GetHealth();
		Snapshot.Armor = PS->Health->GetArmor();
		Snapshot.Magazine = Pawn->GetWeapon()->Ammo->GetMagazineAmmo();
		Snapshot.Reserve = Pawn->GetWeapon()->Ammo->GetReserveAmmo();
		Snapshot.MovementMode = Pawn->GetCharacterMovement()->MovementMode;
		Snapshot.CustomMovementMode = Pawn->GetCharacterMovement()->CustomMovementMode;
	}
	const uint32 RunId = State.RunId + 1;
	State = FCombatScenarioRun();
	State.RunId = RunId;
	State.Settings = GS->Experiment.Scenario;
	State.Shooter = Shooter;
	State.Target = Target;
	bCancelled = false;
	for (FParticipantSnapshot& Snapshot : Participants)
	{
		ACombatCharacter* Pawn = Snapshot.Pawn.Get();
		Pawn->GetWeapon()->Fire->SetTriggerHeld(false);
		Pawn->GetWeapon()->Fire->SetADS(false);
		Pawn->GetCharacterMovement()->StopMovementImmediately();
		Pawn->GetCharacterMovement()->DisableMovement();
		FCharacterStats Durable = Pawn->GetCharacterStats();
		Durable.MaxHealth = 100000.f;
		Durable.MaxArmor = 0.f;
		Pawn->GetPlayerState<ACombatPlayerState>()->Health->InitializeForSpawn(Durable);
	}
	StartPass();
	UE_LOG(LogTemp, Display, TEXT("[LiveScenario] START Run=%u Kind=%d Moving=%d DistanceM=%d Shots=%d Repeats=%d IntentionalMisses=%d"),
		State.RunId, static_cast<int32>(State.Settings.Kind), State.Settings.bMovingTarget,
		State.Settings.DistanceMeters, State.Settings.ShotsPerRun, State.Settings.Repetitions, State.Settings.bIntentionalMisses);
	return FString();
}

void UCombatScenarioComponent::StartPass()
{
	ACombatGameState* GS = CastChecked<ACombatGameState>(GetOwner());
	State.Stage = ECombatScenarioStage::Preparing;
	for (const FParticipantSnapshot& Snapshot : Participants)
	{
		const ACombatCharacter* Pawn = Snapshot.Pawn.Get();
		if (!Pawn || !Pawn->GetController() || !IsValid(Pawn->GetWeapon()))
		{
			Stop(TEXT("플레이어 상태 변경으로 중지"));
			return;
		}
	}
	State.bRewind = State.Settings.Kind == ECombatExperimentKind::RewindComparison && State.Pass % 2 == 1;
	GS->SetExperiment(State.bRewind, GS->Experiment.PacketLagMs, GS->Experiment.Revision);
	State.Samples = 0;
	LastAcceptedShot = 0;
	State.Stage = ECombatScenarioStage::Preparing;
	State.Epoch = GS->Experiment.CollectAfterServerTime + 2.0;
	for (int32 Index = 0; Index < Participants.Num(); ++Index)
	{
		ACombatCharacter* Pawn = Participants[Index].Pawn.Get();
		const float Side = Index == 0 ? -1.f : 1.f;
		Pawn->SetActorLocation(FVector(Side * State.Settings.DistanceMeters * 50.f, 0.f, 88.f), false, nullptr, ETeleportType::TeleportPhysics);
		Pawn->GetController()->SetControlRotation(FRotator(0.f, Index == 0 ? 0.f : 180.f, 0.f));
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			PC->ClientSetRotation(Pawn->GetControlRotation());
		}
		Pawn->RewindHistory->ResetHistory();
		Pawn->GetWeapon()->Ammo->CancelReload();
		Pawn->GetWeapon()->Ammo->Initialize(Pawn->GetWeapon()->GetStats());
		Pawn->ForceNetUpdate();
	}
	GS->ForceNetUpdate();
	UE_LOG(LogTemp, Display, TEXT("[LiveScenario] PASS Run=%u Pass=%d Rewind=%d Revision=%u LagMs=%d"),
		State.RunId, State.Pass, State.bRewind, GS->Experiment.Revision, GS->Experiment.PacketLagMs);
}

void UCombatScenarioComponent::Stop(const FString& Reason)
{
	if (!GetOwner()->HasAuthority() || !IsBusy() || State.Stage == ECombatScenarioStage::Stopping)
	{
		return;
	}
	bCancelled = true;
	State.Notice = Reason;
	State.Stage = ECombatScenarioStage::Stopping;
	const ACombatGameState* GS = CastChecked<ACombatGameState>(GetOwner());
	// 중지 통지와 마지막 발사 요청이 도착할 때까지 임시 체력과 배치를 유지한다.
	StopAfter = GetWorld()->GetTimeSeconds() + 2.0 + 4.0 * GS->Experiment.PacketLagMs / 1000.0;
	GetOwner()->ForceNetUpdate();
}

void UCombatScenarioComponent::AcceptSample(ACombatPlayerController* PC, uint32 RunId, int32 Pass,
	int32 SampleIndex, const FCombatShotResponse& Response, float AckMs)
{
	ACombatGameState* GS = CastChecked<ACombatGameState>(GetOwner());
	if (!GS->HasAuthority() || State.Stage != ECombatScenarioStage::Running || !PC || PC->PlayerState != State.Shooter ||
		State.RunId != RunId || State.Pass != Pass || SampleIndex != State.Samples + 1 ||
		Response.ExperimentRevision != GS->Experiment.Revision || Response.ShotId <= LastAcceptedShot ||
		!FMath::IsFinite(AckMs) || AckMs < 0.f || AckMs > 30000.f)
	{
		return;
	}
	LastAcceptedShot = Response.ShotId;
	++State.Samples;
	FCombatScenarioModeStats& Stats = State.bRewind ? State.On : State.Off;
	++Stats.Shots;
	Stats.Hits += Response.bDamagedPlayer ? 1 : 0;
	Stats.Rejected += Response.Rejection != EShotRejection::None ? 1 : 0;
	UE_LOG(LogTemp, Display, TEXT("[LiveScenario] SAMPLE Run=%u Pass=%d Index=%d Shot=%u Hit=%d Rejected=%d AckMs=%.3f"),
		RunId, Pass, SampleIndex, Response.ShotId, Response.bDamagedPlayer, static_cast<int32>(Response.Rejection), AckMs);
	if (State.Samples >= State.Settings.ShotsPerRun)
	{
		if (State.Pass + 1 < State.Settings.PassCount())
		{
			++State.Pass;
			StartPass();
		}
		else
		{
			Stop(TEXT("완료"));
			bCancelled = false;
		}
	}
	GS->ForceNetUpdate();
}

void UCombatScenarioComponent::TickServer()
{
	if (!IsBusy()) { return; }
	if (State.Stage == ECombatScenarioStage::Stopping)
	{
		if (GetWorld()->GetTimeSeconds() >= StopAfter)
		{
			RestoreParticipants();
			State.Stage = bCancelled ? ECombatScenarioStage::Cancelled : ECombatScenarioStage::Complete;
			GetOwner()->ForceNetUpdate();
			UE_LOG(LogTemp, Display, TEXT("[LiveScenario] END Run=%u Cancelled=%d Off=%d/%d On=%d/%d"),
				State.RunId, bCancelled, State.Off.Hits, State.Off.Shots, State.On.Hits, State.On.Shots);
		}
		return;
	}
	for (const FParticipantSnapshot& Snapshot : Participants)
	{
		const ACombatCharacter* Pawn = Snapshot.Pawn.Get();
		const ACombatPlayerState* PS = Pawn ? Pawn->GetPlayerState<ACombatPlayerState>() : nullptr;
		if (!Pawn || !Pawn->GetController() || !IsValid(Pawn->GetWeapon()) || !PS || PS->GetPawn() != Pawn ||
			PS->Team == ETeamSide::Neutral || PS->Health->IsDead())
		{
			Stop(TEXT("플레이어 상태 변경으로 중지"));
			return;
		}
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now > State.Epoch + 60.0 + State.Settings.ShotsPerRun * 8.0)
	{
		Stop(TEXT("응답 대기 시간 초과"));
		return;
	}
	if (Now >= State.Epoch && State.Stage == ECombatScenarioStage::Preparing)
	{
		State.Stage = ECombatScenarioStage::Running;
		GetOwner()->ForceNetUpdate();
	}
	FVector Location(State.Settings.DistanceMeters * 50.f, 0.f, 88.f);
	if (State.Settings.bMovingTarget)
	{
		Location.Y = 240.f * FMath::Sin(FMath::Max(0.0, Now - State.Epoch) * 2.0 * PI / 2.4);
	}
	Participants[1].Pawn->SetActorLocation(Location);
	Participants[1].Pawn->ForceNetUpdate();
}

void UCombatScenarioComponent::UnbindFire()
{
	if (BoundFire.IsValid())
	{
		BoundFire->OnShotSubmitted.RemoveAll(this);
		BoundFire->OnShotResult.RemoveAll(this);
		BoundFire->SetADS(false);
	}
	BoundFire.Reset();
	PendingShot = 0;
}

void UCombatScenarioComponent::HandleSubmitted(uint32 ShotId)
{
	PendingShot = ShotId;
	SubmittedAt = FPlatformTime::Seconds();
}

void UCombatScenarioComponent::HandleResult(const FCombatShotResponse& Response)
{
	if (!PendingShot || Response.ShotId != PendingShot || State.Stage != ECombatScenarioStage::Running) { return; }
	PendingShot = 0;
	++LocalSamples;
	if (ACombatPlayerController* PC = LockedController.Get())
	{
		PC->ServerScenarioSample(LocalRunId, LocalPass, LocalSamples, Response,
			static_cast<float>((FPlatformTime::Seconds() - SubmittedAt) * 1000.0));
	}
}

void UCombatScenarioComponent::TickShooter(ACombatPlayerController* PC)
{
	if (LocalRunId != State.RunId || LocalPass != State.Pass)
	{
		UnbindFire();
		LocalRunId = State.RunId;
		LocalPass = State.Pass;
		LocalSamples = 0;
		LastShotAt = -1000.0;
	}
	ACombatCharacter* Pawn = Cast<ACombatCharacter>(PC->GetPawn());
	const ACombatCharacter* Target = State.Target ? Cast<ACombatCharacter>(State.Target->GetPawn()) : nullptr;
	if (!Pawn || !Pawn->GetWeapon() || !Target || !PC->TimeSync->IsReady()) { return; }
	UWeaponFireComponent* Fire = Pawn->GetWeapon()->Fire;
	if (BoundFire.Get() != Fire)
	{
		UnbindFire();
		BoundFire = Fire;
		Fire->OnShotSubmitted.AddUObject(this, &UCombatScenarioComponent::HandleSubmitted);
		Fire->OnShotResult.AddUObject(this, &UCombatScenarioComponent::HandleResult);
		Fire->SetTriggerHeld(false);
		Fire->SetADS(true);
	}
	if (State.Stage != ECombatScenarioStage::Running || !PC->Telemetry->IsMeasuring() || PendingShot ||
		LocalSamples >= State.Settings.ShotsPerRun || GetWorld()->GetTimeSeconds() - LastShotAt < 0.65)
	{
		return;
	}
	if (Pawn->GetWeapon()->Ammo->GetMagazineAmmo() == 0)
	{
		Pawn->GetWeapon()->Ammo->StartReload();
		return;
	}
	if (!Fire->CanFire()) { return; }
	FVector Aim = Target->GetActorLocation();
	const bool bIntentionalMiss = State.Settings.ShouldIntentionallyMiss(LocalSamples);
	if (bIntentionalMiss) { Aim.Z += 300.f; }
	PC->SetControlRotation((Aim - Pawn->GetPawnViewLocation()).Rotation());
	UE_LOG(LogTemp, Display, TEXT("[LiveScenario] AIM Run=%u Pass=%d Index=%d IntentionalMiss=%d Pitch=%.3f"),
		State.RunId, State.Pass, LocalSamples + 1, bIntentionalMiss, PC->GetControlRotation().Pitch);
	LastShotAt = GetWorld()->GetTimeSeconds();
	Fire->FireWeapon();
}

void UCombatScenarioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!IsBusy() && !LockedController.IsValid()) { return; }
	if (GetOwner()->HasAuthority()) { TickServer(); }
	ACombatPlayerController* LocalPC = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ACombatPlayerController* PC = Cast<ACombatPlayerController>(It->Get());
		if (PC && PC->IsLocalController() && IsParticipant(PC)) { LocalPC = PC; break; }
	}
	if (LockedController.Get() != LocalPC)
	{
		UnbindFire();
		if (ACombatPlayerController* Previous = LockedController.Get())
		{
			Previous->SetIgnoreMoveInput(false);
			Previous->SetIgnoreLookInput(false);
		}
		LockedController = LocalPC;
		if (LocalPC)
		{
			LocalPC->SetIgnoreMoveInput(true);
			LocalPC->SetIgnoreLookInput(true);
			if (const ACombatCharacter* Pawn = Cast<ACombatCharacter>(LocalPC->GetPawn()); Pawn && Pawn->GetWeapon())
			{
				Pawn->GetWeapon()->Fire->SetTriggerHeld(false);
				Pawn->GetWeapon()->Fire->SetADS(false);
			}
		}
	}
	if (LocalPC && LocalPC->PlayerState == State.Shooter && State.Stage != ECombatScenarioStage::Stopping)
	{
		TickShooter(LocalPC);
	}
}

void UCombatScenarioComponent::RestoreParticipants()
{
	for (const FParticipantSnapshot& Snapshot : Participants)
	{
		ACombatCharacter* Pawn = Snapshot.Pawn.Get();
		ACombatPlayerState* PS = Pawn ? Pawn->GetPlayerState<ACombatPlayerState>() : nullptr;
		if (!PS || PS->GetPawn() != Pawn || PS->Health->IsDead()) { continue; }
		Pawn->SetActorTransform(Snapshot.Transform, false, nullptr, ETeleportType::TeleportPhysics);
		Pawn->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(Snapshot.MovementMode), Snapshot.CustomMovementMode);
		PS->Health->RestoreExperimentVitals(Snapshot.Stats, Snapshot.Health, Snapshot.Armor);
		if (Pawn->GetController()) { Pawn->GetController()->SetControlRotation(Snapshot.Aim); }
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController())) { PC->ClientSetRotation(Snapshot.Aim); }
		if (IsValid(Pawn->GetWeapon()))
		{
			Pawn->GetWeapon()->Fire->SetTriggerHeld(false);
			Pawn->GetWeapon()->Fire->SetADS(false);
			Pawn->GetWeapon()->Ammo->RestoreExperimentAmmo(Snapshot.Magazine, Snapshot.Reserve);
		}
		Pawn->RewindHistory->ResetHistory();
		Pawn->ForceNetUpdate();
	}
	Participants.Reset();
}

void UCombatScenarioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	UnbindFire();
	if (ACombatPlayerController* PC = LockedController.Get())
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}
	if (GetOwner()->HasAuthority()) { RestoreParticipants(); }
	Super::EndPlay(Reason);
}

FString UCombatScenarioComponent::GetProgressLabel() const
{
	const TCHAR* Label = State.Stage == ECombatScenarioStage::Preparing ? TEXT("안정화 중") :
		State.Stage == ECombatScenarioStage::Running ? TEXT("자동 사격 중") :
		State.Stage == ECombatScenarioStage::Stopping ? TEXT("종료·복구 중") :
		State.Stage == ECombatScenarioStage::Complete ? TEXT("완료") :
		State.Stage == ECombatScenarioStage::Cancelled ? TEXT("중지됨") : TEXT("대기");
	if (State.RunId == 0) { return Label; }
	return FString::Printf(TEXT("%s | %d/%d구간 | %d/%d발"), Label, State.Pass + 1, State.Settings.PassCount(),
		State.Samples, State.Settings.ShotsPerRun);
}
