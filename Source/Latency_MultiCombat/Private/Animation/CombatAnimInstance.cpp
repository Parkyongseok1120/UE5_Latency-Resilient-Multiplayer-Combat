#include "Animation/CombatAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "CombatCharacter.h"
#include "CombatWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/UWeaponDataAsset.h"
#include "Engine/SkeletalMeshSocket.h"
#include "TwoBoneIK.h"

static bool ResolveLeftHandGrip(const ACombatCharacter* Character, FName RightHandBone, FTransform& OutGrip)
{
	const ACombatWeapon* Weapon = Character ? Character->GetWeapon() : nullptr;
	const UWeaponDataAsset* Data = IsValid(Weapon) ? Weapon->WeaponData.Get() : nullptr;
	if (!Data || Data->LeftHandGripSocketName.IsNone())
	{
		return false;
	}
	const USceneComponent* Visual = Weapon->ThirdPersonMesh->GetSkeletalMeshAsset()
		? static_cast<const USceneComponent*>(Weapon->ThirdPersonMesh.Get())
		: static_cast<const USceneComponent*>(Weapon->ThirdPersonStaticMesh.Get());
	const USkeletalMeshComponent* Body = Character->GetMesh();
	if (Visual->GetAttachParent() != Body || !Visual->DoesSocketExist(Data->LeftHandGripSocketName))
	{
		return false;
	}
	FTransform AttachmentInHand = FTransform::Identity;
	if (Visual->GetAttachSocketName() != RightHandBone)
	{
		const USkeletalMeshSocket* Socket = Body->GetSocketByName(Visual->GetAttachSocketName());
		if (!Socket || Socket->BoneName != RightHandBone)
		{
			return false;
		}
		AttachmentInHand = Socket->GetSocketLocalTransform();
	}
	// Only socket-local transforms are sampled here. Evaluation uses this frame's animated right hand,
	// so support-hand IK does not chase the previous frame's weapon world transform.
	OutGrip = Data->LeftHandGripOffset * Visual->GetSocketTransform(Data->LeftHandGripSocketName, RTS_Component) *
		Visual->GetRelativeTransform() * AttachmentInHand;
	return !OutGrip.ContainsNaN();
}

static FCompactPoseBoneIndex FindBone(const FBoneContainer& Bones, FName Name)
{
	const int32 Index = Bones.GetReferenceSkeleton().FindBoneIndex(Name);
	return Index == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE)
							   : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Index));
}

