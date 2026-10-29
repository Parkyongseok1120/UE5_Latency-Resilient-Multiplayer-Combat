#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CombatTestWorld.h"
#include "Animation/CombatAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
ACombatCharacter* SpawnAnimatedPlayer(FFPSBaselineTestWorld& Fixture, FAutomationTestBase& Test)
{
	UCharacterDataAsset* Data =
		LoadObject<UCharacterDataAsset>(nullptr, TEXT("/Game/Data/DA_DefaultCharacter.DA_DefaultCharacter"));
	if (!Test.TestNotNull(TEXT("Default character data exists"), Data))
	{
		return nullptr;
	}
	ACombatCharacter* Pawn = Fixture.World->SpawnActorDeferred<ACombatCharacter>(
		ACombatCharacter::StaticClass(), FTransform(FVector(0.f, 0.f, 300.f)));
	Pawn->CharacterData = Data;
	UGameplayStatics::FinishSpawningActor(Pawn, Pawn->GetActorTransform());
	ACombatPlayerController* PC = Fixture.World->SpawnActor<ACombatPlayerController>();
	PC->SetAsLocalPlayerController();
	PC->GetPlayerState<ACombatPlayerState>()->SetTeam(ETeamSide::Red);
	PC->Possess(Pawn);
	Pawn->GetCharacterMovement()->DisableMovement();
	Pawn->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Fixture.Advance(0.4f);
	return Pawn;
}

