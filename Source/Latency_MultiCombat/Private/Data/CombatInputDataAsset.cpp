#include "Data/CombatInputDataAsset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

static UInputAction* CreateAction(UObject* Owner, const TCHAR* Name, EInputActionValueType Type)
{
	UInputAction* Action = NewObject<UInputAction>(Owner, Name);
	Action->ValueType = Type;
	return Action;
}

static void MapAxis(UInputMappingContext* Context, UInputAction* Action, FKey Key, bool bY, bool bNegate)
{
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
	if (bNegate)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}
	if (bY)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
}

bool UCombatInputDataAsset::IsComplete() const
{
	return MappingContext && Move && Look && Jump && Fire && Reload && ADS;
}

void UCombatInputDataAsset::CreateDefaultMappings()
{
	MappingContext = NewObject<UInputMappingContext>(this);
	Move = CreateAction(this, TEXT("Move"), EInputActionValueType::Axis2D);
	Look = CreateAction(this, TEXT("Look"), EInputActionValueType::Axis2D);
	Jump = CreateAction(this, TEXT("Jump"), EInputActionValueType::Boolean);
	Fire = CreateAction(this, TEXT("Fire"), EInputActionValueType::Boolean);
	Reload = CreateAction(this, TEXT("Reload"), EInputActionValueType::Boolean);
	ADS = CreateAction(this, TEXT("ADS"), EInputActionValueType::Boolean);
	// Preserve the project's previous mouse-axis sensitivity using an Enhanced Input modifier.
	UInputModifierScalar* LookScale = NewObject<UInputModifierScalar>(this);
	LookScale->Scalar = FVector(0.07f, 0.07f, 1.f);
	Look->Modifiers.Add(LookScale);
	MapAxis(MappingContext, Move, EKeys::W, true, false);
	MapAxis(MappingContext, Move, EKeys::S, true, true);
	MapAxis(MappingContext, Move, EKeys::D, false, false);
	MapAxis(MappingContext, Move, EKeys::A, false, true);
	MapAxis(MappingContext, Look, EKeys::MouseX, false, false);
	MapAxis(MappingContext, Look, EKeys::MouseY, true, true);
	MappingContext->MapKey(Jump, EKeys::SpaceBar);
	MappingContext->MapKey(Fire, EKeys::LeftMouseButton);
	MappingContext->MapKey(Reload, EKeys::R);
	MappingContext->MapKey(ADS, EKeys::RightMouseButton);
}
