#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CombatTestWorld.h"
#include "Animation/CombatAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WeaponPresentationComponent.h"
#include "Components/CharacterPresentationComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

static ACombatCharacter* SpawnAnimatedPlayer(FFPSBaselineTestWorld& Fixture, FAutomationTestBase& Test)
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

static float GripError(const ACombatCharacter* Pawn)
{
	// World::Tick alone need not finalize a parallel pose before these test-side socket reads.
	Pawn->GetMesh()->GetBoneSpaceTransforms();
	const ACombatWeapon* Weapon = Pawn->GetWeapon();
	const FTransform Grip = Weapon->WeaponData->LeftHandGripOffset *
		Weapon->ThirdPersonMesh->GetSocketTransform(Weapon->WeaponData->LeftHandGripSocketName);
	return FVector::Distance(Pawn->GetMesh()->GetSocketLocation(TEXT("hand_l")), Grip.GetLocation());
}

static float BarrelAimError(const ACombatCharacter* Pawn, const FRotator& View)
{
	Pawn->GetMesh()->GetBoneSpaceTransforms();
	const FVector Barrel =
		Pawn->GetWeapon()->ThirdPersonMesh->GetSocketTransform(TEXT("Muzzle")).GetRotation().GetForwardVector();
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Barrel, View.Vector()), -1.0, 1.0)));
}

