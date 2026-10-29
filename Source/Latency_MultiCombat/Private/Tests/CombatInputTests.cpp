#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Data/CombatInputDataAsset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatDefaultInputTest, "LatencyCombat.Input.DefaultKeyboardMouse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatDefaultInputTest::RunTest(const FString& Parameters)
{
	UCombatInputDataAsset* Data = NewObject<UCombatInputDataAsset>();
	TestFalse(TEXT("An unconfigured asset is incomplete"), Data->IsComplete());
	Data->CreateDefaultMappings();
	TestTrue(TEXT("Built-in input is complete without content assets"), Data->IsComplete());
	struct FExpectedAxis
	{
		FKey Key;
		UInputAction* Action;
		FVector2D Value;
	};
	const FExpectedAxis Expected[] = {
		{EKeys::W, Data->Move, FVector2D(0.f, 1.f)},
		{EKeys::S, Data->Move, FVector2D(0.f, -1.f)},
		{EKeys::D, Data->Move, FVector2D(1.f, 0.f)},
		{EKeys::A, Data->Move, FVector2D(-1.f, 0.f)},
		{EKeys::MouseX, Data->Look, FVector2D(0.07f, 0.f)},
		{EKeys::MouseY, Data->Look, FVector2D(0.f, 0.07f)},
	};
	for (const FExpectedAxis& Axis : Expected)
	{
		bool bFound = false;
		for (const FEnhancedActionKeyMapping& Mapping : Data->MappingContext->GetMappings())
		{
			if (Mapping.Key != Axis.Key || Mapping.Action != Axis.Action)
			{
				continue;
			}
			bFound = true;
			for (const float Direction : {1.f, -1.f})
			{
				FInputActionValue Value(FVector2D(Direction, 0.f));
				for (const UInputModifier* Modifier : Mapping.Modifiers)
				{
					Value = Modifier->ModifyRaw(nullptr, Value, 0.016f);
				}
				for (const UInputModifier* Modifier : Axis.Action->Modifiers)
				{
					Value = Modifier->ModifyRaw(nullptr, Value, 0.016f);
				}
				TestTrue(*FString::Printf(TEXT("%s input %+.0f retains direction and sensitivity"),
					*Axis.Key.ToString(), Direction),
					Value.Get<FVector2D>().Equals(Axis.Value * Direction, 0.0001f));
			}
		}
		TestTrue(*FString::Printf(TEXT("%s has a mapping"), *Axis.Key.ToString()), bFound);
	}
	const FKey ButtonKeys[] = {EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::R, EKeys::RightMouseButton};
	const UInputAction* ButtonActions[] = {Data->Jump, Data->Fire, Data->Reload, Data->ADS};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ButtonKeys); ++Index)
	{
		bool bFound = false;
		for (const FEnhancedActionKeyMapping& Mapping : Data->MappingContext->GetMappings())
		{
			bFound |= Mapping.Key == ButtonKeys[Index] && Mapping.Action == ButtonActions[Index];
		}
		TestTrue(*FString::Printf(TEXT("%s maps to its combat action"), *ButtonKeys[Index].ToString()), bFound);
		TestEqual(
			TEXT("Button action uses Boolean values"), ButtonActions[Index]->ValueType, EInputActionValueType::Boolean);
	}
	return true;
}
#endif
