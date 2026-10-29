#include "Animation/CombatFirstPersonAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "CombatCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraComponent.h"
#include "TwoBoneIK.h"

struct FCombatFirstPersonAnimProxy : public FAnimInstanceProxy
{
	explicit FCombatFirstPersonAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

	virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
		const ACombatCharacter* Character = Cast<ACombatCharacter>(Instance->TryGetPawnOwner());
		USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
		LocalPose = Body && Body->GetSkeletalMeshAsset() && GetSkelMeshComponent()->GetSkeletalMeshAsset() &&
				Body->GetSkeletalMeshAsset()->GetSkeleton() ==
					GetSkelMeshComponent()->GetSkeletalMeshAsset()->GetSkeleton()
			? Body->GetBoneSpaceTransforms()
			: TArray<FTransform>();
		if (Character)
		{
			const FTransform ViewInMesh = Character->FirstPersonCamera->GetComponentTransform().GetRelativeTransform(
				GetSkelMeshComponent()->GetComponentTransform());
			LeftElbowPole = ViewInMesh.TransformPosition(FVector(5.f, -40.f, -65.f));
			RightElbowPole = ViewInMesh.TransformPosition(FVector(5.f, 40.f, -65.f));
		}
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		Output.Pose.ResetToRefPose();
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		for (const FCompactPoseBoneIndex Bone : Output.Pose.ForEachBoneIndex())
		{
			const int32 MeshIndex = Bones.MakeMeshPoseIndex(Bone).GetInt();
			if (LocalPose.IsValidIndex(MeshIndex))
			{
				Output.Pose[Bone] = LocalPose[MeshIndex];
			}
		}
		// Keep the authored hand and finger targets, but bend the elbows below the view.
		// Forearm cut ends then continue beyond the viewport instead of floating in its center.
		FCSPose<FCompactPose> Pose;
		Pose.InitPose(Output.Pose);
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const int32 HandIndex =
				Bones.GetReferenceSkeleton().FindBoneIndex(Side == 0 ? TEXT("hand_l") : TEXT("hand_r"));
			const FCompactPoseBoneIndex Hand = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandIndex));
			if (!Hand.IsValid())
			{
				continue;
			}
			const FCompactPoseBoneIndex Elbow = Bones.GetParentBoneIndex(Hand);
			const FCompactPoseBoneIndex Shoulder = Bones.GetParentBoneIndex(Elbow);
			if (!Shoulder.IsValid())
			{
				continue;
			}
			FTransform Upper = Pose.GetComponentSpaceTransform(Shoulder);
			FTransform Lower = Pose.GetComponentSpaceTransform(Elbow);
			FTransform End = Pose.GetComponentSpaceTransform(Hand);
			const FQuat HandRotation = End.GetRotation();
			AnimationCore::SolveTwoBoneIK(
				Upper, Lower, End, Side == 0 ? LeftElbowPole : RightElbowPole, End.GetLocation(), false, 1.0, 1.0);
			End.SetRotation(HandRotation);
			const FBoneTransform Changes[] = {
				FBoneTransform(Shoulder, Upper), FBoneTransform(Elbow, Lower), FBoneTransform(Hand, End)};
			Pose.LocalBlendCSBoneTransforms(MakeArrayView(Changes), 1.f);
		}
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(Pose, Output.Pose);
		return true;
	}

	TArray<FTransform> LocalPose;
	FVector LeftElbowPole = FVector::ZeroVector;
	FVector RightElbowPole = FVector::ZeroVector;
};

FAnimInstanceProxy* UCombatFirstPersonAnimInstance::CreateAnimInstanceProxy()
{
	return new FCombatFirstPersonAnimProxy(this);
}

void UCombatFirstPersonAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
