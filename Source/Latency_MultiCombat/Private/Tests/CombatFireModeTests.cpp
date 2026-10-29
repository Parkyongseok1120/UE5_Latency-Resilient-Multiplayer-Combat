#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CombatTestWorld.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFireModeTest, "LatencyCombat.Weapon.SingleAutomaticToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatFireModeTest::RunTest(const FString& Parameters)
{
	FFPSBaselineTestWorld Fixture;
	ACombatPlayerController* PC = Fixture.SpawnPlayer(FVector(0, 0, 300), ETeamSide::Red);
	PC->SetAsLocalPlayerController();
	ACombatCharacter* Pawn = CastChecked<ACombatCharacter>(PC->GetPawn());
	ACombatWeapon* Weapon = Pawn->GetWeapon();
	UWeaponFireComponent* Fire = Weapon->Fire;
	TestTrue(TEXT("Default rifle starts in automatic mode"), Fire->IsAutomaticFire());
	TestEqual(TEXT("Magazine remains 30 rounds"), Weapon->Ammo->GetMagazineAmmo(), 30);
	TestEqual(TEXT("Starting magazine and reserve total 360"),
		Weapon->Ammo->GetMagazineAmmo() + Weapon->Ammo->GetReserveAmmo(), 360);
	Fire->SetTriggerHeld(true);
	Fixture.Advance(0.45f);
	const int32 BurstAmmo = Weapon->Ammo->GetMagazineAmmo();
	TestTrue(TEXT("Held automatic trigger fires several rounds"), BurstAmmo < 28);
	Fire->ToggleFireMode();
	TestFalse(TEXT("B switches to single fire"), Fire->IsAutomaticFire());
	Fixture.Advance(0.4f);
	TestEqual(TEXT("Changing modes stops the held burst"), Weapon->Ammo->GetMagazineAmmo(), BurstAmmo);
	Fire->SetTriggerHeld(true);
	Fixture.Advance(0.6f);
	TestEqual(TEXT("Holding single fire consumes one round"), Weapon->Ammo->GetMagazineAmmo(), BurstAmmo - 1);
	Fire->SetTriggerHeld(true);
	Fixture.Advance(0.3f);
	TestEqual(TEXT("Duplicate press cannot repeat a single shot"), Weapon->Ammo->GetMagazineAmmo(), BurstAmmo - 1);
	Fire->SetTriggerHeld(false);
	Fire->SetTriggerHeld(true);
	TestEqual(TEXT("A fresh click fires one more round"), Weapon->Ammo->GetMagazineAmmo(), BurstAmmo - 2);
	Fire->ToggleFireMode();
	TestTrue(TEXT("B switches back to automatic"), Fire->IsAutomaticFire());
	Fixture.Advance(0.3f);
	TestEqual(TEXT("Switching to auto needs a fresh press"), Weapon->Ammo->GetMagazineAmmo(), BurstAmmo - 2);
	Fire->SetTriggerHeld(true);
	Fixture.Advance(0.65f);
	Fire->SetTriggerHeld(false);
	const int32 ReleasedAmmo = Weapon->Ammo->GetMagazineAmmo();
	Fixture.Advance(0.4f);
	TestTrue(TEXT("Automatic fire resumes on the next press"), ReleasedAmmo < BurstAmmo - 4);
	TestEqual(TEXT("Releasing auto stops fire"), Weapon->Ammo->GetMagazineAmmo(), ReleasedAmmo);
	Weapon->Ammo->StartReload();
	Fire->SetTriggerHeld(true);
	Fixture.Advance(0.4f);
	TestEqual(TEXT("Reload blocks held automatic fire"), Weapon->Ammo->GetMagazineAmmo(), ReleasedAmmo);
	Fire->SetTriggerHeld(false);
	Fixture.Advance(2.f);
	TestEqual(TEXT("Reload conserves remaining ammunition"),
		Weapon->Ammo->GetMagazineAmmo() + Weapon->Ammo->GetReserveAmmo(), 330 + ReleasedAmmo);
	Fire->SetTriggerHeld(true);
	Pawn->TakeDamage(1000.f, FDamageEvent(), nullptr, nullptr);
	const int32 DeadAmmo = Weapon->Ammo->GetMagazineAmmo();
	Fire->ToggleFireMode();
	Fixture.Advance(0.4f);
	TestTrue(TEXT("Dead player cannot change mode"), Fire->IsAutomaticFire());
	TestEqual(TEXT("Death cancels held fire"), Weapon->Ammo->GetMagazineAmmo(), DeadAmmo);
	return true;
}
#endif
