#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "CombatTestWorld.h"
#include "Components/RewindHistoryComponent.h"
#include "Components/WeaponHitValidationComponent.h"
#include "Data/CombatShotCommand.h"
#include "Data/CombatNetworkDataAsset.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewindBufferTest, "LatencyCombat.Rewind.HistoryBoundariesAndInterpolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRewindBufferTest::RunTest(const FString& Parameters)
{
	FRewindHistoryBuffer Buffer;
	Buffer.Reset(3);
	FRewindFrame Frame, Sample;
	Frame.LifeId = FGuid::NewGuid();
	Frame.bAlive = true;
	Frame.Radius = 34.f;
	Frame.HalfHeight = 88.f;
	TestFalse(TEXT("Empty history does not invent a pose"), Buffer.Sample(0, Sample, 300));
	for (int32 Index = 0; Index <= 4; ++Index)
	{
		Frame.Time = Index;
		Frame.Location = FVector(Index * 100, 0, 0);
		Frame.ViewLocation = Frame.Location + FVector(0, 0, 64);
		Buffer.Add(Frame);
	}
	TestEqual(TEXT("Ring allocation remains bounded"), Buffer.Num(), 3);
	TestFalse(TEXT("Overwritten frame is unavailable"), Buffer.Sample(1.99, Sample, 300));
	TestFalse(TEXT("Future pose is not extrapolated"), Buffer.Sample(4.01, Sample, 300));
	TestTrue(TEXT("Oldest boundary is inclusive"), Buffer.Sample(2.0, Sample, 300));
	TestTrue(TEXT("Newest boundary is inclusive"), Buffer.Sample(4.0, Sample, 300));
	TestTrue(TEXT("Inter-frame pose can be sampled"), Buffer.Sample(2.5, Sample, 300));
	TestTrue(TEXT("Capsule center is interpolated"), Sample.Location.Equals(FVector(250, 0, 0)));
	TestTrue(TEXT("Historical view origin is interpolated"), Sample.ViewLocation.Equals(FVector(250, 0, 64)));
	Frame.Time = 5;
	Frame.Location.X = 1000;
	Buffer.Add(Frame);
	TestFalse(TEXT("Teleport is not interpolated"), Buffer.Sample(4.5, Sample, 300));
	Frame.Time = 6;
	Frame.LifeId = FGuid::NewGuid();
	Buffer.Add(Frame);
	TestFalse(TEXT("Life boundary is not interpolated"), Buffer.Sample(5.5, Sample, 300));
	Frame.Time = 7;
	Frame.bAlive = false;
	Buffer.Add(Frame);
	TestFalse(TEXT("Death boundary is not interpolated"), Buffer.Sample(6.5, Sample, 300));
	Frame.Time = 8;
	Frame.Team = ETeamSide::Blue;
	Buffer.Add(Frame);
	TestFalse(TEXT("Team boundary is not interpolated"), Buffer.Sample(7.5, Sample, 300));
	TestFalse(TEXT("NaN query rejected"), Buffer.Sample(std::numeric_limits<double>::quiet_NaN(), Sample, 300));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewindCapsuleTest, "LatencyCombat.Rewind.CapsuleIntersection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRewindCapsuleTest::RunTest(const FString& Parameters)
{
	FRewindFrame Frame;
	Frame.Location = FVector(100, 0, 0);
	Frame.Radius = 20;
	Frame.HalfHeight = 50;
	double Distance = 0;
	TestTrue(TEXT("Cylinder hit"),
		UWeaponHitValidationComponent::IntersectCapsule(
			FVector::ZeroVector, FVector::ForwardVector, 200, Frame, Distance));
	TestEqual(TEXT("Nearest intersection distance"), Distance, 80.0);
	TestFalse(TEXT("Range enforced"),
		UWeaponHitValidationComponent::IntersectCapsule(
			FVector::ZeroVector, FVector::ForwardVector, 79, Frame, Distance));
	TestFalse(TEXT("Outside capsule misses"),
		UWeaponHitValidationComponent::IntersectCapsule(
			FVector(0, 21, 0), FVector::ForwardVector, 200, Frame, Distance));
	TestTrue(TEXT("Tangent ray hits"),
		UWeaponHitValidationComponent::IntersectCapsule(
			FVector(0, 20, 0), FVector::ForwardVector, 200, Frame, Distance));
	TestTrue(TEXT("Vertical cap hit"),
		UWeaponHitValidationComponent::IntersectCapsule(
			FVector(100, 0, 100), -FVector::UpVector, 200, Frame, Distance));
	TestEqual(TEXT("Rounded cap distance"), Distance, 50.0);
	TestTrue(TEXT("Inside starts at zero distance"),
		UWeaponHitValidationComponent::IntersectCapsule(Frame.Location, FVector::ForwardVector, 200, Frame, Distance));
	TestEqual(TEXT("Inside distance"), Distance, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewindCommandTest, "LatencyCombat.Rewind.UntrustedCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRewindCommandTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	ACombatCharacter* Pawn = Cast<ACombatCharacter>(PC->GetPawn());
	UWeaponFireComponent* Fire = Pawn->GetWeapon()->Fire;
	Fixture.Advance(1.f);
	FCombatShotCommand Command;
	Command.ShotId = 1;
	Command.LifeId = Pawn->GetLifeId();
	Command.ServerFireTime = Fixture.World->GetTimeSeconds();
	TestTrue(TEXT("Valid command accepted"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::None);
	const int32 Ammo = Pawn->GetWeapon()->Ammo->GetMagazineAmmo();
	TestTrue(
		TEXT("Duplicate ID rejected"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::InvalidSequence);
	Command.ShotId = 0;
	TestTrue(TEXT("Zero ID rejected"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::InvalidSequence);
	Command.ShotId = 2;
	Command.LifeId = FGuid::NewGuid();
	TestTrue(TEXT("Other life rejected"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::InvalidLife);
	Command.LifeId = Pawn->GetLifeId();
	Command.ServerFireTime = std::numeric_limits<double>::quiet_NaN();
	TestTrue(
		TEXT("Nonfinite time rejected"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::InvalidTime);
	Command.ShotId++;
	Command.ServerFireTime = Fixture.World->GetTimeSeconds() + 1;
	TestTrue(TEXT("Future time rejected"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::Future);
	Command.ShotId++;
	Command.ServerFireTime = Fixture.World->GetTimeSeconds() - 1;
	TestTrue(TEXT("Expired time rejected"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::TooOld);
	Command.ShotId++;
	Command.ServerFireTime = Fixture.World->GetTimeSeconds() - 0.1;
	TestTrue(
		TEXT("Reversed time rejected"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::OutOfOrderTime);
	Command.ShotId++;
	Command.ServerFireTime = Fixture.World->GetTimeSeconds();
	Command.AimRotation.Yaw = std::numeric_limits<double>::infinity();
	TestTrue(TEXT("Nonfinite aim rejected"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::InvalidAim);
	Command.ShotId++;
	Command.AimRotation = FRotator::ZeroRotator;
	TestTrue(TEXT("Too-fast fire rejected"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::FireRate);
	TestEqual(TEXT("Rejected commands consume no ammo"), Pawn->GetWeapon()->Ammo->GetMagazineAmmo(), Ammo);
	Pawn->GetWeapon()->SetRole(ROLE_SimulatedProxy);
	TestTrue(TEXT("Client cannot execute authority entry"),
		Fire->ProcessShotCommand(Command).Rejection == EShotRejection::InvalidOwner);
	Pawn->GetWeapon()->SetRole(ROLE_Authority);
	Fixture.World->GetGameState<ACombatGameState>()->bMatchEnded = true;
	Fixture.Advance(0.2f);
	Command.ShotId++;
	Command.ServerFireTime = Fixture.World->GetTimeSeconds();
	TestTrue(
		TEXT("Match end blocks damage"), Fire->ProcessShotCommand(Command).Rejection == EShotRejection::CannotFire);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewindMovingTargetTest, "LatencyCombat.Rewind.MovingTargetOnOff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRewindMovingTargetTest::RunTest(const FString& Parameters)
{
	for (int32 Enabled = 0; Enabled <= 1; ++Enabled)
	{
		FFPSBaselineTestWorld Fixture;
		Fixture.World->GetGameState<ACombatGameState>()->LagCompensation.bEnabled = Enabled != 0;
		ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
		ACombatPlayerController* TargetPC = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Blue);
		ACombatCharacter* Shooter = Cast<ACombatCharacter>(PC->GetPawn());
		ACombatCharacter* Target = Cast<ACombatCharacter>(TargetPC->GetPawn());
		Shooter->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Target->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Fixture.Advance(0.2f);
		FCombatShotCommand Command;
		Command.ShotId = 1;
		Command.LifeId = Shooter->GetLifeId();
		Command.ServerFireTime = Fixture.World->GetTimeSeconds();
		Command.AimRotation = (Target->GetActorLocation() - Shooter->GetPawnViewLocation()).Rotation();
		Target->RewindHistory->RecordNow();
		Fixture.Advance(0.2f);
		Target->SetActorLocation(FVector(1000, 200, 300));
		const FVector Before = Target->GetActorLocation();
		const FCombatShotResponse Response = Shooter->GetWeapon()->Fire->ProcessShotCommand(Command);
		TestTrue(TEXT("Command accepted for both modes"), Response.Rejection == EShotRejection::None);
		TestEqual(TEXT("Past hit recovered only when enabled"), Response.bDamagedPlayer, Enabled != 0);
		TestEqual(TEXT("Mode is reported"), Response.bUsedRewind, Enabled != 0);
		TestTrue(TEXT("Query never moves target"), Target->GetActorLocation().Equals(Before));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewindLifecycleTest, "LatencyCombat.Rewind.RespawnAndDisconnect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRewindLifecycleTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	Fixture.World->GetAuthGameMode<ACombatGameMode>()->RespawnDelay = 0.1f;
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	ACombatPlayerController* TargetPC = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Blue);
	ACombatCharacter* Shooter = Cast<ACombatCharacter>(PC->GetPawn());
	ACombatCharacter* Target = Cast<ACombatCharacter>(TargetPC->GetPawn());
	Fixture.Advance(0.2f);
	FCombatShotCommand Command;
	Command.ShotId = 1;
	Command.LifeId = Shooter->GetLifeId();
	Command.ServerFireTime = Fixture.World->GetTimeSeconds();
	Command.AimRotation = (Target->GetActorLocation() - Shooter->GetPawnViewLocation()).Rotation();
	const FGuid OldTargetLife = Target->GetLifeId();
	Target->TakeDamage(1000, FDamageEvent(), PC, Shooter->GetWeapon());
	Fixture.Advance(0.2f);
	ACombatCharacter* Respawned = Cast<ACombatCharacter>(TargetPC->GetPawn());
	TestNotNull(TEXT("Target respawned"), Respawned);
	if (!Respawned)
	{
		return false;
	}
	Respawned->SetActorLocation(FVector(1000, 0, 300));
	TestTrue(TEXT("New pawn has a fresh life ID"), Respawned->GetLifeId() != OldTargetLife);
	TestFalse(TEXT("Old target history cannot damage new pawn"),
		Shooter->GetWeapon()->Fire->ProcessShotCommand(Command).bDamagedPlayer);
	TestEqual(TEXT("Respawn health remains full"), TargetPC->GetPlayerState<ACombatPlayerState>()->Health->GetHealth(),
		100.f);
	Fixture.Advance(0.2f);
	Command.ShotId++;
	Command.ServerFireTime = Fixture.World->GetTimeSeconds();
	TargetPC->UnPossess();
	TestFalse(TEXT("Disconnected target cannot take rewind damage"),
		Shooter->GetWeapon()->Fire->ProcessShotCommand(Command).bDamagedPlayer);
	const FGuid OldShooterLife = Shooter->GetLifeId();
	Shooter->TakeDamage(1000, FDamageEvent(), nullptr, nullptr);
	Fixture.Advance(0.2f);
	ACombatCharacter* NewShooter = Cast<ACombatCharacter>(PC->GetPawn());
	if (!TestNotNull(TEXT("Shooter respawned"), NewShooter))
	{
		return false;
	}
	Command.LifeId = OldShooterLife;
	Command.ShotId++;
	Command.ServerFireTime = Fixture.World->GetTimeSeconds();
	TestTrue(TEXT("Old life command on new weapon rejected"),
		NewShooter->GetWeapon()->Fire->ProcessShotCommand(Command).Rejection == EShotRejection::InvalidLife);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewindSettingsTest, "LatencyCombat.Rewind.DataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRewindSettingsTest::RunTest(const FString& Parameters)
{
	TestNotNull(
		TEXT("Default GameMode loads the editable network asset"), GetDefault<ACombatGameMode>()->NetworkData.Get());
	FLagCompensationSettings Settings;
	Settings.MaxRewindSeconds = std::numeric_limits<float>::quiet_NaN();
	Settings.RecordInterval = 0.f;
	Settings.FutureTolerance = -1.f;
	Settings.MaxTargetViewDelay = 100.f;
	Settings.DisplayDelaySeconds = std::numeric_limits<float>::infinity();
	Settings = Settings.Normalized();
	TestEqual(TEXT("Invalid rewind falls back to the shared default"), Settings.MaxRewindSeconds,
		FLagCompensationSettings().MaxRewindSeconds);
	TestTrue(TEXT("Recording period remains bounded"), Settings.RecordInterval >= 0.008f);
	TestEqual(TEXT("Negative tolerance clamped"), Settings.FutureTolerance, 0.f);
	TestEqual(TEXT("Target view delay is server bounded"), Settings.MaxTargetViewDelay, 0.5f);
	TestTrue(TEXT("Smoothing delay remains finite"), FMath::IsFinite(Settings.DisplayDelaySeconds));
	UWeaponDataAsset* Weapon = NewObject<UWeaponDataAsset>();
	Weapon->Stats.FireRate = std::numeric_limits<float>::quiet_NaN();
	Weapon->Stats.BaseSpread = std::numeric_limits<float>::infinity();
	Weapon->Stats.Damage = -10.f;
	const FWeaponStats Validated = FCombatRules::ReadWeaponStats(Weapon);
	TestEqual(TEXT("NaN fire rate cannot bypass fire limits"), Validated.FireRate, FWeaponStats().FireRate);
	TestTrue(TEXT("Spread remains finite"), FMath::IsFinite(Validated.BaseSpread));
	TestEqual(TEXT("Negative damage clamped"), Validated.Damage, 0.f);
	Weapon->Stats.MaxRange = 10.f;
	Weapon->Stats.RangeFalloffStart = std::numeric_limits<float>::quiet_NaN();
	Weapon->Stats.RangeFalloffEnd = std::numeric_limits<float>::quiet_NaN();
	const FWeaponStats ShortRange = FCombatRules::ReadWeaponStats(Weapon);
	TestTrue(TEXT("Fallback falloff still respects configured range"),
		ShortRange.RangeFalloffStart <= ShortRange.RangeFalloffEnd &&
			ShortRange.RangeFalloffEnd <= ShortRange.MaxRange);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewindNearestAndLifetimeTest, "LatencyCombat.Rewind.NearestHitAndDamageRecheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRewindNearestAndLifetimeTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* ShooterPC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	ACombatPlayerController* NearPC = Fixture.SpawnPlayer(FVector(500, 0, 300), ETeamSide::Blue);
	ACombatPlayerController* FarPC = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Blue);
	ACombatCharacter* Shooter = Cast<ACombatCharacter>(ShooterPC->GetPawn());
	FCombatShotCommand Command;
	Command.ShotId = 1;
	Command.LifeId = Shooter->GetLifeId();
	Command.ServerFireTime = Fixture.World->GetTimeSeconds();
	FHitResult Hit;
	FVector Start, End, Direction;
	FCombatShotResponse Response;
	UWeaponHitValidationComponent* Validation = Shooter->GetWeapon()->HitValidation;
	TestTrue(TEXT("Valid geometric query"),
		Validation->Resolve(Command, Hit, Start, End, Direction, Response) == EShotRejection::None);
	TestTrue(TEXT("Nearest past capsule is selected"), Hit.GetActor() == NearPC->GetPawn());
	NearPC->UnPossess();
	TestEqual(TEXT("Disconnect between query and damage blocks damage"),
		Validation->ApplyValidatedDamage(Hit, Direction), 0.f);
	TestEqual(TEXT("Farther player remains undamaged"),
		FarPC->GetPlayerState<ACombatPlayerState>()->Health->GetHealth(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewindADSAndTriggerTest, "LatencyCombat.Rewind.ADSAndAutomaticFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRewindADSAndTriggerTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	// 로컬 입력과 카메라를 검사하는 테스트라 LocalPlayer도 붙여줌.
	PC->Player = NewObject<ULocalPlayer>(GEngine);
	ACombatCharacter* Pawn = Cast<ACombatCharacter>(PC->GetPawn());
	UWeaponFireComponent* Fire = Pawn->GetWeapon()->Fire;
	Fire->SetADS(true);
	Fixture.Advance(0.4f);
	TestTrue(TEXT("ADS state reaches the owning weapon"), Fire->IsADS());
	TestTrue(TEXT("ADS zoom reaches camera"), Pawn->FirstPersonCamera->FieldOfView < 50.f);
	Fire->SetTriggerHeld(true);
	Fixture.Advance(0.5f);
	const int32 AfterBurst = Pawn->GetWeapon()->Ammo->GetMagazineAmmo();
	TestTrue(TEXT("Held trigger produces a burst"), AfterBurst < Pawn->GetWeapon()->GetStats().MagazineSize - 1);
	TestTrue(TEXT("Server rate bounds burst length"), AfterBurst >= Pawn->GetWeapon()->GetStats().MagazineSize - 5);
	Fire->SetTriggerHeld(false);
	Fire->SetADS(false);
	Fixture.Advance(0.4f);
	TestEqual(TEXT("Released trigger stops firing"), Pawn->GetWeapon()->Ammo->GetMagazineAmmo(), AfterBurst);
	TestTrue(TEXT("ADS release restores FOV"), Pawn->FirstPersonCamera->FieldOfView > 85.f);
	Pawn->GetWeapon()->Ammo->StartReload();
	FCombatShotCommand Command;
	Command.ShotId = 100;
	Command.LifeId = Pawn->GetLifeId();
	Command.ServerFireTime = Fixture.World->GetTimeSeconds();
	TestTrue(TEXT("Server blocks bypass attempt while reloading"),
		Fire->ProcessShotCommand(Command).Rejection == EShotRejection::CannotFire);
	Pawn->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	Command.ShotId++;
	TestTrue(TEXT("Server blocks bypass attempt after death"),
		Fire->ProcessShotCommand(Command).Rejection == EShotRejection::CannotFire);
	TestEqual(
		TEXT("Blocked requests leave magazine unchanged"), Pawn->GetWeapon()->Ammo->GetMagazineAmmo(), AfterBurst);
	return true;
}
#endif