static void WalkAnimatedPlayer(FFPSBaselineTestWorld& Fixture, ACombatCharacter* Pawn, float Seconds)
{
	for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += 0.05f)
	{
		Pawn->AddMovementInput(FVector::ForwardVector);
		Fixture.Advance(0.05f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatMovingAimTest, "LatencyCombat.Animation.MovingUpperBodyAndReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatMovingAimTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatCharacter* Pawn = SpawnAnimatedPlayer(Fixture, *this);
	if (!Pawn)
	{
		return false;
	}
	USkeletalMeshComponent* Mesh = Pawn->GetMesh();
	UAnimInstance* Anim = Mesh->GetAnimInstance();
	ACombatPlayerController* PC = CastChecked<ACombatPlayerController>(Pawn->GetController());
	UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
	AActor* Floor = Fixture.World->SpawnActor<AActor>();
	UBoxComponent* FloorCollision = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(FloorCollision);
	FloorCollision->SetBoxExtent(FVector(10000.f, 10000.f, 50.f));
	FloorCollision->SetCollisionProfileName(TEXT("BlockAll"));
	FloorCollision->RegisterComponent();
	Floor->SetActorLocation(FVector(0.f, 0.f, -50.f));
	Pawn->SetActorLocation(FVector(0.f, 0.f, 100.f));
	Movement->SetMovementMode(MOVE_Walking);
	for (const float Speed : {150.f, 450.f, 600.f})
	{
		for (const float Pitch : {0.f, 40.f, -40.f})
		{
			const FRotator View(Pitch, 0.f, 0.f);
			PC->SetControlRotation(View);
			Movement->MaxWalkSpeed = Speed;
			WalkAnimatedPlayer(Fixture, Pawn, 0.5f);
			TestTrue(TEXT("Movement input drives real ground locomotion"), Pawn->GetVelocity().Size2D() > Speed * 0.8f);
			const float Error = BarrelAimError(Pawn, View);
			AddInfo(FString::Printf(TEXT("Moving speed=%.0f pitch=%.0f barrel error=%.2f"), Speed, Pitch, Error));
			TestTrue(TEXT("Walking/running upper body keeps the rifle aimed"), Error < 3.f);
			TestTrue(TEXT("Moving support hand stays on rifle"), GripError(Pawn) < 1.f);
		}
	}
	const FQuat BeforeLeg = Mesh->GetSocketQuaternion(TEXT("calf_l"));
	Movement->MaxWalkSpeed = 450.f;
	Pawn->GetWeapon()->Fire->FireWeapon();
	WalkAnimatedPlayer(Fixture, Pawn, 0.1f);
	Mesh->GetBoneSpaceTransforms();
	TestTrue(TEXT("Upper-body fire montage evaluates while moving"), Anim->IsAnyMontagePlaying());
	TestTrue(TEXT("Lower body continues locomotion during fire"),
		BeforeLeg.AngularDistance(Mesh->GetSocketQuaternion(TEXT("calf_l"))) > 0.01f);
	Pawn->GetWeapon()->Ammo->StartReload();
	WalkAnimatedPlayer(Fixture, Pawn, 0.4f);
	TestTrue(TEXT("Reload state starts its presentation"), Pawn->GetWeapon()->Presentation->IsReloadAnimationPlaying());
	TestTrue(TEXT("Reload montage is active"), Anim->IsAnyMontagePlaying());
	TestTrue(TEXT("First-person weapon cants during reload without losing the grip"),
		Pawn->GetWeapon()->FirstPersonMesh->GetRelativeRotation().Quaternion().AngularDistance(
			Pawn->GetWeapon()->WeaponData->FirstPersonRotationOffset.Quaternion()) > 0.1f);
	Fixture.Advance(2.f);
	TestFalse(
		TEXT("Reload presentation stops with ammo state"), Pawn->GetWeapon()->Presentation->IsReloadAnimationPlaying());
	TestTrue(TEXT("Reload restores full magazine"),
		Pawn->GetWeapon()->Ammo->GetMagazineAmmo() == Pawn->GetWeapon()->GetStats().MagazineSize);
	Pawn->GetWeapon()->Fire->FireWeapon();
	Pawn->GetWeapon()->Ammo->StartReload();
	Fixture.Advance(0.3f);
	Pawn->GetWeapon()->Ammo->CancelReload();
	Fixture.Advance(0.3f);
	TestFalse(TEXT("Cancelled reload releases its presentation"),
		Pawn->GetWeapon()->Presentation->IsReloadAnimationPlaying());
	TestTrue(TEXT("Cancelled reload restores the first-person transform"),
		Pawn->GetWeapon()->FirstPersonMesh->GetRelativeLocation().Equals(
			Pawn->GetWeapon()->WeaponData->FirstPersonLocationOffset, 0.1f));
	Pawn->GetWeapon()->Ammo->StartReload();
	Fixture.Advance(0.2f);
	TestTrue(
		TEXT("Reload can restart after cancellation"), Pawn->GetWeapon()->Presentation->IsReloadAnimationPlaying());
	Pawn->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	Fixture.Advance(0.1f);
	TestFalse(TEXT("Death stops the reload presentation"), Pawn->GetWeapon()->Presentation->IsReloadAnimationPlaying());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFirstPersonHandsTest, "LatencyCombat.Animation.FirstPersonHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatFirstPersonHandsTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatCharacter* Pawn = SpawnAnimatedPlayer(Fixture, *this);
	if (!Pawn)
	{
		return false;
	}
	ACombatWeapon* Weapon = Pawn->GetWeapon();
	USkeletalMeshComponent* Arms = Pawn->FirstPersonArms;
	TestEqual(
		TEXT("Eye line is raised above the original camera"), Pawn->FirstPersonCamera->GetRelativeLocation().Z, 76.0);
	TestNotNull(TEXT("First-person hands have a skeletal mesh"), Arms->GetSkeletalMeshAsset());
	TestTrue(TEXT("Only the owner can see view arms"), Arms->bOnlyOwnerSee && !Arms->bOwnerNoSee);
	TestTrue(
		TEXT("View arms cannot collide or block shots"), Arms->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestTrue(TEXT("View arms use first-person rendering"),
		Arms->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson);
	TestTrue(TEXT("View arms use a separate mesh without changing the world body"),
		Arms->GetSkeletalMeshAsset() != Pawn->GetMesh()->GetSkeletalMeshAsset());
	for (const float Pitch : {0.f, 60.f, -60.f})
	{
		CastChecked<ACombatPlayerController>(Pawn->GetController())->SetControlRotation(FRotator(Pitch, 0.f, 0.f));
		Fixture.Advance(0.4f);
		Arms->GetBoneSpaceTransforms();
		Pawn->Presentation->UpdateFirstPersonArms();
		const FTransform ExpectedWeapon = Weapon->ThirdPersonMesh->GetRelativeTransform() *
			Arms->GetSocketTransform(Weapon->ThirdPersonMesh->GetAttachSocketName());
		const float RightError =
			FVector::Dist(ExpectedWeapon.GetLocation(), Weapon->FirstPersonMesh->GetComponentLocation());
		const FTransform LeftTarget = Weapon->WeaponData->LeftHandGripOffset *
			Weapon->FirstPersonMesh->GetSocketTransform(Weapon->WeaponData->LeftHandGripSocketName);
		const float LeftError = FVector::Dist(Arms->GetSocketLocation(TEXT("hand_l")), LeftTarget.GetLocation());
		AddInfo(FString::Printf(
			TEXT("First-person pitch=%.0f right grip=%.4fcm left grip=%.4fcm"), Pitch, RightError, LeftError));
		TestTrue(TEXT("Right hand holds first-person rifle"), RightError < 0.2f);
		TestTrue(TEXT("Support hand holds first-person rifle"), LeftError < 1.f);
	}
	Weapon->Fire->FireWeapon();
	Fixture.Advance(0.1f);
	TestTrue(TEXT("Hands stay visible during recoil"), Arms->IsVisible());
	Weapon->Ammo->StartReload();
	Fixture.Advance(0.5f);
	TestTrue(TEXT("Hands stay visible during reload"), Arms->IsVisible());
	Arms->GetBoneSpaceTransforms();
	const FTransform LeftReloadTarget = Weapon->WeaponData->LeftHandGripOffset *
		Weapon->FirstPersonMesh->GetSocketTransform(Weapon->WeaponData->LeftHandGripSocketName);
	TestTrue(TEXT("Reload releases the left hand from the foregrip"),
		FVector::Dist(Arms->GetSocketLocation(TEXT("hand_l")), LeftReloadTarget.GetLocation()) > 2.f);
	Fixture.Advance(2.f);
	TestTrue(TEXT("Hands remain visible after reload"), Arms->IsVisible());
	Pawn->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	Fixture.Advance(0.1f);
	TestFalse(TEXT("Death hides the view arms"), Arms->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatViewDirectionTest, "LatencyCombat.Animation.ViewDirectionAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatViewDirectionTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatCharacter* Pawn = SpawnAnimatedPlayer(Fixture, *this);
	if (!Pawn || !TestNotNull(TEXT("Animated player has a rifle"), Pawn->GetWeapon()))
	{
		return false;
	}
	ACombatPlayerController* PC = CastChecked<ACombatPlayerController>(Pawn->GetController());
	for (const float Yaw : {0.f, 90.f, -135.f})
	{
		for (const float Pitch : {0.f, 60.f, -60.f})
		{
			const FRotator View(Pitch, Yaw, 0.f);
			PC->SetControlRotation(View);
			Pawn->FaceRotation(View, 0.f);
			Fixture.Advance(0.6f);
			const float Error = BarrelAimError(Pawn, View);
			AddInfo(FString::Printf(TEXT("ViewPitch=%.0f ViewYaw=%.0f BarrelErrorDeg=%.3f"), Pitch, Yaw, Error));
			TestTrue(TEXT("Rifle points along the view direction, including neutral aim"), Error < 2.f);
			TestTrue(TEXT("Support hand stays on the aligned rifle"), GripError(Pawn) < 1.f);
		}
	}
	return true;
}

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
	Pawn->SetActorRotation(FRotator(0.f, 90.f, 0.f));
	for (const float Pitch : {0.f, -60.f, 60.f})
	{
		Pawn->SetRemoteViewPitch(Pitch);
		Fixture.Advance(0.6f);
		const float Error = BarrelAimError(Pawn, FRotator(Pitch, 90.f, 0.f));
		AddInfo(FString::Printf(TEXT("RemotePitch=%.0f BarrelErrorDeg=%.3f"), Pitch, Error));
		TestTrue(TEXT("Controllerless replicated aim aligns the final rifle direction"), Error < 2.f);
	}
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