struct FCombatAnimInstanceProxy : public FAnimInstanceProxy
{
	explicit FCombatAnimInstanceProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* InInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InInstance, DeltaSeconds);
		UCombatAnimInstance* Instance = CastChecked<UCombatAnimInstance>(InInstance);
		const ACombatCharacter* Character = Cast<ACombatCharacter>(Instance->TryGetPawnOwner());
		const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
		const bool bAlive =
			Character && IsValid(Character->GetWeapon()) && Character->GetCombatState() != ECombatState::Dead;
		// The owner reads current input directly; simulated proxies use replicated RemoteViewPitch
		// through GetBaseAimRotation. Camera-manager caches can lag control input by one frame.
		const FRotator Aim = Character
			? (Character->GetController() ? Character->GetControlRotation() : Character->GetBaseAimRotation())
			: FRotator::ZeroRotator;
		float TargetPitch = bAlive && Instance->bEnableAimIK ? FRotator::NormalizeAxis(Aim.Pitch) : 0.f;
		TargetPitch = FMath::IsFinite(TargetPitch) ? TargetPitch : 0.f;
		const float Limit = FMath::Clamp(Instance->MaxAimPitch, 0.f, 89.f);
		SmoothedPitch = FMath::FInterpTo(SmoothedPitch, FMath::Clamp(TargetPitch, -Limit, Limit), DeltaSeconds,
			FMath::Max(0.f, Instance->AimInterpolationSpeed));
		AimBones = Instance->AimBones;
		RightHandBone = Instance->RightHandBone;
		LeftHandBone = Instance->LeftHandBone;
		AimCorrection = FQuat::Identity;
		if (Character && Mesh)
		{
			const FQuat CharacterToMesh = Mesh->GetComponentQuat().Inverse() * Character->GetActorQuat();
			const FQuat Reference = bAlive && Instance->bEnableAimIK && !Instance->ReferenceAimRotation.ContainsNaN()
				? Instance->ReferenceAimRotation.Quaternion() : FQuat::Identity;
			// Calibrate the authored neutral barrel before adding view pitch. Applying a camera pitch of
			// zero to the uncorrected rifle pose still points its muzzle up and to the character's right.
			AimCorrection = (CharacterToMesh * FRotator(SmoothedPitch, 0.f, 0.f).Quaternion() * Reference.Inverse() *
				CharacterToMesh.Inverse()).GetNormalized();
			ElbowHint = Mesh->GetComponentTransform().InverseTransformVectorNoScale(
				Character->GetActorTransform().TransformVectorNoScale(Instance->LeftElbowHintOffset));
		}
		const bool bHasGrip =
			bAlive && Instance->bEnableLeftHandIK && ResolveLeftHandGrip(Character, RightHandBone, GripInRightHand);
		const float TargetAlpha = bHasGrip && Character->GetCombatState() != ECombatState::Reloading ? 1.f : 0.f;
		IKAlpha = bHasGrip
			? FMath::FInterpTo(IKAlpha, TargetAlpha, DeltaSeconds, FMath::Max(0.f, Instance->IKInterpolationSpeed))
			: 0.f;
		Instance->AimPitch = SmoothedPitch;
		Instance->LeftHandIKAlpha = IKAlpha;
		Instance->LeftHandTargetInRightHandSpace = bHasGrip ? GripInRightHand : FTransform::Identity;
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		// Preserve the complete AnimBP graph, including its montage slot and foot IK.
		EvaluateAnimationNode(Output);
		if (AimCorrection.IsIdentity() && IKAlpha < ZERO_ANIMWEIGHT_THRESH)
		{
			return true;
		}
		FCSPose<FCompactPose> Pose;
		Pose.InitPose(Output.Pose);
		const FBoneContainer& Bones = Pose.GetPose().GetBoneContainer();
		TArray<TPair<FCompactPoseBoneIndex, float>, TInlineAllocator<12>> Rotations;
		for (const FCombatAimBone& Bone : AimBones)
		{
			const FCompactPoseBoneIndex Index = FindBone(Bones, Bone.Bone);
			if (Index.IsValid() && FMath::IsFinite(Bone.Weight))
			{
				Rotations.Emplace(Index, FMath::Clamp(Bone.Weight, 0.f, 1.f));
			}
		}
		Rotations.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
		for (const auto& Bone : Rotations)
		{
			FTransform Transform = Pose.GetComponentSpaceTransform(Bone.Key);
			Transform.SetRotation((FQuat::Slerp(FQuat::Identity, AimCorrection, Bone.Value) * Transform.GetRotation())
				.GetNormalized());
			const FBoneTransform Change(Bone.Key, Transform);
			Pose.LocalBlendCSBoneTransforms(MakeArrayView(&Change, 1), 1.f);
		}

		const FCompactPoseBoneIndex RightHand = FindBone(Bones, RightHandBone);
		const FCompactPoseBoneIndex Hand = FindBone(Bones, LeftHandBone);
		const FCompactPoseBoneIndex Elbow =
			Hand.IsValid() ? Bones.GetParentBoneIndex(Hand) : FCompactPoseBoneIndex(INDEX_NONE);
		const FCompactPoseBoneIndex Shoulder =
			Elbow.IsValid() ? Bones.GetParentBoneIndex(Elbow) : FCompactPoseBoneIndex(INDEX_NONE);
		if (IKAlpha > ZERO_ANIMWEIGHT_THRESH && RightHand.IsValid() && Shoulder.IsValid())
		{
			const FTransform Target = GripInRightHand * Pose.GetComponentSpaceTransform(RightHand);
			FTransform Upper = Pose.GetComponentSpaceTransform(Shoulder);
			FTransform Lower = Pose.GetComponentSpaceTransform(Elbow);
			FTransform End = Pose.GetComponentSpaceTransform(Hand);
			const FVector Pole = Upper.GetLocation() + AimCorrection.RotateVector(ElbowHint);
			AnimationCore::SolveTwoBoneIK(Upper, Lower, End, Pole, Target.GetLocation(), false, 1.0, 1.0);
			End.SetRotation(Target.GetRotation());
			const FBoneTransform Changes[] = {
				FBoneTransform(Shoulder, Upper), FBoneTransform(Elbow, Lower), FBoneTransform(Hand, End)};
			Pose.LocalBlendCSBoneTransforms(MakeArrayView(Changes), IKAlpha);
		}
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Pose, Output.Pose);
		return true;
	}

	TArray<FCombatAimBone> AimBones;
	FName RightHandBone;
	FName LeftHandBone;
	FQuat AimCorrection = FQuat::Identity;
	FVector ElbowHint = FVector::ZeroVector;
	FTransform GripInRightHand = FTransform::Identity;
	float SmoothedPitch = 0.f;
	float IKAlpha = 0.f;
};

UCombatAnimInstance::UCombatAnimInstance()
{
	AimBones = {{TEXT("spine_01"), 0.20f}, {TEXT("spine_02"), 0.20f}, {TEXT("spine_03"), 0.20f},
		{TEXT("spine_04"), 0.10f}, {TEXT("spine_05"), 0.10f}, {TEXT("neck_01"), 0.10f}, {TEXT("neck_02"), 0.05f},
		{TEXT("head"), 0.05f}, {TEXT("clavicle_l"), 0.20f}, {TEXT("clavicle_r"), 0.20f}};
}

FAnimInstanceProxy* UCombatAnimInstance::CreateAnimInstanceProxy()
{
	return new FCombatAnimInstanceProxy(this);
}

void UCombatAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
