#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CombatTestWorld.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/CharacterPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatLocalFeedbackTest, "LatencyCombat.Feedback.LocalCameraAndVitals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatLocalFeedbackTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	PC->SetAsLocalPlayerController();
	ACombatPlayerController* Other = Fixture.SpawnPlayer(FVector(1000, 0, 300), ETeamSide::Blue);
	ACombatCharacter* Pawn = CastChecked<ACombatCharacter>(PC->GetPawn());
	UCombatFeedbackComponent* Feedback = PC->Feedback;
	Feedback->SetComponentTickEnabled(false);
	Feedback->UpdateFeedback(0.05f);
	TestTrue(TEXT("Damage material is bundled"), Feedback->HasDamageMaterial());
	TestEqual(TEXT("Initial vitals never cause a damage flash"), Feedback->GetHitFlash(), 0.f);
	TestEqual(TEXT("Full health has no glitch"), Feedback->GetLowHealthIntensity(), 0.f);
	const int32 BlendCount = Pawn->FirstPersonCamera->PostProcessSettings.WeightedBlendables.Array.Num();
	const FRotator Aim(0.f, 90.f, 0.f);
	PC->SetControlRotation(Aim);
	Pawn->GetWeapon()->Fire->FireWeapon();
	TestTrue(TEXT("Local shot starts a camera shake immediately"), Feedback->HasActiveFireShake());
	TestTrue(TEXT("Camera shake never changes authoritative aim input"), PC->GetControlRotation().Equals(Aim));
	Fixture.Advance(0.3f);
	// This isolated world fixture does not run the engine's viewport camera update.
	PC->PlayerCameraManager->bUseClientSideCameraUpdates = false;
	PC->PlayerCameraManager->UpdateCamera(0.3f);
	Feedback->UpdateFeedback(0.3f);
	TestFalse(TEXT("Shot shake ends instead of accumulating indefinitely"), Feedback->HasActiveFireShake());
	Other->GetPlayerState<ACombatPlayerState>()->Health->ApplyDamage(30.f);
	Feedback->UpdateFeedback(0.05f);
	TestEqual(TEXT("Another player's damage cannot flash our screen"), Feedback->GetHitFlash(), 0.f);
	UHealthComponent* Health = PC->GetPlayerState<ACombatPlayerState>()->Health;
	Health->ApplyDamage(30.f);
	Feedback->UpdateFeedback(0.05f);
	TestEqual(TEXT("Replicated vitals decrease causes a full damage pulse"), Feedback->GetHitFlash(), 1.f);
	Feedback->UpdateFeedback(0.4f);
	TestEqual(TEXT("Damage pulse fades without requiring another health change"), Feedback->GetHitFlash(), 0.f);
	const float MildGlitch = Feedback->GetLowHealthIntensity();
	TestEqual(TEXT("Glitch is absent above half health"), MildGlitch, 0.f);
	Health->ApplyDamage(80.f);
	Feedback->UpdateFeedback(0.4f);
	TestTrue(TEXT("Lower health makes the glitch stronger"), Feedback->GetLowHealthIntensity() > MildGlitch + 0.1f);
	Health->Heal(100.f);
	Feedback->UpdateFeedback(0.5f);
	TestEqual(TEXT("Healing removes low-health distortion"), Feedback->GetLowHealthIntensity(), 0.f);
	TestEqual(TEXT("Healing does not retrigger damage flash"), Feedback->GetHitFlash(), 0.f);
	TestEqual(TEXT("Only one effect blendable is attached"),
		Pawn->FirstPersonCamera->PostProcessSettings.WeightedBlendables.Array.Num(), BlendCount);
	// Armor is empty after the earlier damage, allowing exact health threshold checks.
	Health->ApplyDamage(49.f);
	Feedback->UpdateFeedback(0.5f);
	TestEqual(TEXT("51 HP has no glitch"), Feedback->GetLowHealthIntensity(), 0.f);
	Health->ApplyDamage(1.f);
	Feedback->UpdateFeedback(0.5f);
	const float AtThreshold = Feedback->GetLowHealthIntensity();
	TestTrue(TEXT("50 HP starts a visible glitch"), AtThreshold >= 0.15f);
	Health->ApplyDamage(1.f);
	Feedback->UpdateFeedback(0.5f);
	TestTrue(TEXT("49 HP is stronger than 50 HP"), Feedback->GetLowHealthIntensity() > AtThreshold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatRagdollRespawnTest, "LatencyCombat.Feedback.RagdollAndRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatRagdollRespawnTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	Fixture.World->GetAuthGameMode<ACombatGameMode>()->RespawnDelay = 0.2f;
	UCharacterDataAsset* Data =
		LoadObject<UCharacterDataAsset>(nullptr, TEXT("/Game/Data/DA_DefaultCharacter.DA_DefaultCharacter"));
	Data = DuplicateObject<UCharacterDataAsset>(Data, Fixture.World);
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red, FWeaponStats(), Data);
	PC->SetAsLocalPlayerController();
	ACombatCharacter* Pawn = CastChecked<ACombatCharacter>(PC->GetPawn());
	UCombatFeedbackComponent* Feedback = PC->Feedback;
	Feedback->SetComponentTickEnabled(false);
	Feedback->UpdateFeedback(0.05f);
	TestNotNull(TEXT("Default mannequin has a physics asset"), Pawn->GetMesh()->GetPhysicsAsset());
	Pawn->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	Feedback->UpdateFeedback(0.05f);
	TestTrue(TEXT("Actual death starts visual ragdoll"), Pawn->Presentation->IsRagdollActive());
	TestTrue(TEXT("Ragdoll simulates pelvis physics"), Pawn->GetMesh()->IsSimulatingPhysics(TEXT("pelvis")));
	TestTrue(TEXT("Corpse cannot block hitscan traces"),
		Pawn->GetMesh()->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Ignore);
	TestTrue(TEXT("Corpse cannot push player capsules"),
		Pawn->GetMesh()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);
	Pawn->Presentation->StartDeathRagdoll();
	TestTrue(TEXT("Repeated death presentation is idempotent"), Pawn->Presentation->IsRagdollActive());
	TestFalse(TEXT("Death stops the firing shake"), Feedback->HasActiveFireShake());
	Fixture.Advance(0.4f);
	ACombatCharacter* Respawned = Cast<ACombatCharacter>(PC->GetPawn());
	if (!TestNotNull(TEXT("Respawn creates a fresh pawn"), Respawned))
	{
		return false;
	}
	TestFalse(TEXT("New life does not simulate corpse physics"), Respawned->Presentation->IsRagdollActive());
	Feedback->UpdateFeedback(0.05f);
	TestEqual(TEXT("New life clears the old damage pulse"), Feedback->GetHitFlash(), 0.f);
	TestEqual(TEXT("New life clears old glitch strength"), Feedback->GetLowHealthIntensity(), 0.f);
	return true;
}
#endif
