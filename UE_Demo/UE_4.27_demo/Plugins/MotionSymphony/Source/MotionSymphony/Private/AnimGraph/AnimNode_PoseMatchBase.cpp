// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_PoseMatchBase.h"
#include "AnimNode_MotionRecorder.h"
#include "DrawDebugHelpers.h"
#include "MMPreProcessUtils.h"
#include "MotionMatchingUtil/MotionMatchingUtils.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyNodes"

static TAutoConsoleVariable<int32> CVarPoseMatchingDebug(
	TEXT("a.AnimNode.MoSymph.PoseMatch.Debug"),
	0,
	TEXT("Turns Pose Matching Debugging On / Off.\n")
	TEXT("<=0: Off \n")
	TEXT("  1: On - Show chosen pose\n")
	TEXT("  2: On - Show All Poses\n")
	TEXT("  3: On - Show All Poses With Velocity\n"));


FPoseMatchData::FPoseMatchData()
	: PoseId(-1),
	AnimId(0),
	bMirror(false),
	Time(0.0f),
	LocalVelocity(FVector::ZeroVector)
{
}

FPoseMatchData::FPoseMatchData(int32 InPoseId, int32 InAnimId, float InTime, FVector& InLocalVelocity, bool bInMirror)
	: PoseId(InPoseId),
	AnimId(InAnimId),
	bMirror(bInMirror),
	Time(InTime),
	LocalVelocity(InLocalVelocity)
{
}

FMatchBone::FMatchBone()
	: PositionWeight(1.0f),
	VelocityWeight(1.0f)
{
}


FAnimNode_PoseMatchBase::FAnimNode_PoseMatchBase()
	: PoseInterval(0.1f),
	PosesEndTime(5.0f),
	BodyVelocityWeight(1.0f),
	bEnableMirroring(false),
	MirroringProfile(nullptr),
	bInitialized(false),
	bInitPoseSearch(false),
	CurrentLocalVelocity(FVector::ZeroVector),
	MatchPose(nullptr),
	AnimInstanceProxy(nullptr)
{
}
#if WITH_EDITOR
void FAnimNode_PoseMatchBase::PreProcess()
{
	Poses.Empty();
}
#endif

#if WITH_EDITOR
void FAnimNode_PoseMatchBase::PreProcessAnimation(UAnimSequence* Anim, int32 AnimIndex, bool bMirror/* = false*/)
{
	if(!Anim 
	|| PoseConfig.Num() == 0)
	{
		return;
	}

	if (bMirror 
	&& !MirroringProfile) //Cannot pre-process mirrored animation if mirroring profile is null
	{
		return;
	}

	const float AnimLength = FMath::Min(Anim->GetPlayLength(), PosesEndTime);
	float CurrentTime = 0.0f;
	
	if(PoseInterval < 0.01f)
	{
		PoseInterval = 0.01f;
	}

	while (CurrentTime <= AnimLength)
	{
		int32 PoseId = Poses.Num();

		FVector RootVelocity;
		float RootRotVelocity;
		FMMPreProcessUtils::ExtractRootVelocity(RootVelocity, RootRotVelocity, Anim, CurrentTime, PoseInterval);
		
		if (bMirror)
		{
			RootVelocity.X *= -1.0f;
			RootRotVelocity *= -1.0f;
		}

		FPoseMatchData NewPoseData = FPoseMatchData(PoseId, AnimIndex, CurrentTime, RootVelocity, bMirror);

		//Process Joints for Pose
		for (int32 i = 0; i < PoseConfig.Num(); ++i)
		{
			FJointData BoneData;

			if (bMirror)
			{
				FName BoneName = PoseConfig[i].Bone.BoneName;
				FName MirrorBoneName = MirroringProfile->FindBoneMirror(BoneName);

				const FReferenceSkeleton& RefSkeleton = Anim->GetSkeleton()->GetReferenceSkeleton();

				FMMPreProcessUtils::ExtractJointData(BoneData, Anim, RefSkeleton.FindBoneIndex(MirrorBoneName), CurrentTime, PoseInterval);

				BoneData.Position.X *= -1.0f;
				BoneData.Velocity.X *= -1.0f;
			}
			else
			{
				FMMPreProcessUtils::ExtractJointData(BoneData, Anim, PoseConfig[i].Bone, CurrentTime, PoseInterval);
			}
		
			NewPoseData.BoneData.Add(BoneData);
		}

		Poses.Add(NewPoseData);
		CurrentTime += PoseInterval;
	}
}
#endif

