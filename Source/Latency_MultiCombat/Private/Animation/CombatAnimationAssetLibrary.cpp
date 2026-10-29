#include "Animation/CombatAnimationAssetLibrary.h"
#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SkeletalMesh.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"

static int32 CloseForearmEnds(FMeshDescription& Description, const FVector3f& LeftRoot, const FVector3f& RightRoot)
{
	FSkeletalMeshAttributes Attributes(Description);
	TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
	FSkinWeightsVertexAttributesRef Weights = Attributes.GetVertexSkinWeights();
	TSet<FEdgeID> Boundaries;
	for (const FEdgeID Edge : Description.Edges().GetElementIDs())
	{
		if (Description.GetEdgeConnectedTriangleIDs(Edge).Num() == 1)
		{
			Boundaries.Add(Edge);
		}
	}
	int32 CapTriangles = 0;
	while (!Boundaries.IsEmpty())
	{
		const FEdgeID FirstEdge = *Boundaries.CreateConstIterator();
		TArray<FVertexID> Queue;
		TSet<FVertexID> Vertices;
		TArray<FEdgeID> Edges;
		Queue.Add(Description.GetEdgeVertex(FirstEdge, 0));
		for (int32 Index = 0; Index < Queue.Num(); ++Index)
		{
			const FVertexID Vertex = Queue[Index];
			if (Vertices.Contains(Vertex))
			{
				continue;
			}
			Vertices.Add(Vertex);
			for (const FEdgeID Edge : Description.GetVertexConnectedEdgeIDs(Vertex))
			{
				if (Boundaries.Remove(Edge) > 0)
				{
					Edges.Add(Edge);
					Queue.Add(Description.GetEdgeVertex(Edge, 0));
					Queue.Add(Description.GetEdgeVertex(Edge, 1));
				}
			}
		}
		FVector3f Center = FVector3f::ZeroVector;
		bool bNearForearmRoot = false;
		for (const FVertexID Vertex : Vertices)
		{
			Center += Positions[Vertex];
			bNearForearmRoot |= FMath::Min(FVector3f::DistSquared(Positions[Vertex], LeftRoot),
									FVector3f::DistSquared(Positions[Vertex], RightRoot)) < 144.f;
		}
		if (!bNearForearmRoot || Vertices.Num() < 3)
		{
			continue;
		}
		Center /= Vertices.Num();
		TArray<UE::AnimationCore::FBoneWeight> CenterWeights;
		for (const UE::AnimationCore::FBoneWeight Weight : Weights.Get(Queue[0]))
		{
			CenterWeights.Add(Weight);
		}
		const FVertexID CenterVertex = Description.CreateVertex();
		Positions[CenterVertex] = Center;
		Weights.Set(CenterVertex, MakeArrayView(CenterWeights));
		for (const FEdgeID Edge : Edges)
		{
			const FTriangleID Neighbor = Description.GetEdgeConnectedTriangleIDs(Edge)[0];
			const TArrayView<const FVertexID> NeighborVertices = Description.GetTriangleVertices(Neighbor);
			FVertexID A = Description.GetEdgeVertex(Edge, 0);
			FVertexID B = Description.GetEdgeVertex(Edge, 1);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (NeighborVertices[Index] == B && NeighborVertices[(Index + 1) % 3] == A)
				{
					Swap(A, B);
					break;
				}
			}
			// Reverse the existing face's boundary winding so this closes the outside surface.
			const FVertexID CapVertices[] = {B, A, CenterVertex};
			const FVector3f Tangent = (Positions[A] - Positions[B]).GetSafeNormal();
			const FVector3f Normal =
				FVector3f::CrossProduct(Positions[A] - Positions[B], Center - Positions[B]).GetSafeNormal();
			TArray<FVertexInstanceID> Instances;
			for (const FVertexID Vertex : CapVertices)
			{
				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
				Attributes.GetVertexInstanceNormals()[Instance] = Normal;
				Attributes.GetVertexInstanceTangents()[Instance] = Tangent;
				Attributes.GetVertexInstanceBinormalSigns()[Instance] = 1.f;
				Attributes.GetVertexInstanceColors()[Instance] = FVector4f(1.f, 1.f, 1.f, 1.f);
				for (int32 Channel = 0; Channel < Attributes.GetVertexInstanceUVs().GetNumChannels(); ++Channel)
				{
					Attributes.GetVertexInstanceUVs().Set(Instance, Channel, FVector2f(0.5f, 0.5f));
				}
				Instances.Add(Instance);
			}
			Description.CreateTriangle(Description.GetTrianglePolygonGroup(Neighbor), Instances);
			++CapTriangles;
		}
	}
	return CapTriangles;
}
#endif