float GripError(const ACombatCharacter* Pawn)
{
	// World::Tick alone need not finalize a parallel pose before these test-side socket reads.
	Pawn->GetMesh()->GetBoneSpaceTransforms();
	const ACombatWeapon* Weapon = Pawn->GetWeapon();
	const FTransform Grip = Weapon->WeaponData->LeftHandGripOffset *
		Weapon->ThirdPersonMesh->GetSocketTransform(Weapon->WeaponData->LeftHandGripSocketName);
	return FVector::Distance(Pawn->GetMesh()->GetSocketLocation(TEXT("hand_l")), Grip.GetLocation());
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAimAndGripTest, "LatencyCombat.Animation.AimAndSupportHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatAimAndGripTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatCharacter* Pawn = SpawnAnimatedPlayer(Fixture, *this);
	if (!Pawn)
	{
		return false;
	}
	USkeletalMeshComponent* Mesh = Pawn->GetMesh();
	UCombatAnimInstance* Anim = Cast<UCombatAnimInstance>(Mesh->GetAnimInstance());
	if (!TestNotNull(TEXT("Rifle AnimBP uses the native aim/IK parent"), Anim))
	{
		return false;
	}
	ACombatWeapon* Weapon = Pawn->GetWeapon();
	TestTrue(TEXT("Rifle exposes a support-hand IK socket"),
		Weapon->ThirdPersonMesh->DoesSocketExist(Weapon->WeaponData->LeftHandGripSocketName));
	TestTrue(TEXT("Support-hand IK blends in"), Anim->LeftHandIKAlpha > 0.99f);
	TestTrue(TEXT("Support hand reaches the rifle at rest"), GripError(Pawn) < 1.f);
	const FTransform Pelvis = Mesh->GetSocketTransform(TEXT("pelvis"), RTS_Component);
	const TArray<FTransform> RestPose = Mesh->GetBoneSpaceTransforms();
	const float RestBarrelPitch =
		Weapon->ThirdPersonMesh->GetSocketTransform(TEXT("Muzzle")).GetRotation().GetForwardVector().Rotation().Pitch;
	ACombatPlayerController* PC = CastChecked<ACombatPlayerController>(Pawn->GetController());
	for (const float Pitch : {60.f, -60.f, 0.f})
	{
		PC->SetControlRotation(FRotator(Pitch, 0.f, 0.f));
		Fixture.Advance(0.4f);
		AddInfo(FString::Printf(TEXT("CameraPitch=%.1f AimPitch=%.2f IKAlpha=%.3f GripErrorCm=%.3f"), Pitch,
			Anim->AimPitch, Anim->LeftHandIKAlpha, GripError(Pawn)));
		TestTrue(FString::Printf(TEXT("Aim pitch follows camera at %.0f degrees"), Pitch),
			FMath::IsNearlyEqual(Anim->AimPitch, Pitch, 1.f));
		TestTrue(
			FString::Printf(TEXT("Support hand stays on the rifle at %.0f degrees"), Pitch), GripError(Pawn) < 1.f);
		TestTrue(TEXT("Aim leaves pelvis position unchanged"),
			Mesh->GetSocketTransform(TEXT("pelvis"), RTS_Component).GetLocation().Equals(Pelvis.GetLocation(), 1.f));
		if (FMath::Abs(Pitch) > 1.f)
		{
			const TArray<FTransform> Pose = Mesh->GetBoneSpaceTransforms();
			for (const FName Bone : {FName(TEXT("spine_01")), FName(TEXT("neck_01"))})
			{
				const int32 Index = Mesh->GetBoneIndex(Bone);
				TestTrue(FString::Printf(TEXT("%s contributes to vertical aiming"), *Bone.ToString()),
					FMath::RadiansToDegrees(RestPose[Index].GetRotation().AngularDistance(Pose[Index].GetRotation())) >
						3.f);
			}
			const float BarrelPitch = Weapon->ThirdPersonMesh->GetSocketTransform(TEXT("Muzzle"))
										  .GetRotation()
										  .GetForwardVector()
										  .Rotation()
										  .Pitch;
			TestTrue(TEXT("Rifle barrel follows the vertical aim direction"),
				FMath::IsNearlyEqual(FMath::FindDeltaAngleDegrees(RestBarrelPitch, BarrelPitch), Pitch, 5.f));
		}
		Weapon->Fire->FireWeapon();
		Fixture.Advance(0.05f);
		TestTrue(TEXT("Firing montage continues to play with IK"), Anim->IsAnyMontagePlaying());
		TestTrue(TEXT("Support hand follows rifle recoil in the same evaluated pose"), GripError(Pawn) < 1.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatReplicatedAimTest, "LatencyCombat.Animation.ReplicatedViewPitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatReplicatedAimTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatCharacter* Pawn = SpawnAnimatedPlayer(Fixture, *this);
	UCombatAnimInstance* Anim = Pawn ? Cast<UCombatAnimInstance>(Pawn->GetMesh()->GetAnimInstance()) : nullptr;
	if (!TestNotNull(TEXT("Native combat animation instance exists"), Anim))
	{
		return false;
	}
	Pawn->GetController()->UnPossess();
	// Simulated proxies have no owning controller; use the engine's replicated RemoteViewPitch.
	Pawn->SetRemoteViewPitch(315.0);
	Fixture.Advance(0.4f);
	TestTrue(TEXT("Replicated wrapped-down pitch is normalized"), FMath::IsNearlyEqual(Anim->AimPitch, -45.f, 1.f));
	Pawn->SetRemoteViewPitch(60.0);
	Fixture.Advance(0.4f);
	TestTrue(TEXT("Replicated up pitch drives remote animation"), FMath::IsNearlyEqual(Anim->AimPitch, 60.f, 1.f));
	Pawn->SetRemoteViewPitch(89.0);
	Fixture.Advance(0.4f);
	TestTrue(TEXT("Extreme aim is clamped for the spine and neck"),
		FMath::IsNearlyEqual(Anim->AimPitch, Anim->MaxAimPitch, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatIKFallbackTest, "LatencyCombat.Animation.SupportHandFallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatIKFallbackTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatCharacter* Pawn = SpawnAnimatedPlayer(Fixture, *this);
	UCombatAnimInstance* Anim = Pawn ? Cast<UCombatAnimInstance>(Pawn->GetMesh()->GetAnimInstance()) : nullptr;
	if (!TestNotNull(TEXT("Native combat animation instance exists"), Anim))
	{
		return false;
	}
	ACombatWeapon* Weapon = Pawn->GetWeapon();
	Weapon->Fire->FireWeapon();
	Weapon->Ammo->StartReload();
	Fixture.Advance(0.3f);
	TestTrue(TEXT("Reload frees the support hand"), Anim->LeftHandIKAlpha < 0.01f);
	Weapon->Ammo->CancelReload();
	Fixture.Advance(0.4f);
	TestTrue(TEXT("Support hand returns after reload"), Anim->LeftHandIKAlpha > 0.99f && GripError(Pawn) < 1.f);

	UWeaponDataAsset* Data = DuplicateObject<UWeaponDataAsset>(Weapon->WeaponData, Weapon);
	Weapon->WeaponData = Data;
	Data->LeftHandGripSocketName = TEXT("MissingSupportGrip");
	Fixture.Advance(0.1f);
	TestEqual(TEXT("A missing support socket disables IK safely"), Anim->LeftHandIKAlpha, 0.f);
	Data->LeftHandGripSocketName = NAME_None;
	Fixture.Advance(0.1f);
	TestEqual(TEXT("Weapons can explicitly opt out of support-hand IK"), Anim->LeftHandIKAlpha, 0.f);
	TestFalse(TEXT("Missing support target leaves a finite pose"),
		Pawn->GetMesh()->GetSocketTransform(TEXT("hand_l")).ContainsNaN());
	return true;
}
#endif