void FAnimNode_PoseMatchBase::FindMatchPose(const FAnimationUpdateContext& Context)
{
	if(Poses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FAnimNode_PoseMatchBase: No poses recorded in node"))
		return;
	}
#if ENGINE_MAJOR_VERSION > 4
	FAnimNode_MotionRecorder* MotionRecorderNode = nullptr;
	IMotionSnapper* MotionSnapper = Context.GetMessage<IMotionSnapper>();
	if (MotionSnapper)
	{
		MotionRecorderNode = &MotionSnapper->GetNode();
	}
#else
	FAnimNode_MotionRecorder* MotionRecorderNode = Context.GetAncestor<FAnimNode_MotionRecorder>();
#endif

	if (MotionRecorderNode)
	{
		if (!bInitialized)
		{
			for (FMatchBone& MatchBone : PoseConfig)
			{
				MotionRecorderNode->RegisterBoneToRecord(MatchBone.Bone);
			}

			InitializePoseBoneRemap(Context);

			bInitialized = true;
		}

		ComputeCurrentPose(MotionRecorderNode->GetMotionPose());

		int32 MinimaCostPoseId = FMath::Clamp(GetMinimaCostPoseId(), 0, Poses.Num() - 1);

		MatchPose = &Poses[MinimaCostPoseId];
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FAnimNode_PoseMatchBase: Cannot find Motion Snapshot node to pose match against."))
		MatchPose = &Poses[0];
	}

	Sequence = FindActiveAnim();
	InternalTimeAccumulator = StartPosition = MatchPose->Time;
	PlayRateScaleBiasClamp.Reinitialize();
}

UAnimSequenceBase* FAnimNode_PoseMatchBase::FindActiveAnim()
{
	return nullptr;
}

void FAnimNode_PoseMatchBase::ComputeCurrentPose(const FCachedMotionPose& MotionPose)
{
	for (int32 i = 0; i < PoseBoneRemap.Num(); ++i)
	{
		int32 BoneIndex = PoseBoneRemap[i];

		if(const FCachedMotionBone* CachedMotionBone = MotionPose.CachedBoneData.Find(BoneIndex))
		{
			CurrentPose[i] = FJointData(CachedMotionBone->Transform.GetLocation(), CachedMotionBone->Velocity);
		}
		else
		{
			CurrentPose[i] = FJointData();
			UE_LOG(LogTemp, Warning, TEXT("Could not find pose matching remapped index."));
		}
	}
}

int32 FAnimNode_PoseMatchBase::GetMinimaCostPoseId()
{
	int32 MinimaCostPoseId = 0;
	float MinimaCost = 10000000.0f;
	for (FPoseMatchData& Pose : Poses)
	{
		float Cost = 0.0f;

		for (int32 i = 0; i < Pose.BoneData.Num(); ++i)
		{
			const FJointData& CurrentJoint = CurrentPose[i];
			const FJointData& CandidateJoint = Pose.BoneData[i];
			const FMatchBone& MatchBoneInfo = PoseConfig[i];

			Cost += FVector::Distance(CurrentJoint.Velocity, CandidateJoint.Velocity) * MatchBoneInfo.VelocityWeight;
			Cost += FVector::Distance(CurrentJoint.Position, CandidateJoint.Position) * MatchBoneInfo.PositionWeight;
		}

		//Todo: Re-Add this once the motion recorder records character velocity
		//Cost += FVector::Distance(CurrentInterpolatedPose.LocalVelocity, Pose.LocalVelocity) * Calibration.BodyWeight_Velocity;

		if (Cost < MinimaCost)
		{
			MinimaCost = Cost;
			MinimaCostPoseId = Pose.PoseId;
		}
	}

	return MinimaCostPoseId;
}