bool UCombatAnimationAssetLibrary::KeepFirstPersonArmGeometry(USkeletalMesh* Mesh)
{
#if WITH_EDITOR
	if (!Mesh)
	{
		return false;
	}
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	const int32 LeftArm = Skeleton.FindBoneIndex(TEXT("lowerarm_l"));
	const int32 RightArm = Skeleton.FindBoneIndex(TEXT("lowerarm_r"));
	if (LeftArm == INDEX_NONE || RightArm == INDEX_NONE)
	{
		return false;
	}
	TSet<int32> ArmBones;
	TArray<FTransform> ReferencePose;
	for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
	{
		const int32 Parent = Skeleton.GetParentIndex(Bone);
		ReferencePose.Add(Parent == INDEX_NONE ? Skeleton.GetRefBonePose()[Bone]
											   : Skeleton.GetRefBonePose()[Bone] * ReferencePose[Parent]);
		if (Bone == LeftArm || Bone == RightArm || Skeleton.BoneIsChildOf(Bone, LeftArm) ||
			Skeleton.BoneIsChildOf(Bone, RightArm))
		{
			ArmBones.Add(Bone);
		}
	}
	Mesh->Modify();
	Mesh->PreEditChange(nullptr);
	for (int32 LOD = 0; LOD < Mesh->GetLODNum(); ++LOD)
	{
		FMeshDescription* Description = Mesh->GetMeshDescription(LOD);
		if (!Description)
		{
			return false;
		}
		FSkeletalMeshAttributes Attributes(*Description);
		FSkinWeightsVertexAttributesRef Weights = Attributes.GetVertexSkinWeights();
		TSet<FVertexID> ArmVertices;
		for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
		{
			float ArmWeight = 0.f;
			for (const UE::AnimationCore::FBoneWeight Weight : Weights.Get(Vertex))
			{
				if (ArmBones.Contains(Weight.GetBoneIndex()))
				{
					ArmWeight += Weight.GetWeight();
				}
			}
			if (ArmWeight > 0.5f)
			{
				ArmVertices.Add(Vertex);
			}
		}
		TArray<FTriangleID> Remove;
		for (const FTriangleID Triangle : Description->Triangles().GetElementIDs())
		{
			for (const FVertexID Vertex : Description->GetTriangleVertices(Triangle))
			{
				if (!ArmVertices.Contains(Vertex))
				{
					Remove.Add(Triangle);
					break;
				}
			}
		}
		Description->DeleteTriangles(Remove);
		if (Description->Triangles().Num() == 0)
		{
			return false;
		}
		const int32 CapTriangles = CloseForearmEnds(*Description, FVector3f(ReferencePose[LeftArm].GetLocation()),
			FVector3f(ReferencePose[RightArm].GetLocation()));
		FElementIDRemappings Remappings;
		Description->Compact(Remappings);
		if (!Mesh->CommitMeshDescription(LOD))
		{
			return false;
		}
		UE_LOG(LogTemp, Display, TEXT("[FirstPersonArms] LOD%d ArmTriangles=%d Removed=%d CapTriangles=%d"), LOD,
			Description->Triangles().Num(), Remove.Num(), CapTriangles);
	}
	Mesh->Build();
	Mesh->PostEditChange();
	return true;
#else
	return false;
#endif
}

