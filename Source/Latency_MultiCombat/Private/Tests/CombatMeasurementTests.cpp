#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CombatShotSample.h"
#include "CombatTestWorld.h"
#include "Components/CharacterPresentationComponent.h"
#include "Components/CombatTelemetryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMeasurementTest, "LatencyCombat.Measurement.Correlation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatMeasurementTest::RunTest(const FString& Parameters)
{
	FCombatShotSample Sample;
	Sample.ShotId = 42;
	Sample.StartedAt = 10.0;
	Sample.HealthBefore = 100.f;
	TestFalse(TEXT("Foreign feedback rejected"), Sample.Feedback(41, true, 10.1));
	TestFalse(TEXT("Negative elapsed feedback rejected"), Sample.Feedback(42, true, 9.9));
	TestFalse(TEXT("Foreign confirmation rejected"), Sample.Confirm(41, 10.1));
	Sample.ObserveHealth(100.f, 10.1);
	TestEqual(TEXT("Unchanged health is not a response"), Sample.HealthMs, -1.0);
	Sample.ObserveHealth(75.f, 10.2);
	TestTrue(TEXT("Confirmation can precede feedback"), Sample.Confirm(42, 10.3));
	TestFalse(TEXT("Await feedback even with HP and confirmation"), Sample.IsComplete());
	TestTrue(TEXT("Matching feedback accepted"), Sample.Feedback(42, true, 10.4));
	TestTrue(TEXT("All hit responses complete"), Sample.IsComplete());
	TestFalse(TEXT("Duplicate feedback rejected"), Sample.Feedback(42, false, 11.0));
	TestFalse(TEXT("Duplicate confirmation rejected"), Sample.Confirm(42, 11.0));
	Sample.ObserveHealth(50.f, 11.0);
	TestEqual(TEXT("First HP response preserved"), Sample.HealthAfter, 75.f);
	TestTrue(TEXT("Same monotonic clock elapsed"), FMath::IsNearlyEqual(Sample.HealthMs, 200.0, 0.0001));
	FCombatShotSample Miss;
	Miss.ShotId = 7;
	Miss.Feedback(7, false, 0.1);
	TestTrue(TEXT("Miss needs no HP change or confirmation"), Miss.IsComplete());
	TestEqual(TEXT("Missing response remains sentinel"), Miss.HealthMs, -1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatTelemetryLifecycleTest, "LatencyCombat.Measurement.TelemetryAndRespawnRebind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatTelemetryLifecycleTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	Fixture.World->GetAuthGameMode<ACombatGameMode>()->RespawnDelay = 0.1f;
	ACombatPlayerController* ShooterPC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	ACombatPlayerController* TargetPC = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Blue);
	UCombatTelemetryComponent* Telemetry = ShooterPC->Telemetry;
	if (!TestNotNull(TEXT("PlayerController owns local telemetry component"), Telemetry))
	{
		return false;
	}
	TestFalse(TEXT("Telemetry never replicates"), Telemetry->GetIsReplicated());
	Telemetry->RefreshWeaponBinding();
	Telemetry->SampleNetworkMetrics(0.016f, true);
	TestEqual(TEXT("Initial network sample recorded"), Telemetry->GetNetSampleCount(), 1);

	ACombatCharacter* FirstPawn = Cast<ACombatCharacter>(ShooterPC->GetPawn());
	ShooterPC->SetControlRotation(FRotator::ZeroRotator);
	FirstPawn->GetWeapon()->Fire->FireWeapon();
	TestEqual(TEXT("Hit shot recorded in telemetry"), Telemetry->GetShotSampleCount(), 1);
	FCombatShotTelemetry LastShot;
	TestTrue(TEXT("Last shot available"), Telemetry->GetLastShot(LastShot));
	TestTrue(TEXT("First shot hit recorded"), LastShot.bDamagedPlayer && LastShot.Rejection == EShotRejection::None);

	// 리스폰 후 새 Pawn과 새 무기가 생겨도 이전 통계는 유지하고 새 무기 델리게이트로 자동 재연결되어야 함.
	FirstPawn->TakeDamage(1000.f, FDamageEvent(), TargetPC, nullptr);
	Fixture.Advance(0.25f);
	ACombatCharacter* RespawnedPawn = Cast<ACombatCharacter>(ShooterPC->GetPawn());
	if (!TestNotNull(TEXT("Shooter respawned with new pawn"), RespawnedPawn) || RespawnedPawn == FirstPawn)
	{
		return false;
	}
	Telemetry->RefreshWeaponBinding();
	ShooterPC->SetControlRotation(FRotator(45.f, 0.f, 0.f));
	RespawnedPawn->GetWeapon()->Fire->FireWeapon();
	const FCombatTelemetrySummary Summary = Telemetry->GetSummary();
	TestEqual(TEXT("Shot history persists across respawn"), Summary.TotalSubmitted, 2);
	TestEqual(TEXT("Accepted count tracks both lives"), Summary.TotalAccepted, 2);
	TestEqual(TEXT("Hit count distinguishes hit and miss"), Summary.TotalHits, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkeletalPresentationTest,
	"LatencyCombat.Structure.SkeletalCharacterAndWeaponPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkeletalPresentationTest::RunTest(const FString& Parameters)
{
	UCharacterDataAsset* DefaultChar =
		LoadObject<UCharacterDataAsset>(nullptr, TEXT("/Game/Data/DA_DefaultCharacter.DA_DefaultCharacter"));
	UWeaponDataAsset* DefaultWeapon =
		LoadObject<UWeaponDataAsset>(nullptr, TEXT("/Game/Data/DA_DefaultWeapon.DA_DefaultWeapon"));
	if (!TestNotNull(TEXT("Default character data asset exists"), DefaultChar) ||
		!TestNotNull(TEXT("Default weapon data asset exists"), DefaultWeapon))
	{
		return false;
	}
	TestFalse(TEXT("Default character references UE skeletal mesh"), DefaultChar->CharacterSkelMesh.IsNull());
	TestFalse(TEXT("Default weapon references UE skeletal mesh"), DefaultWeapon->WeaponSkeletalMesh.IsNull());
	TestTrue(TEXT("Default mesh offset lowers the body by 90 cm"),
		DefaultChar->MeshLocationOffset.Equals(FVector(0.f, 0.f, -90.f)));
	TestTrue(TEXT("Default rotation applies -90 to yaw, not pitch"),
		DefaultChar->MeshRotationOffset.Equals(FRotator(0.f, -90.f, 0.f)));
	TestTrue(TEXT("Default character uses rifle locomotion"),
		DefaultChar->CharacterAnimClass.ToString().Contains(TEXT("ABP_CombatRifle")));
	TestFalse(
		TEXT("Default weapon supplies a character fire animation"), DefaultWeapon->CharacterFireAnimation.IsNull());
	UBlendSpace* Locomotion =
		LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Rifle/BS_CombatRifle.BS_CombatRifle"));
	if (!TestNotNull(TEXT("Rifle locomotion blend space exists"), Locomotion))
	{
		return false;
	}
	for (const FBlendSample& Sample : Locomotion->GetBlendSamples())
	{
		TestTrue(TEXT("Saved locomotion samples use rifle animations"),
			Sample.Animation && Sample.Animation->GetPathName().Contains(TEXT("/Rifle/")));
	}

	FFPSBaselineTestWorld Fixture;
	ACombatCharacter* Pawn =
		Fixture.World->SpawnActorDeferred<ACombatCharacter>(ACombatCharacter::StaticClass(), FTransform::Identity);
	Pawn->CharacterData = DefaultChar;
	UGameplayStatics::FinishSpawningActor(Pawn, FTransform(FVector(0, 0, 300)));
	ACombatPlayerController* PC = Fixture.World->SpawnActor<ACombatPlayerController>();
	PC->SetAsLocalPlayerController();
	PC->GetPlayerState<ACombatPlayerState>()->SetTeam(ETeamSide::Red);
	PC->Possess(Pawn);

	ACombatWeapon* Weapon = Pawn->GetWeapon();
	if (!TestNotNull(TEXT("Weapon spawned for skeletal character"), Weapon))
	{
		return false;
	}
	TestTrue(TEXT("Character presentation switched to skeletal body"), Pawn->Presentation->UsesSkeletalBody());
	TestNotNull(TEXT("Character GetMesh has skeletal asset"), Pawn->GetMesh()->GetSkeletalMeshAsset());
	TestTrue(TEXT("Spawn applies the configured mesh location"),
		Pawn->GetMesh()->GetRelativeLocation().Equals(DefaultChar->MeshLocationOffset));
	TestTrue(TEXT("Spawn applies the configured mesh rotation"),
		Pawn->GetMesh()->GetRelativeRotation().Equals(DefaultChar->MeshRotationOffset));
	TestTrue(TEXT("Network smoothing preserves the configured mesh location"),
		Pawn->GetBaseTranslationOffset().Equals(DefaultChar->MeshLocationOffset));
	TestTrue(TEXT("Network smoothing preserves the configured mesh rotation"),
		Pawn->GetBaseRotationOffset().Equals(DefaultChar->MeshRotationOffset.Quaternion()));
	TestTrue(TEXT("Skeletal mesh has no trace/physics collision"),
		Pawn->GetMesh()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Legacy cylinder proxy hidden when skeletal mesh active"), Pawn->BodyProxy->IsVisible());
	TestTrue(TEXT("Weapon presentation uses skeletal weapon"), Weapon->Presentation->UsesSkeletalWeapon());
	TestNotNull(TEXT("1P weapon skeletal mesh assigned"), Weapon->FirstPersonMesh->GetSkeletalMeshAsset());
	TestNotNull(TEXT("3P weapon skeletal mesh assigned"), Weapon->ThirdPersonMesh->GetSkeletalMeshAsset());
	TestTrue(TEXT("1P weapon only visible to owner"), Weapon->FirstPersonMesh->bOnlyOwnerSee);
	TestTrue(TEXT("3P weapon hidden from owner"), Weapon->ThirdPersonMesh->bOwnerNoSee);
	TestTrue(TEXT("3P weapon attached to character skeletal mesh"),
		Weapon->ThirdPersonMesh->GetAttachParent() == Pawn->GetMesh());
	TestFalse(
		TEXT("3P weapon attached to right-hand socket/bone"), Weapon->ThirdPersonMesh->GetAttachSocketName().IsNone());
	TestTrue(TEXT("First-person rifle rotation uses yaw without tipping the barrel down"),
		FMath::IsNearlyZero(DefaultWeapon->FirstPersonRotationOffset.Pitch));

	// Exercise the accepted-shot multicast path, rather than playing a montage directly.
	UAnimInstance* Anim = Pawn->GetMesh()->GetAnimInstance();
	if (!TestNotNull(TEXT("Rifle animation instance exists"), Anim))
	{
		return false;
	}
	Pawn->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	const FVector RestLocation = Weapon->FirstPersonMesh->GetRelativeLocation();
	const FQuat RestRotation = Weapon->FirstPersonMesh->GetRelativeRotation().Quaternion();
	PC->SetControlRotation(FRotator::ZeroRotator);
	Weapon->Fire->FireWeapon();
	TestEqual(
		TEXT("Accepted shot consumes a round"), Weapon->Ammo->GetMagazineAmmo(), Weapon->GetStats().MagazineSize - 1);
	TestTrue(TEXT("Accepted shot starts the character fire montage"), Anim->IsAnyMontagePlaying());
	TestTrue(TEXT("Accepted shot kicks the locally controlled first-person weapon"),
		Weapon->FirstPersonMesh->GetRelativeLocation().X < RestLocation.X);
	TestFalse(TEXT("First-person recoil rotates the weapon in camera space"),
		Weapon->FirstPersonMesh->GetRelativeRotation().Quaternion().Equals(RestRotation));
	Fixture.Advance(0.05f);
	TestTrue(TEXT("Rifle AnimBP evaluates the fire montage slot"),
		Anim->GetSlotMontageGlobalWeight(DefaultWeapon->CharacterAnimationSlot) > 0.f);
	Fixture.Advance(0.5f);
	TestTrue(TEXT("First-person recoil recovers to the data asset offset"),
		Weapon->FirstPersonMesh->GetRelativeLocation().Equals(RestLocation, 0.01f));
	TestTrue(TEXT("First-person recoil recovers to the data asset rotation"),
		Weapon->FirstPersonMesh->GetRelativeRotation().Quaternion().Equals(RestRotation, 0.001f));
	Weapon->Fire->FireWeapon();
	TestTrue(TEXT("Subsequent accepted shots replay the character fire montage"), Anim->IsAnyMontagePlaying());

	Pawn->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	TestFalse(TEXT("1P weapon hidden on death"), Weapon->FirstPersonMesh->IsVisible());
	TestFalse(TEXT("3P weapon hidden on death"), Weapon->ThirdPersonMesh->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMeshConstructionTest,
	"LatencyCombat.Structure.CharacterMeshConfiguredBeforePlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCharacterMeshConstructionTest::RunTest(const FString& Parameters)
{
	const UCharacterDataAsset* DefaultData =
		LoadObject<UCharacterDataAsset>(nullptr, TEXT("/Game/Data/DA_DefaultCharacter.DA_DefaultCharacter"));
	if (!TestNotNull(TEXT("Default character data exists"), DefaultData))
	{
		return false;
	}
	const ACombatCharacter* DefaultCharacter = GetDefault<ACombatCharacter>();
	TestTrue(TEXT("Native mesh defaults expose the configured location"),
		DefaultCharacter->GetMesh()->GetRelativeLocation().Equals(DefaultData->MeshLocationOffset));
	TestTrue(TEXT("Native mesh defaults expose the configured rotation"),
		DefaultCharacter->GetMesh()->GetRelativeRotation().Equals(DefaultData->MeshRotationOffset));

	UWorld* EditorWorld = UWorld::CreateWorld(EWorldType::Editor, false);
	ACombatCharacter* Pawn =
		EditorWorld->SpawnActorDeferred<ACombatCharacter>(ACombatCharacter::StaticClass(), FTransform::Identity);
	UCharacterDataAsset* CustomData = DuplicateObject<UCharacterDataAsset>(DefaultData, Pawn);
	CustomData->MeshLocationOffset = FVector(4.f, 8.f, -96.f);
	CustomData->MeshRotationOffset = FRotator(0.f, -75.f, 0.f);
	Pawn->CharacterData = CustomData;
	UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
	TestFalse(TEXT("Editor construction runs before BeginPlay"), Pawn->HasActorBegunPlay());
	TestNotNull(TEXT("Editor construction loads the skeletal body"), Pawn->GetMesh()->GetSkeletalMeshAsset());
	TestTrue(TEXT("Editor construction honors character-specific location"),
		Pawn->GetMesh()->GetRelativeLocation().Equals(CustomData->MeshLocationOffset));
	TestTrue(TEXT("Editor construction honors character-specific rotation"),
		Pawn->GetMesh()->GetRelativeRotation().Equals(CustomData->MeshRotationOffset));
	TestTrue(TEXT("Editor construction updates smoothing's translation baseline"),
		Pawn->GetBaseTranslationOffset().Equals(CustomData->MeshLocationOffset));
	TestTrue(TEXT("Editor construction updates smoothing's rotation baseline"),
		Pawn->GetBaseRotationOffset().Equals(CustomData->MeshRotationOffset.Quaternion()));
	EditorWorld->DestroyWorld(false);
	return true;
}
#endif