int32 FAnimNode_PoseMatchBase::GetMinimaCostPoseId(float& OutCost, int32 StartPose, int32 EndPose)
{
	if (Poses.Num() == 0)
	{
		return -1;
	}

	StartPose = FMath::Clamp(StartPose, 0, Poses.Num() - 1);
	EndPose = FMath::Clamp(EndPose, 0, Poses.Num() - 1);

	int32 MinimaCostPoseId = 0;
	OutCost = 10000000.0f;
	for (int32 i = StartPose; i < EndPose; ++i)
	{
		FPoseMatchData& Pose = Poses[i];
		
		float Cost = 0.0f;
		for (int32 k = 0; k < Pose.BoneData.Num(); ++k)
		{
			const FJointData& CurrentJoint = CurrentPose[k];
			const FJointData& CandidateJoint = Pose.BoneData[k];
			const FMatchBone& MatchBoneInfo = PoseConfig[k];

			Cost += FVector::Distance(CurrentJoint.Velocity, CandidateJoint.Velocity) * MatchBoneInfo.VelocityWeight;
			Cost += FVector::Distance(CurrentJoint.Position, CandidateJoint.Position) * MatchBoneInfo.PositionWeight;
		}

		//Todo: Re-Add this once the motion recorder records character velocity
		//Cost += FVector::Distance(CurrentInterpolatedPose.LocalVelocity, Pose.LocalVelocity) * Calibration.BodyWeight_Velocity;

		if (Cost < OutCost)
		{
			OutCost = Cost;
			MinimaCostPoseId = Pose.PoseId;
		}
	}

	return MinimaCostPoseId;
}

void FAnimNode_PoseMatchBase::InitializePoseBoneRemap(const FAnimationUpdateContext& Context)
{
	//Create the bone remap for runtime retargetting
	USkeletalMesh* SkeletalMesh = Context.AnimInstanceProxy->GetSkelMeshComponent()->SkeletalMesh;
	
	if (!SkeletalMesh)
	{
		return;
	}

	const FReferenceSkeleton& AnimBPRefSkeleton = Context.AnimInstanceProxy->GetSkeleton()->GetReferenceSkeleton();
#if ENGINE_MAJOR_VERSION > 4	
	const FReferenceSkeleton& SkelMeshRefSkeleton = SkeletalMesh->GetRefSkeleton();
#else
	const FReferenceSkeleton& SkelMeshRefSkeleton = SkeletalMesh->RefSkeleton;
#endif

	PoseBoneRemap.Empty(PoseConfig.Num() + 1);
	for (int32 i = 0; i < PoseConfig.Num(); ++i)
	{
		int32 RemapBoneIndex = SkelMeshRefSkeleton.FindBoneIndex(PoseConfig[i].Bone.BoneName);

		PoseBoneRemap.Add(RemapBoneIndex);
	}
}

bool FAnimNode_PoseMatchBase::NeedsOnInitializeAnimInstance() const
{
	return true;
}

void FAnimNode_PoseMatchBase::OnInitializeAnimInstance(const FAnimInstanceProxy* InAnimInstanceProxy, const UAnimInstance* InAnimInstance)
{
	Super::OnInitializeAnimInstance(InAnimInstanceProxy, InAnimInstance);

	if (bEnableMirroring && MirroringProfile)
	{
		MirroringData.Initialize(MirroringProfile, InAnimInstanceProxy->GetSkelMeshComponent());
	}

	CurrentPose.Empty(PoseConfig.Num() + 1);
	for (FMatchBone& MatchBone : PoseConfig)
	{
		MatchBone.Bone.Initialize(Sequence->GetSkeleton());
		CurrentPose.Emplace(FJointData());
	}
}

void FAnimNode_PoseMatchBase::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	bInitPoseSearch = true;
	AnimInstanceProxy = Context.AnimInstanceProxy;
}


void FAnimNode_PoseMatchBase::UpdateAssetPlayer(const FAnimationUpdateContext & Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);

	if (bInitPoseSearch)
	{
		FindMatchPose(Context); //Override this to setup the animation data

		if (MatchPose && Sequence)
		{
			InternalTimeAccumulator = StartPosition = FMath::Clamp(StartPosition, 0.0f, Sequence->GetPlayLength());
			const float AdjustedPlayRate = PlayRateScaleBiasClamp.ApplyTo(FMath::IsNearlyZero(PlayRateBasis) ? 0.0f : (PlayRate / PlayRateBasis), Context.GetDeltaTime());
			const float EffectivePlayRate = Sequence->RateScale * AdjustedPlayRate;

			if ((MatchPose->Time == 0.0f) && (EffectivePlayRate < 0.0f))
			{
				InternalTimeAccumulator = Sequence->GetPlayLength();
			}

			CreateTickRecordForNode(Context, Sequence, bLoopAnimation, AdjustedPlayRate);
		}

		bInitPoseSearch = false;
	}
	else
	{
		if(Sequence)
		{
			InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.f, Sequence->GetPlayLength());
			const float AdjustedPlayRate = PlayRateScaleBiasClamp.ApplyTo(FMath::IsNearlyZero(PlayRateBasis) ? 0.f : (PlayRate / PlayRateBasis), Context.GetDeltaTime());

			CreateTickRecordForNode(Context, Sequence, bLoopAnimation, AdjustedPlayRate);
		}
	}

	//Maybe get rid of this and make my own
	//FAnimNode_SequencePlayer::UpdateAssetPlayer(Context);