bool UCombatAnimationAssetLibrary::ConfigureRifleUpperBody(UAnimBlueprint* Blueprint, UAnimSequence* AimPose)
{
#if WITH_EDITOR
	if (!Blueprint || !AimPose)
	{
		return false;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node->NodeComment == TEXT("CombatUpperBodyLayer"))
			{
				return true;
			}
		}
	}
	UAnimGraphNode_Slot* Slot = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph->GetFName() != TEXT("AnimGraph"))
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UAnimGraphNode_Slot* Candidate = Cast<UAnimGraphNode_Slot>(Node);
				Candidate && Candidate->Node.SlotName == TEXT("DefaultSlot"))
			{
				Slot = Candidate;
				break;
			}
		}
	}
	if (!Slot)
	{
		return false;
	}
	UEdGraph* Graph = Slot->GetGraph();
	UEdGraphPin* Source = Slot->FindPin(TEXT("Source"));
	UEdGraphPin* Output = Slot->FindPin(TEXT("Pose"));
	if (!Source || Source->LinkedTo.Num() != 1 || !Output || Output->LinkedTo.IsEmpty())
	{
		return false;
	}
	UEdGraphPin* Locomotion = Source->LinkedTo[0];
	const TArray<UEdGraphPin*> Consumers = Output->LinkedTo;
	Blueprint->Modify();
	Graph->Modify();
	Slot->Modify();
	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> AimCreator(*Graph);
	UAnimGraphNode_SequencePlayer* Aim = AimCreator.CreateNode();
	Aim->Node.SetSequence(AimPose);
	Aim->NodePosX = Slot->NodePosX - 280;
	Aim->NodePosY = Slot->NodePosY + 220;
	Aim->NodeComment = TEXT("CombatUpperBodyAim");
	AimCreator.Finalize();
	FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> BlendCreator(*Graph);
	UAnimGraphNode_LayeredBoneBlend* Blend = BlendCreator.CreateNode();
	if (Blend->Node.BlendPoses.IsEmpty())
	{
		Blend->Node.AddPose();
	}
	Blend->Node.bMeshSpaceRotationBlend = true;
	FBranchFilter Filter;
	Filter.BoneName = TEXT("spine_01");
	Filter.BlendDepth = 0;
	Blend->Node.LayerSetup[0].BranchFilters.Add(Filter);
	Blend->NodePosX = Slot->NodePosX + 260;
	Blend->NodePosY = Slot->NodePosY;
	Blend->NodeComment = TEXT("CombatUpperBodyLayer");
	BlendCreator.Finalize();
	Source->BreakAllPinLinks();
	Output->BreakAllPinLinks();
	const UEdGraphSchema* Schema = Graph->GetSchema();
	bool bConnected = Schema->TryCreateConnection(Locomotion, Blend->FindPin(TEXT("BasePose"))) &&
		Schema->TryCreateConnection(Aim->FindPin(TEXT("Pose")), Source) &&
		Schema->TryCreateConnection(Output, Blend->FindPin(TEXT("BlendPoses_0")));
	for (UEdGraphPin* Consumer : Consumers)
	{
		bConnected &= Schema->TryCreateConnection(Blend->FindPin(TEXT("Pose")), Consumer);
	}
	if (!bConnected)
	{
		Source->BreakAllPinLinks();
		Output->BreakAllPinLinks();
		Graph->RemoveNode(Blend);
		Graph->RemoveNode(Aim);
		Schema->TryCreateConnection(Locomotion, Source);
		for (UEdGraphPin* Consumer : Consumers)
		{
			Schema->TryCreateConnection(Output, Consumer);
		}
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Blueprint->Status != BS_Error;
#else
	return false;
#endif
}