#if WITH_EDITORONLY_DATA
	if (FAnimBlueprintDebugData* DebugData = Context.AnimInstanceProxy->GetAnimBlueprintDebugData())
	{
#if ENGINE_MAJOR_VERSION > 4
		int32 NumFrames = Sequence->GetNumberOfSampledKeys();
#else
		int32 NumFrames = Sequence->GetNumberOfFrames();
#endif

		DebugData->RecordSequencePlayer(Context.GetCurrentNodeId(), GetAccumulatedTime(), Sequence != nullptr ? Sequence->GetPlayLength() : 0.0f, Sequence != nullptr ? NumFrames : 0);
	}
#endif

#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	if (AnimInstanceProxy && MatchPose)
	{
		const USkeletalMeshComponent* SkelMeshComp = AnimInstanceProxy->GetSkelMeshComponent();
		const int32 DebugLevel = CVarPoseMatchingDebug.GetValueOnAnyThread();

		if (DebugLevel > 0)
		{
			const FTransform ComponentTransform = AnimInstanceProxy->GetComponentTransform();

			for (FJointData& JointData : MatchPose->BoneData)
			{
				FVector Point = ComponentTransform.TransformPosition(JointData.Position);

				AnimInstanceProxy->AnimDrawDebugSphere(Point, 10.0f, 12.0f, FColor::Yellow, false, -1.0f, 0.5f);
			}

			if(DebugLevel > 1)
			{
				for (int i = 0; i < PoseConfig.Num(); ++i)
				{
					const float Progress = ((float)i) / ((float)PoseConfig.Num() - 1);
					FColor Color = (FLinearColor::Blue + Progress * (FLinearColor::Red - FLinearColor::Blue)).ToFColor(true);

					FVector LastPoint = FVector::ZeroVector;
					int LastAnimId = -1;
					for (FPoseMatchData& Pose : Poses)
					{
						FVector Point = ComponentTransform.TransformPosition(Pose.BoneData[i].Position);
						
						AnimInstanceProxy->AnimDrawDebugSphere(Point, 3.0f, 6.0f, Color, false, -1.0f, 0.25f);

						if(DebugLevel > 2)
						{
							FVector ArrowPoint = ComponentTransform.TransformVector(Pose.BoneData[i].Velocity) * 0.33333f;
							AnimInstanceProxy->AnimDrawDebugDirectionalArrow(Point, ArrowPoint, 20.0f, Color, false, -1.0f, 0.0f);
						}
						
						if(Pose.AnimId == LastAnimId)
						{
							AnimInstanceProxy->AnimDrawDebugLine(LastPoint, Point, Color, false, -1.0f, 0.0f);
						}

						LastAnimId = Pose.AnimId;
						LastPoint = Point;
					}

				}
			}
		}
	}
#endif

	TRACE_ANIM_SEQUENCE_PLAYER(Context, *this);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Name"), Sequence != nullptr ? Sequence->GetFName() : NAME_None);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Sequence"), Sequence);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Playback Time"), InternalTimeAccumulator);
}

void FAnimNode_PoseMatchBase::Evaluate_AnyThread(FPoseContext& Output)
{
	Super::Evaluate_AnyThread(Output);

	if (MatchPose 
	    && MatchPose->bMirror 
		&& MirroringProfile
		&& IsLODEnabled(Output.AnimInstanceProxy))
	{
		FMotionMatchingUtils::MirrorPose(Output.Pose, MirroringProfile, MirroringData,
			Output.AnimInstanceProxy->GetSkelMeshComponent());
	}
}

#undef LOCTEXT_NAMESPACE