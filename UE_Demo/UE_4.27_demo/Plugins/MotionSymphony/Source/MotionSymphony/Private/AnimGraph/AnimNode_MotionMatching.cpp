// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_MotionMatching.h"
#include "AnimationRuntime.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "MotionMatchingUtil/MotionMatchingUtils.h"

static TAutoConsoleVariable<int32> CVarMMSearchDebug(
	TEXT("a.AnimNode.MoSymph.MMSearch.Debug"),
	0,
	TEXT("Turns Motion Matching Search Debugging On / Off.\n")
	TEXT("<=0: Off \n")
	TEXT("  1: On - Candidate Trajectory Debug\n")
	TEXT("  2: On - Optimisation Error Debugging\n"));

static TAutoConsoleVariable<int32> CVarMMTrajectoryDebug(
	TEXT("a.AnimNode.MoSymph.MMTrajectory.Debug"),
	0,
	TEXT("Turns Motion Matching Trajectory Debugging On / Off. \n")
	TEXT("<=0: Off \n")
	TEXT("  1: On - Show Desired Trajectory\n")
	TEXT("  2: On - Show Chosen Trajectory\n"));

static TAutoConsoleVariable<int32> CVarMMPoseDebug(
	TEXT("a.AnimNode.MoSymph.MMPose.Debug"),
	0,
	TEXT("Turns Motion Matching Pose Debugging On / Off. \n")
	TEXT("<=0: Off \n")
	TEXT("  1: On - Show Pose Position\n")
	TEXT("  2: On - Show Pose Position and Velocity"));

static TAutoConsoleVariable<int32> CVarMMAnimDebug(
	TEXT("a.AnimNode.MoSymph.MMAnim.Debug"),
	0,
	TEXT("Turns on animation debugging for Motion Matching On / Off. \n")
	TEXT("<=0: Off \n")
	TEXT("  2: On - Show Current Anim Info"));

FAnimNode_MotionMatching::FAnimNode_MotionMatching() :
	UpdateInterval(0.1f),
	PlaybackRate(1.0f),
	BlendTime(0.3f),
	MotionData(nullptr),
	UserCalibration(nullptr),
	bBlendOutEarly(true),
	PoseMatchMethod(EPoseMatchMethod::Optimized),
	TransitionMethod(ETransitionMethod::Inertialization),
	PastTrajectoryMode(EPastTrajectoryMode::ActualHistory),
	bBlendTrajectory(false),
	TrajectoryBlendMagnitude(1.0f),
	bFavourCurrentPose(false),
	CurrentPoseFavour(0.95f),
	bEnableToleranceTest(true),
	PositionTolerance(50.0f),
	RotationTolerance(2.0f),
	//RequiredTraits(0),
	TimeSinceMotionUpdate(0.0f),
	TimeSinceMotionChosen(0.0f),
	PoseInterpolationValue(0.0f),
	bForcePoseSearch(false),
	CurrentChosenPoseId(0),
	DominantBlendChannel(0),
	bValidToEvaluate(false),
	bInitialized(false),
	bTriggerTransition(false),
	bShouldRecord(false),
	TimeSinceFirstRecord(0.0f),
	TotalRecordTime(2.0f)
{
	DesiredTrajectory.Clear();
	BlendChannels.Empty(12);
	HistoricalPosesSearchCounts.Empty(11);
	
	for (int32 i = 0; i < 30; ++i)
	{
		HistoricalPosesSearchCounts.Add(0);
	}
}

FAnimNode_MotionMatching::~FAnimNode_MotionMatching()
{

}

void FAnimNode_MotionMatching::UpdateBlending(const float DeltaTime)
{
	float HighestBlendWeight = -1.0f;
	int32 HighestBlendChannel = 0;
	for (int32 i = 0; i < BlendChannels.Num(); ++i)
	{
		bool bCurrent = i == BlendChannels.Num() - 1;

		float Weight = BlendChannels[i].Update(DeltaTime, BlendTime, bCurrent);

		if (!bCurrent && Weight < -0.05f)
		{
			BlendChannels.RemoveAt(i);
			--i;
		}
		else if (Weight > HighestBlendWeight)
		{
			HighestBlendWeight = Weight;
			HighestBlendChannel = i;
		}
	}

	DominantBlendChannel = HighestBlendChannel;
}

void FAnimNode_MotionMatching::InitializeWithPoseRecorder(const FAnimationUpdateContext& Context)
{
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

	if(MotionRecorderNode)
	{
		MotionRecorderNode->RegisterBonesToRecord(MotionData->MotionMatchConfig->PoseBones);
	}


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

	UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;

	PoseBoneRemap.Empty(MMConfig->PoseBones.Num() + 1);
	for (int32 i = 0; i < MMConfig->PoseBones.Num(); ++i)
	{
		FName BoneName = AnimBPRefSkeleton.GetBoneName(MMConfig->PoseBones[i].BoneIndex);
		int32 RemapBoneIndex = SkelMeshRefSkeleton.FindBoneIndex(BoneName);

		PoseBoneRemap.Add(RemapBoneIndex);
	}
}

void FAnimNode_MotionMatching::InitializeMatchedTransition(const FAnimationUpdateContext& Context)
{
	TimeSinceMotionChosen = 0.0f;
	TimeSinceMotionUpdate = 0.0f;

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
		ComputeCurrentPose(MotionRecorderNode->GetMotionPose());
		ScheduleTransitionPoseSearch(Context);
	}
	else
	{
		//We just jump tot he default pose because there is no way to match to external nodes.
		JumpToPose(0);
		return;
	}
}

void FAnimNode_MotionMatching::InitializeDistanceMatching(const FAnimationUpdateContext& Context)
{
	if (bBlendTrajectory)
	{
		ApplyTrajectoryBlending();
	}

	//Filter the appropriate Distance Match Groups
	FDistanceMatchGroup* DistanceMatchGroup = MotionData->DistanceMatchSections.Find(FDistanceMatchIdentifier(DistanceMatchPayload.MatchType, DistanceMatchPayload.MatchBasis));

	if(!DistanceMatchGroup || !FinalCalibrationSets.Contains(RequiredTraits))
	{
		return;
	}

	FCalibrationData& FinalCalibration = FinalCalibrationSets[RequiredTraits];

	//Go through each section, find it's distance point and closest pose and find if it is the lowest cost
	FDistanceMatchSection* LowestCostSection = nullptr;
	float LowestCost = 10000000.0f;
	float LowestCostTime = -1.0f;
	int32 LowestCostPoseId = -1;
	int32 LowestLastKeyChecked = 0;
	for (FDistanceMatchSection& Section : DistanceMatchGroup->DistanceMatchSections)
	{
		int32 LastKeyChecked = 0;
		float MatchTime = Section.FindMatchingTime(DistanceMatchPayload.MarkerDistance, LastKeyChecked);

		//Todo: Try interpolating the pose if matching is not great 
		int32 PoseId = Section.StartPoseId + FMath::RoundToInt(MatchTime / MotionData->PoseInterval);
		FPoseMotionData& Pose = MotionData->Poses[PoseId];

		//Trajectory Cost
		float Cost = FMotionMatchingUtils::ComputeTrajectoryCost(DesiredTrajectory.TrajectoryPoints,
			Pose.Trajectory, FinalCalibration);

		//Pose Cost
		Cost += FMotionMatchingUtils::ComputePoseCost(CurrentInterpolatedPose.JointData,
			Pose.JointData, FinalCalibration);

		//Body Velocity Cost
		Cost += FVector::DistSquared(CurrentInterpolatedPose.LocalVelocity, Pose.LocalVelocity) * FinalCalibration.Weight_Momentum;

		//Rotational Momentum Cost
		Cost += FMath::Abs(CurrentInterpolatedPose.RotationalVelocity - Pose.RotationalVelocity)
			* FinalCalibration.Weight_AngularMomentum;

		Cost *= Pose.Favour;

		if (Cost < LowestCost)
		{
			LowestCost = Cost;
			LowestCostTime = MatchTime;
			LowestCostSection = &Section;
			LowestCostPoseId = PoseId;
			LowestLastKeyChecked = LastKeyChecked;
		}
	}

	//Whatever pose at the desired distance has the lowest cost, use that pose for distance matching
	if (LowestCostSection)
	{
		ActiveDistanceMatchSection = LowestCostSection;
		MotionMatchingMode = EMotionMatchingMode::DistanceMatching;
		DistanceMatchTime = LowestCostTime;
		LastDistanceMatchKeyChecked = LowestLastKeyChecked;

		const FPoseMotionData& Pose = MotionData->Poses[LowestCostPoseId];

		TransitionToPose(LowestCostPoseId, Context, LowestCostTime - Pose.Time);
	}
}

void FAnimNode_MotionMatching::InitializeMotionAction(const FAnimationUpdateContext& Context)
{
	//Calculate how many poses prior to the action to use
	int32 PoseOffsetToStart = FMath::Abs(FMath::RoundHalfFromZero(MotionActionPayload.LeadLength / MotionData->PoseInterval));

	if(!FinalCalibrationSets.Contains(RequiredTraits))
	{
		return;
	}

	FCalibrationData& FinalCalibration = FinalCalibrationSets[RequiredTraits];

	//Cost function on action poses
	int32 BestPoseId = -1;
	int32 BestActionId = -1;
	float BestActionCost = 10000000.0f;
	for (int32 i = 0; i < MotionData->Actions.Num(); ++i)
	{
		FMotionAction& MotionAction = MotionData->Actions[i];

		if (MotionAction.ActionId == MotionActionPayload.ActionId) //TODO: Support Traits?
		{
			int32 PoseId = FMath::Clamp(MotionAction.PoseId - PoseOffsetToStart, 0, MotionData->Poses.Num());
			FPoseMotionData& Pose = MotionData->Poses[PoseId];

			float Cost = FMotionMatchingUtils::ComputePoseCost(Pose.JointData, CurrentInterpolatedPose.JointData, FinalCalibration);

			//TODO: Maybe add momentum
			//TODO: Maybe add rotational momentum
			//TODO: Maybe add trajectory

			Cost *= Pose.Favour;

			if (Cost < BestActionCost)
			{
				BestActionCost = Cost;
				BestActionId = i;
				BestPoseId = Pose.PoseId;
			}
		}
	}

	if (BestActionId > -1)
	{
		TransitionToPose(BestPoseId, Context);
		MotionMatchingMode = EMotionMatchingMode::Action;
		CurrentActionId = BestActionId;
		CurrentActionTime = MotionData->Actions[BestActionId].Time;
		CurrentActionEndTime = CurrentActionTime + MotionActionPayload.TailLength;
		CurrentActionTime -= MotionActionPayload.LeadLength;
		
	}
}

void FAnimNode_MotionMatching::UpdateMotionMatchingState(const float DeltaTime, const FAnimationUpdateContext& Context)
{
	if (DistanceMatchPayload.bTrigger && DistanceMatchPayload.MatchType != EDistanceMatchType::None)
	{
		InitializeDistanceMatching(Context);
	}
	else if (bTriggerTransition)
	{
		InitializeMatchedTransition(Context);
		bTriggerTransition = false;
	}
	else
	{
		UpdateMotionMatching(DeltaTime, Context);
		UpdateBlending(DeltaTime);
	}
}

void FAnimNode_MotionMatching::UpdateDistanceMatchingState(const float DeltaTime, const FAnimationUpdateContext& Context)
{
	if (DistanceMatchPayload.MatchType == EDistanceMatchType::None)
	{
		MotionMatchingMode = EMotionMatchingMode::MotionMatching;
		InitializeMatchedTransition(Context);
	}
	else
	{
		if (!UpdateDistanceMatching(DeltaTime, Context))
		{
			MotionMatchingMode = EMotionMatchingMode::MotionMatching;
			UpdateMotionMatching(DeltaTime, Context);
			UpdateBlending(DeltaTime);
		}
	}
}

void FAnimNode_MotionMatching::UpdateMotionActionState(const float DeltaTime, const FAnimationUpdateContext& Context)
{
	TimeSinceMotionChosen += DeltaTime;
	TimeSinceMotionUpdate += DeltaTime;
	CurrentActionTime += DeltaTime;

	UpdateBlending(DeltaTime);

	if (CurrentActionTime >= CurrentActionEndTime)
	{
		MotionMatchingMode = EMotionMatchingMode::MotionMatching;
	}
}

void FAnimNode_MotionMatching::UpdateMotionMatching(const float DeltaTime, const FAnimationUpdateContext& Context)
{
	bForcePoseSearch = false;
	TimeSinceMotionChosen += DeltaTime;
	TimeSinceMotionUpdate += DeltaTime;

	FAnimChannelState& PrimaryChannel = BlendChannels.Last();
	int32 AnimId = PrimaryChannel.AnimId;

	if (!PrimaryChannel.bLoop)
	{
		float CurrentBlendTime = 0.0f;

		if (bBlendOutEarly)
		{
			CurrentBlendTime = BlendTime * PrimaryChannel.Weight * PlaybackRate;
		}

		if (TimeSinceMotionChosen + PrimaryChannel.StartTime + CurrentBlendTime > PrimaryChannel.AnimLength)
		{
			bForcePoseSearch = true;
		}
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
		ComputeCurrentPose(MotionRecorderNode->GetMotionPose());
	}
	else
	{
		ComputeCurrentPose();
	}

	//If we have ran into a 'DoNotUse' pose. We need to force a new pose search
	if(CurrentInterpolatedPose.bDoNotUse)
	{
		bForcePoseSearch = true;
	}

	UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;

	//Past trajectory mode
	if (PastTrajectoryMode == EPastTrajectoryMode::CopyFromCurrentPose)
	{
		for (int32 i = 0; i < MMConfig->TrajectoryTimes.Num(); ++i)
		{
			if (MMConfig->TrajectoryTimes[i] > 0.0f)
			{ 
				break;
			}

			DesiredTrajectory.TrajectoryPoints[i] = CurrentInterpolatedPose.Trajectory[i];
		}
	}

	if (TimeSinceMotionUpdate >= UpdateInterval || bForcePoseSearch)
	{
		TimeSinceMotionUpdate = 0.0f;
		SchedulePoseSearch(DeltaTime, Context);
		//UE_LOG(LogTemp, Warning, TEXT("MotionMatching! %d, %d"), count, bForcePoseSearch);
		//count++;
	}
}

bool FAnimNode_MotionMatching::UpdateDistanceMatching(const float DeltaTime, const FAnimationUpdateContext& Context)
{
	UpdateBlending(DeltaTime);

	DistanceMatchTime = ActiveDistanceMatchSection->FindMatchingTime(DistanceMatchPayload.MarkerDistance, LastDistanceMatchKeyChecked);

	FAnimChannelState& PrimaryChannel = BlendChannels.Last();
	PrimaryChannel.AnimTime = DistanceMatchTime;

	if (FMath::Abs(DistanceMatchTime - ActiveDistanceMatchSection->EndTime) < 0.01f)
	{
		return false;
	}

	return true;
}

void FAnimNode_MotionMatching::ComputeCurrentPose()
{
	const float PoseInterval = FMath::Max(0.01f, MotionData->PoseInterval);

	//====== Determine the next chosen pose ========
	FAnimChannelState& ChosenChannel = BlendChannels.Last();

	float ChosenClipLength = 0.0f;
	switch (ChosenChannel.AnimType)
	{
		case EMotionAnimAssetType::Sequence: ChosenClipLength = MotionData->GetSourceAnimAtIndex(ChosenChannel.AnimId).GetPlayLength(); break;
		case EMotionAnimAssetType::BlendSpace: ChosenClipLength = MotionData->GetSourceBlendSpaceAtIndex(ChosenChannel.AnimId).GetPlayLength(); break;
		case EMotionAnimAssetType::Composite: ChosenClipLength = MotionData->GetSourceCompositeAtIndex(ChosenChannel.AnimId).GetPlayLength(); break;
	}

	float TimePassed = TimeSinceMotionChosen;
	int32 PoseIndex = ChosenChannel.StartPoseId;

	float NewChosenTime = ChosenChannel.AnimTime;
	if (ChosenChannel.AnimTime >= ChosenClipLength)
	{
		if (ChosenChannel.bLoop)
		{
			NewChosenTime = FMotionMatchingUtils::WrapAnimationTime(NewChosenTime, ChosenClipLength);
		}
		else
		{
			const float TimeToNextClip = ChosenClipLength - (TimePassed + ChosenChannel.StartTime);

			if (TimeToNextClip < PoseInterval / 2.0f)
			{
				--PoseIndex;
			}

			NewChosenTime = ChosenClipLength;
		}

		TimePassed = NewChosenTime - ChosenChannel.StartTime;
	}

	int32 NumPosesPassed = 0;
	if (TimePassed < 0.0f)
	{
		NumPosesPassed = FMath::CeilToInt(TimePassed / PoseInterval);
	}
	else
	{
		NumPosesPassed = FMath::FloorToInt(TimePassed / PoseInterval); 
	}

	CurrentChosenPoseId = PoseIndex + NumPosesPassed;

	//====== Determine the next dominant pose ========
	FAnimChannelState& DominantChannel = BlendChannels[DominantBlendChannel];

	float DominantClipLength = 0.0f;
	switch (ChosenChannel.AnimType)
	{
		case EMotionAnimAssetType::Sequence: DominantClipLength = MotionData->GetSourceAnimAtIndex(DominantChannel.AnimId).GetPlayLength(); break;
		case EMotionAnimAssetType::BlendSpace: DominantClipLength = MotionData->GetSourceBlendSpaceAtIndex(DominantChannel.AnimId).GetPlayLength(); break;
		case EMotionAnimAssetType::Composite: DominantClipLength = MotionData->GetSourceCompositeAtIndex(DominantChannel.AnimId).GetPlayLength(); break;
	}

	if (TransitionMethod == ETransitionMethod::Blend)
	{
		TimePassed = DominantChannel.Age;
	}
	else
	{
		TimePassed = TimeSinceMotionChosen;
	}

	PoseIndex = DominantChannel.StartPoseId;

	//Determine if the new time is out of bounds of the dominant pose clip
	float NewDominantTime = DominantChannel.StartTime + TimePassed;
	if (NewDominantTime >= DominantClipLength)
	{
		if (DominantChannel.bLoop)
		{
			NewDominantTime = FMotionMatchingUtils::WrapAnimationTime(NewDominantTime, DominantClipLength);
		}
		else
		{
			float TimeToNextClip = DominantClipLength - (TimePassed + DominantChannel.StartTime);

			if (TimeToNextClip < PoseInterval)
			{
				--PoseIndex;
			}

			NewDominantTime = DominantClipLength;
		}

		TimePassed = NewDominantTime - DominantChannel.StartTime;
	}

	if (TimePassed < -0.00001f)
	{
		NumPosesPassed = FMath::CeilToInt(TimePassed / PoseInterval);
	}
	else
	{
		NumPosesPassed = FMath::CeilToInt(TimePassed / PoseInterval);
	}

	PoseIndex = FMath::Clamp(PoseIndex + NumPosesPassed, 0, MotionData->Poses.Num());

	//Get the before and after poses and then interpolate
	FPoseMotionData* BeforePose;
	FPoseMotionData* AfterPose;

	if (TimePassed < -0.00001f)
	{
		AfterPose = &MotionData->Poses[PoseIndex];
		BeforePose = &MotionData->Poses[FMath::Clamp(AfterPose->LastPoseId, 0, MotionData->Poses.Num() - 1)];

		PoseInterpolationValue = 1.0f - FMath::Abs((TimePassed / PoseInterval) - (float)NumPosesPassed);
	}
	else
	{
		BeforePose = &MotionData->Poses[FMath::Min(PoseIndex, MotionData->Poses.Num() - 2)];
		AfterPose = &MotionData->Poses[BeforePose->NextPoseId];

		PoseInterpolationValue = (TimePassed / PoseInterval) - (float)NumPosesPassed;
	}

	FMotionMatchingUtils::LerpPose(CurrentInterpolatedPose, *BeforePose, *AfterPose, PoseInterpolationValue);
}

void FAnimNode_MotionMatching::ComputeCurrentPose(const FCachedMotionPose& CachedMotionPose)
{
	const float PoseInterval = FMath::Max(0.01f, MotionData->PoseInterval);

	//====== Determine the next chosen pose ========
	FAnimChannelState& ChosenChannel = BlendChannels.Last();

	float ChosenClipLength = 0.0f;
	switch (ChosenChannel.AnimType)
	{
		case EMotionAnimAssetType::Sequence: ChosenClipLength = MotionData->GetSourceAnimAtIndex(ChosenChannel.AnimId).GetPlayLength(); break;
		case EMotionAnimAssetType::BlendSpace: ChosenClipLength = MotionData->GetSourceBlendSpaceAtIndex(ChosenChannel.AnimId).GetPlayLength(); break;
		case EMotionAnimAssetType::Composite: ChosenClipLength = MotionData->GetSourceCompositeAtIndex(ChosenChannel.AnimId).GetPlayLength(); break;
	}

	float TimePassed = TimeSinceMotionChosen;
	int32 PoseIndex = ChosenChannel.StartPoseId;

	float NewChosenTime = ChosenChannel.AnimTime;
	if (ChosenChannel.AnimTime >= ChosenClipLength)
	{
		if (ChosenChannel.bLoop)
		{
			NewChosenTime = FMotionMatchingUtils::WrapAnimationTime(NewChosenTime, ChosenClipLength);
		}
		else
		{
			float TimeToNextClip = ChosenClipLength - (TimePassed + ChosenChannel.StartTime);

			if (TimeToNextClip < PoseInterval / 2.0f)
			{
				--PoseIndex;
			}

			NewChosenTime = ChosenClipLength;
		}

		TimePassed = NewChosenTime - ChosenChannel.StartTime;
	}

	int32 NumPosesPassed = 0;
	if (TimePassed < 0.0f)
	{
		NumPosesPassed = FMath::CeilToInt(TimePassed / PoseInterval);
	}
	else
	{
		NumPosesPassed = FMath::FloorToInt(TimePassed / PoseInterval);
	}
	
	CurrentChosenPoseId = PoseIndex + NumPosesPassed;

	//====== Determine the next dominant pose ========
	FAnimChannelState& DominantChannel = BlendChannels[DominantBlendChannel];

	float DominantClipLength = 0.0f;
	switch (ChosenChannel.AnimType)
	{
	case EMotionAnimAssetType::Sequence: DominantClipLength = MotionData->GetSourceAnimAtIndex(DominantChannel.AnimId).GetPlayLength(); break;
	case EMotionAnimAssetType::BlendSpace: DominantClipLength = MotionData->GetSourceBlendSpaceAtIndex(DominantChannel.AnimId).GetPlayLength(); break;
	case EMotionAnimAssetType::Composite: DominantClipLength = MotionData->GetSourceCompositeAtIndex(DominantChannel.AnimId).GetPlayLength(); break;
	}

	if (TransitionMethod == ETransitionMethod::Blend)
	{
		TimePassed = DominantChannel.Age;
	}
	else
	{
		TimePassed = TimeSinceMotionChosen;
	}

	PoseIndex = DominantChannel.StartPoseId;

	//Determine if the new time is out of bounds of the dominant pose clip
	float NewDominantTime = DominantChannel.StartTime + TimePassed;
	if (NewDominantTime >= DominantClipLength)
	{
		if (DominantChannel.bLoop)
		{
			NewDominantTime = FMotionMatchingUtils::WrapAnimationTime(NewDominantTime, DominantClipLength);
		}
		else
		{
			const float TimeToNextClip = DominantClipLength - (TimePassed + DominantChannel.StartTime);

			if (TimeToNextClip < PoseInterval)
			{
				--PoseIndex;
			}

			NewDominantTime = DominantClipLength;
		}

		TimePassed = NewDominantTime - DominantChannel.StartTime;
	}

	if (TimePassed < -0.00001f)
	{
		NumPosesPassed = FMath::CeilToInt(TimePassed / PoseInterval);
	}
	else
	{
		NumPosesPassed = FMath::CeilToInt(TimePassed / PoseInterval);
	}

	PoseIndex = FMath::Clamp(PoseIndex + NumPosesPassed, 0, MotionData->Poses.Num());

	//Get the before and after poses and then interpolate
	FPoseMotionData* BeforePose;
	FPoseMotionData* AfterPose;

	if (TimePassed < -0.00001f)
	{
		AfterPose = &MotionData->Poses[PoseIndex];
		BeforePose = &MotionData->Poses[FMath::Clamp(AfterPose->LastPoseId, 0, MotionData->Poses.Num() - 1)];

		PoseInterpolationValue = 1.0f - FMath::Abs((TimePassed / PoseInterval) - (float)NumPosesPassed);
	}
	else
	{
		BeforePose = &MotionData->Poses[FMath::Min(PoseIndex, MotionData->Poses.Num() - 2)];
		AfterPose = &MotionData->Poses[BeforePose->NextPoseId];

		PoseInterpolationValue = (TimePassed / PoseInterval) - (float)NumPosesPassed;
	}

	FMotionMatchingUtils::LerpPoseTrajectory(CurrentInterpolatedPose, *BeforePose, *AfterPose, PoseInterpolationValue);

	UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;

	for (int32 i = 0; i < PoseBoneRemap.Num(); ++i)
	{
		const FCachedMotionBone& CachedMotionBone = CachedMotionPose.CachedBoneData[PoseBoneRemap[i]];
		CurrentInterpolatedPose.JointData[i] = FJointData(CachedMotionBone.Transform.GetLocation(), CachedMotionBone.Velocity);
	}
}

void FAnimNode_MotionMatching::SchedulePoseSearch(float DeltaTime, const FAnimationUpdateContext& Context)
{
	if (bBlendTrajectory)
		ApplyTrajectoryBlending();

	CurrentChosenPoseId = FMath::Clamp(CurrentChosenPoseId, 0, MotionData->Poses.Num() - 1); //Just in case
	FPoseMotionData& NextPose = MotionData->Poses[MotionData->Poses[CurrentChosenPoseId].NextPoseId];

	if (!bForcePoseSearch && bEnableToleranceTest)
	{
		if (NextPoseToleranceTest(NextPose))
		{
			TimeSinceMotionUpdate = 0.0f;
			return;
		}
	}

	int32 LowestPoseId = NextPose.PoseId;

	switch (PoseMatchMethod)
	{
		case EPoseMatchMethod::Optimized: { LowestPoseId = GetLowestCostPoseId(NextPose); } break;
		case EPoseMatchMethod::Linear: { LowestPoseId = GetLowestCostPoseId_Linear(NextPose); } break;
	}

	/*-----------------------------xc-RecordTrajectory------------------------*/
	if (bShouldRecord) {
		// GEngine->AddOnScreenDebugMessage(-1, 3, FColor::Blue, TEXT("Recored From MMNode Started!"));
		RecordedTrajectories.Add(DesiredTrajectory);
	}
	else if(RecordedTrajectories.Num()>3){
		// Save to Json
		FString JsonStr = "";
		FString FilePath = FPaths::ProjectPluginsDir() + TEXT("JsonData/PredictTrajectoryFromMMNode.json");
		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonStr);
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteArrayStart(TEXT("Trajectory"));
		int trajNum = 0;
		for (FTrajectory& traj : RecordedTrajectories) {
			trajNum++;
			JsonWriter->WriteObjectStart();
			JsonWriter->WriteArrayStart(TEXT("TrajectoryPoints"));
			for (FTrajectoryPoint& trajPoint : traj.TrajectoryPoints) {
				JsonWriter->WriteObjectStart();

				JsonWriter->WriteObjectStart(TEXT("Position"));
				JsonWriter->WriteValue(TEXT("X"), trajPoint.Position.X);
				JsonWriter->WriteValue(TEXT("Y"), trajPoint.Position.Y);
				JsonWriter->WriteValue(TEXT("Z"), trajPoint.Position.Z);
				JsonWriter->WriteObjectEnd();

				JsonWriter->WriteObjectStart(TEXT("Rotation"));
				JsonWriter->WriteValue(TEXT("RotationZ"), trajPoint.RotationZ);
				JsonWriter->WriteObjectEnd();

				JsonWriter->WriteObjectEnd();
			}
			JsonWriter->WriteArrayEnd();
			JsonWriter->WriteObjectEnd();
		}
		JsonWriter->WriteArrayEnd();
		JsonWriter->WriteValue(TEXT("PoseCount"), trajNum);
		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();
		FFileHelper::SaveStringToFile(JsonStr, *FilePath);

		GEngine->AddOnScreenDebugMessage(-1, 3, FColor::Blue, TEXT("Recored From MMNode Finished!"));
		RecordedTrajectories.Empty(trajNum);
	}

#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	const int32 DebugLevel = CVarMMSearchDebug.GetValueOnAnyThread();

	if(DebugLevel == 2)
	{
		PerformLinearSearchComparison(Context.AnimInstanceProxy, LowestPoseId, NextPose);
	}
#endif

	FPoseMotionData& BestPose = MotionData->Poses[LowestPoseId];
	FPoseMotionData& ChosenPose = MotionData->Poses[CurrentChosenPoseId];

	bool bWinnerAtSameLocation = BestPose.AnimId == CurrentInterpolatedPose.AnimId &&
								 BestPose.bMirrored == CurrentInterpolatedPose.bMirrored &&
								FMath::Abs(BestPose.Time - CurrentInterpolatedPose.Time) < 0.25f
								&& FVector2D::DistSquared(BestPose.BlendSpacePosition, CurrentInterpolatedPose.BlendSpacePosition) < 1.0f;

	

	if (!bWinnerAtSameLocation)
	{
		bWinnerAtSameLocation = BestPose.AnimId == ChosenPose.AnimId &&
								BestPose.bMirrored == ChosenPose.bMirrored &&
								FMath::Abs(BestPose.Time - ChosenPose.Time) < 0.25f
								&& FVector2D::DistSquared(BestPose.BlendSpacePosition, ChosenPose.BlendSpacePosition) < 1.0f;
	}

	if (!bWinnerAtSameLocation)
	{
		TransitionToPose(BestPose.PoseId, Context);
	}
}

void FAnimNode_MotionMatching::ScheduleTransitionPoseSearch(const FAnimationUpdateContext & Context)
{
	int32 LowestPoseId = GetLowestCostPoseId();

	LowestPoseId = FMath::Clamp(LowestPoseId, 0, MotionData->Poses.Num() - 1);
	JumpToPose(LowestPoseId);
}

int32 FAnimNode_MotionMatching::GetLowestCostPoseId()
{
	if (!FinalCalibrationSets.Contains(RequiredTraits))
	{
		return CurrentChosenPoseId;
	}

	FCalibrationData& FinalCalibration = FinalCalibrationSets[RequiredTraits];

	int32 LowestPoseId = 0;
	float LowestCost = 10000000.0f;
	for (FPoseMotionData& Pose : MotionData->Poses)
	{
		if (Pose.bDoNotUse 
		 && Pose.Traits != RequiredTraits)
		{
			continue;
		}

		float Cost = FMotionMatchingUtils::ComputeTrajectoryCost(DesiredTrajectory.TrajectoryPoints,
			Pose.Trajectory, FinalCalibration);

		Cost += FMotionMatchingUtils::ComputePoseCost(CurrentInterpolatedPose.JointData,
			Pose.JointData, FinalCalibration);

		Cost += FVector::DistSquared(CurrentInterpolatedPose.LocalVelocity, Pose.LocalVelocity) * FinalCalibration.Weight_Momentum;

		Cost *= Pose.Favour;

		if (Cost < LowestCost)
		{
			LowestCost = Cost;
			LowestPoseId = Pose.PoseId;
		}
	}

	return LowestPoseId;
}

int32 FAnimNode_MotionMatching::GetLowestCostPoseId(FPoseMotionData& NextPose)
{
	if (!FinalCalibrationSets.Contains(RequiredTraits))
	{
		return CurrentChosenPoseId;
	}

	FCalibrationData& FinalCalibration = FinalCalibrationSets[RequiredTraits];

	TArray<FPoseMotionData>* PoseCandidates = MotionData->OptimisationModule->GetFilteredPoseList(CurrentInterpolatedPose, RequiredTraits, FinalCalibration);

	if (!PoseCandidates)
	{
		return GetLowestCostPoseId_Linear(NextPose);
	}

	// get current top10 lowest pose
	TArray<FPoseCostInfo> LowestPoses;
	float tempDiff = 0.f;


	int32 LowestPoseId = 0;
	float LowestCost = 10000000.0f;
	for (FPoseMotionData& Pose : *PoseCandidates)
	{
		//Body Momentum
		float Cost = FVector::DistSquared(CurrentInterpolatedPose.LocalVelocity, Pose.LocalVelocity) * FinalCalibration.Weight_Momentum;

		float MomentumCost = Cost;

		//Body Rotational Momentum
		Cost += FMath::Abs(CurrentInterpolatedPose.RotationalVelocity - Pose.RotationalVelocity)
			* FinalCalibration.Weight_AngularMomentum;

		float RotationalCost = Cost - MomentumCost;

		if(Cost > LowestCost) 
		{
			continue; //Early Out?
		}

		//Trajectory Cost 
		const int32 TrajectoryIterations = FMath::Min(DesiredTrajectory.TrajectoryPoints.Num(), FinalCalibration.TrajectoryWeights.Num());
		for (int32 i = 0; i < TrajectoryIterations; ++i)
		{
			const FTrajectoryWeightSet WeightSet = FinalCalibration.TrajectoryWeights[i];
			const FTrajectoryPoint CurrentPoint = DesiredTrajectory.TrajectoryPoints[i];
			const FTrajectoryPoint CandidatePoint = Pose.Trajectory[i];

			Cost += FVector::DistSquared(CandidatePoint.Position, CurrentPoint.Position) * WeightSet.Weight_Pos;
			Cost += FMath::Abs(FMath::FindDeltaAngleDegrees(CandidatePoint.RotationZ, CurrentPoint.RotationZ)) * WeightSet.Weight_Facing;
		}

		float TrajectoryCost = Cost - MomentumCost - RotationalCost;

		if (Cost > LowestCost) 
		{
			continue; //Early Out?
		}

		for (int32 i = 0; i < CurrentInterpolatedPose.JointData.Num(); ++i)
		{
			const FJointWeightSet WeightSet = FinalCalibration.PoseJointWeights[i];
			const FJointData CurrentJoint = CurrentInterpolatedPose.JointData[i];
			const FJointData CandidateJoint = Pose.JointData[i];

			Cost += FVector::DistSquared(CurrentJoint.Velocity, CandidateJoint.Velocity) * WeightSet.Weight_Vel;
			Cost += FVector::DistSquared(CurrentJoint.Position, CandidateJoint.Position) * WeightSet.Weight_Pos;
		}

		float PoseCost = Cost - TrajectoryCost - MomentumCost - RotationalCost;

		//Favour Current Pose
		if (bFavourCurrentPose && Pose.PoseId == NextPose.PoseId)
		{
			Cost *= CurrentPoseFavour;
		}

		//Apply Pose Favour
		Cost *= Pose.Favour;

		if (Cost < LowestCost)
		{
			LowestCost = Cost;
			LowestPoseId = Pose.PoseId;
		}

		// 将最小的PoseId添加到前十小的候选姿势中，用于Debug
		UpdateLowestPoses(Pose, LowestPoses, Cost, MomentumCost, RotationalCost, TrajectoryCost, PoseCost);
	}
	/*--------------------xc------------------------*/
	if (CustomDebugData != nullptr) {
		// Update LowesetCostCandidates
		CustomDebugData->LowestCostCandidates = LowestPoses;

		// Update PoseSearchCountsForDebug & Draw CandidateTrajectory
		CustomDebugData->PoseSearchCountsForDebug.Add(PoseCandidates->Num());
		CustomDebugData->PoseSearchCountsForDebug.RemoveAt(0);
		if (CustomDebugData->SearchDebugLevel == 1) {
			DrawCandidateTrajectories(PoseCandidates);
		}
	}
	/*--------------------xc------------------------*/
#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	const int32 DebugLevel = CVarMMSearchDebug.GetValueOnAnyThread();
	if (DebugLevel == 1)
	{
		HistoricalPosesSearchCounts.Add(PoseCandidates->Num());
		HistoricalPosesSearchCounts.RemoveAt(0);

		DrawCandidateTrajectories(PoseCandidates);
	}
#endif

	return LowestPoseId;
}

int32 FAnimNode_MotionMatching::GetLowestCostPoseId_Linear(FPoseMotionData& NextPose)
{
	if (!FinalCalibrationSets.Contains(RequiredTraits))
	{
		return CurrentChosenPoseId;
	}

	FCalibrationData& FinalCalibration = FinalCalibrationSets[RequiredTraits];

	int32 LowestPoseId = 0;
	float LowestCost = 10000000.0f;
	for (FPoseMotionData& Pose : MotionData->Poses)
	{
		if (Pose.bDoNotUse || Pose.Traits != RequiredTraits)
		{
			continue;
		}

		//Body Velocity Cost
		float Cost = FVector::DistSquared(CurrentInterpolatedPose.LocalVelocity, Pose.LocalVelocity) * FinalCalibration.Weight_Momentum;

		//Body Rotational Velocity Cost
		Cost += FMath::Abs(CurrentInterpolatedPose.RotationalVelocity - Pose.RotationalVelocity)
			* FinalCalibration.Weight_AngularMomentum;

		if(Cost > LowestCost) 
		{
			continue; //Early out
		}

		//Pose Trajectory Cost
		Cost += FMotionMatchingUtils::ComputeTrajectoryCost(DesiredTrajectory.TrajectoryPoints,
			Pose.Trajectory, FinalCalibration);

		if(Cost > LowestCost) 
		{
			continue; //Early out
		}

		// Pose Joint Cost
		Cost += FMotionMatchingUtils::ComputePoseCost(CurrentInterpolatedPose.JointData,
			Pose.JointData, FinalCalibration);
		
		//Pose Favour
		Cost *= Pose.Favour;

		//Favour Current Pose
		if (bFavourCurrentPose && Pose.PoseId == NextPose.PoseId)
		{
			Cost *= CurrentPoseFavour;
		}

		if (Cost < LowestCost)
		{
			LowestCost = Cost;
			LowestPoseId = Pose.PoseId;
		}
	}

	return LowestPoseId;
}

void FAnimNode_MotionMatching::TransitionToPose(const int32 PoseId, const FAnimationUpdateContext& Context, const float TimeOffset /*= 0.0f*/)
{
	switch (TransitionMethod)
	{
		case ETransitionMethod::None: { JumpToPose(PoseId, TimeOffset); } break;
		case ETransitionMethod::Blend: { BlendToPose(PoseId, TimeOffset); } break;
		case ETransitionMethod::Inertialization:
		{
			JumpToPose(PoseId, TimeOffset);


#if ENGINE_MAJOR_VERSION > 4
			UE::Anim::IInertializationRequester* InertializationRequester = Context.GetMessage<UE::Anim::IInertializationRequester>();
			if (InertializationRequester)
			{
				InertializationRequester->RequestInertialization(BlendTime);
				InertializationRequester->AddDebugRecord(*Context.AnimInstanceProxy, Context.GetCurrentNodeId());
			}
			else
			{
				//FAnimNode_Inertialization::LogRequestError(Context, BlendPose[ChildIndex]);
				UE_LOG(LogTemp, Error, TEXT("Motion Matching Node: Failed to get inertialisation node ancestor in the animation graph. Either add an inertialiation node or change the blend type."));
			}
#else
			FAnimNode_Inertialization* InertializationNode = Context.GetAncestor<FAnimNode_Inertialization>();

			if (InertializationNode)
			{
				InertializationNode->RequestInertialization(BlendTime);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Motion Matching Node: Failed to get inertialisation node ancestor in the animation graph. Either add an inertialiation node or change the blend type."));
			}		
#endif		
		} break;
	}
}

void FAnimNode_MotionMatching::JumpToPose(int32 PoseId, float TimeOffset /*= 0.0f */)
{
	TimeSinceMotionChosen = TimeSinceMotionUpdate;
	CurrentChosenPoseId = PoseId;

	BlendChannels.Empty(TransitionMethod == ETransitionMethod::Blend ? 12 : 1);
	FPoseMotionData& Pose = MotionData->Poses[PoseId];

	switch (Pose.AnimType)
	{
		//Sequence Pose
		case EMotionAnimAssetType::Sequence: 
		{
			const FMotionAnimSequence& MotionAnim = MotionData->GetSourceAnimAtIndex(Pose.AnimId);

			if (!MotionAnim.Sequence)
			{
				break;
			}

			BlendChannels.Emplace(FAnimChannelState(Pose, EBlendStatus::Dominant, 1.0f,
				MotionAnim.Sequence->GetPlayLength(), MotionAnim.bLoop, Pose.bMirrored, TimeSinceMotionChosen, TimeOffset));

		} break;
		//Blend Space Pose
		case EMotionAnimAssetType::BlendSpace:
		{
			const FMotionBlendSpace& MotionBlendSpace = MotionData->GetSourceBlendSpaceAtIndex(Pose.AnimId);

			if (!MotionBlendSpace.BlendSpace)
			{
				break;
			}

			BlendChannels.Emplace(FAnimChannelState(Pose, EBlendStatus::Dominant, 1.0f,
				MotionBlendSpace.BlendSpace->AnimLength, MotionBlendSpace.bLoop, Pose.bMirrored, TimeSinceMotionChosen, TimeOffset));

			MotionBlendSpace.BlendSpace->GetSamplesFromBlendInput(FVector(
				Pose.BlendSpacePosition.X, Pose.BlendSpacePosition.Y, 0.0f), 
				BlendChannels.Last().BlendSampleDataCache);

		} break;
		//Composites
		case EMotionAnimAssetType::Composite:
		{
			const FMotionComposite& MotionComposite = MotionData->GetSourceCompositeAtIndex(Pose.AnimId);

			if (!MotionComposite.AnimComposite)
			{
				break;
			}

			BlendChannels.Emplace(FAnimChannelState(Pose, EBlendStatus::Dominant, 1.0f,
				MotionComposite.AnimComposite->GetPlayLength(), MotionComposite.bLoop, Pose.bMirrored, TimeSinceMotionChosen, TimeOffset));
		}
		default: 
		{ 
			return; 
		}
	}

	DominantBlendChannel = 0;
}

void FAnimNode_MotionMatching::BlendToPose(int32 PoseId, float TimeOffset /*= 0.0f */)
{
	TimeSinceMotionChosen = TimeSinceMotionUpdate;
	CurrentChosenPoseId = PoseId;

	//BlendChannels.Last().BlendStatus = EBlendStatus::Decay;

	FPoseMotionData& Pose = MotionData->Poses[PoseId];

	switch (Pose.AnimType)
	{
		//Sequence Pose
		case EMotionAnimAssetType::Sequence:
		{
			const FMotionAnimSequence& MotionAnim = MotionData->GetSourceAnimAtIndex(Pose.AnimId);

			BlendChannels.Emplace(FAnimChannelState(Pose, EBlendStatus::Chosen, 1.0f,
				MotionAnim.Sequence->GetPlayLength(), MotionAnim.bLoop, Pose.bMirrored, TimeSinceMotionChosen, TimeOffset));

		} break;
		//Blend Space Pose
		case EMotionAnimAssetType::BlendSpace:
		{
			const FMotionBlendSpace& MotionBlendSpace = MotionData->GetSourceBlendSpaceAtIndex(Pose.AnimId);

			BlendChannels.Emplace(FAnimChannelState(Pose, EBlendStatus::Chosen, 1.0f,
				MotionBlendSpace.BlendSpace->AnimLength, MotionBlendSpace.bLoop, Pose.bMirrored, TimeSinceMotionChosen, TimeOffset));

			MotionBlendSpace.BlendSpace->GetSamplesFromBlendInput(FVector(
				Pose.BlendSpacePosition.X, Pose.BlendSpacePosition.Y, 0.0f),
				BlendChannels.Last().BlendSampleDataCache);

		} break;
		//Composites
		case EMotionAnimAssetType::Composite:
		{
			const FMotionComposite& MotionComposite = MotionData->GetSourceCompositeAtIndex(Pose.AnimId);

			BlendChannels.Emplace(FAnimChannelState(Pose, EBlendStatus::Chosen, 1.0f,
				MotionComposite.AnimComposite->GetPlayLength(), MotionComposite.bLoop, Pose.bMirrored, TimeSinceMotionChosen, TimeOffset));

		} break;
		//Default
		default: 
		{ 
			return; 
		}
	}
}

bool FAnimNode_MotionMatching::NextPoseToleranceTest(FPoseMotionData& NextPose)
{
	if (NextPose.bDoNotUse 
	|| NextPose.Traits != RequiredTraits)
	{
		return false;
	}

	UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;

	//We already know that the next Pose data will have good Pose transition so we only
	//need to test trajectory (closeness). Additionally there is no need to test past trajectory
	int32 PointCount = FMath::Min(DesiredTrajectory.TrajectoryPoints.Num(), MMConfig->TrajectoryTimes.Num());
	for (int32 i = 0; i < PointCount; ++i)
	{
		float PredictionTime = MMConfig->TrajectoryTimes[i];

		if (PredictionTime > 0.0f)
		{
			float RelativeTolerance_Pos = PredictionTime * PositionTolerance;
			float RelativeTolerance_Angle = PredictionTime * RotationTolerance;

			FTrajectoryPoint& NextPoint = NextPose.Trajectory[i];
			FTrajectoryPoint& DesiredPoint = DesiredTrajectory.TrajectoryPoints[i];

			FVector DiffVector = NextPoint.Position - DesiredPoint.Position;
			float SqrDistance = DiffVector.SizeSquared();
			
			if (SqrDistance > RelativeTolerance_Pos * RelativeTolerance_Pos)
			{
				return false;
			}

			float AngleDelta = FMath::FindDeltaAngleDegrees(DesiredPoint.RotationZ/* - 90.0f*/, NextPoint.RotationZ);
		
			if (FMath::Abs(AngleDelta) > RelativeTolerance_Angle)
			{
				return false;
			}
		}
	}

	return true;
}

void FAnimNode_MotionMatching::ApplyTrajectoryBlending()
{
	UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;

	const float TotalTime = FMath::Max(0.0001f, MMConfig->TrajectoryTimes.Last());

	for (int i = 0; i < MMConfig->TrajectoryTimes.Num(); ++i)
	{
		const float Time = MMConfig->TrajectoryTimes[i];
		if (Time > 0.0f)
		{
			FTrajectoryPoint& DesiredPoint = DesiredTrajectory.TrajectoryPoints[i];
			FTrajectoryPoint& CurrentPoint = CurrentInterpolatedPose.Trajectory[i];

			float Progress = ((TotalTime - Time) / TotalTime) * TrajectoryBlendMagnitude;

			DesiredPoint.Position = FMath::Lerp(DesiredPoint.Position, CurrentPoint.Position, Progress);
		}
	}
}

float FAnimNode_MotionMatching::GetCurrentAssetTime()
{
	return InternalTimeAccumulator;
}

float FAnimNode_MotionMatching::GetCurrentAssetTimePlayRateAdjusted()
{
	UAnimSequenceBase* Sequence = GetPrimaryAnim();

	float EffectivePlayrate = PlaybackRate * (Sequence ? Sequence->RateScale : 1.0f);
	float Length = Sequence ? Sequence->GetPlayLength() : 0.0f;

	return (EffectivePlayrate < 0.0f) ? Length - InternalTimeAccumulator : InternalTimeAccumulator;
}

float FAnimNode_MotionMatching::GetCurrentAssetLength()
{
	UAnimSequenceBase* Sequence = GetPrimaryAnim();
	return Sequence ? Sequence->GetPlayLength() : 0.0f;
}

UAnimationAsset* FAnimNode_MotionMatching::GetAnimAsset()
{
	return MotionData;
}

bool FAnimNode_MotionMatching::NeedsOnInitializeAnimInstance() const
{
	return true;
}

void FAnimNode_MotionMatching::OnInitializeAnimInstance(const FAnimInstanceProxy* InAnimInstanceProxy, const UAnimInstance* InAnimInstance)
{
	Super::OnInitializeAnimInstance(InAnimInstanceProxy, InAnimInstance);

	bValidToEvaluate = true;

	//Validate Motion Data
	if (!MotionData)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion matching node failed to initialize. Motion Data has not been set."))
		bValidToEvaluate = false;
		return;
	}

	//Validate Motion Matching Configuration
	UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;
	if (MMConfig)
	{
		MMConfig->Initialize();
		CurrentInterpolatedPose = FPoseMotionData(MMConfig->TrajectoryTimes.Num(), MMConfig->PoseBones.Num());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Motion matching node failed to initialize. Motion Match Config has not been set on the motion matching node."));
		bValidToEvaluate = false;
		return;
	}

	//Validate MMConfig matches internal calibration (i.e. the MMConfig has not been since changed
	for(auto& TraitCalibPair : MotionData->FeatureStandardDeviations)
	{
		FCalibrationData& CalibData = TraitCalibPair.Value;

		if (!CalibData.IsValidWithConfig(MMConfig))
		{
			UE_LOG(LogTemp, Error, TEXT("Motion matching node failed to initialize. Internal calibration sets atom count does not match the motion config. Did you change the motion config and forget to pre-process?"));
			bValidToEvaluate = false;
			return;
		}
	}

	//Validate Motion Matching optimization is setup correctly otherwise revert to Linear search
	if (PoseMatchMethod == EPoseMatchMethod::Optimized 
		&& MotionData->IsOptimisationValid())
	{
		MotionData->OptimisationModule->InitializeRuntime();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Motion matching node was set to run in optimized mode. However, the optimisation setup is invalid and optimization will be disabled. Did you forget to pre-process your motion data with optimisation on?"));
		PoseMatchMethod = EPoseMatchMethod::Linear;
	}

	//If the user calibration is not set on the motion data asset, get it from the Motion Data instead
	if (!UserCalibration)
	{
		UserCalibration = MotionData->PreprocessCalibration;
	}

	if (UserCalibration)
	{
		UserCalibration->ValidateData();

		for (auto& FeatureStdDevPairs : MotionData->FeatureStandardDeviations)
		{
			FCalibrationData& NewFinalCalibration = FinalCalibrationSets.Add(FeatureStdDevPairs.Key, FCalibrationData());
			NewFinalCalibration.GenerateFinalWeights(UserCalibration, FeatureStdDevPairs.Value);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Motion matching node failed to initialize. Motion Calibration not set in MotionData asset."));
		bValidToEvaluate = false;
		return;
	}

	JumpToPose(0);
	UAnimSequenceBase* Sequence = GetPrimaryAnim();
	FAnimChannelState& primaryState = BlendChannels.Last();

	if (Sequence)
	{
		InternalTimeAccumulator = FMath::Clamp(primaryState.AnimTime, 0.0f, Sequence->GetPlayLength());

		if (PlaybackRate * Sequence->RateScale < 0.0f)
		{
			InternalTimeAccumulator = Sequence->GetPlayLength();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Motion matching node failed to initialize. The starting sequence is null. Check that all animations in the MotionData are valid"));
		bValidToEvaluate = false;
		return;
	}

	MirroringData.Initialize(MotionData->MirroringProfile, InAnimInstanceProxy->GetSkelMeshComponent());
}

void FAnimNode_MotionMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	InternalTimeAccumulator = 0.0f;

	if (!MotionData 
	|| !MotionData->bIsProcessed
	|| !bValidToEvaluate)
	{
		return;
	}
		
	MotionMatchingMode = EMotionMatchingMode::MotionMatching;

	if(bInitialized)
	{
		bTriggerTransition = true;
	}

	AnimInstanceProxy = Context.AnimInstanceProxy;
}

void FAnimNode_MotionMatching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);

	if (!MotionData 
	|| !MotionData->bIsProcessed
	|| !bValidToEvaluate)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Matching node failed to update properly as the setup is not valid."))
		return;
	}

	float DeltaTime = Context.GetDeltaTime();

	if (!bInitialized)
	{
		InitializeWithPoseRecorder(Context);
		bInitialized = true;
	}

	//Check for triggered action
	if (MotionActionPayload.ActionId > -1) 
	{
		InitializeMotionAction(Context);
	}

	//Update based on motion matching node
	switch (MotionMatchingMode)
	{
		case EMotionMatchingMode::MotionMatching:
		{
			UpdateMotionMatchingState(DeltaTime, Context);
		} break;
		case EMotionMatchingMode::DistanceMatching:
		{
			UpdateDistanceMatchingState(DeltaTime, Context);
		} break;
		case EMotionMatchingMode::Action:
		{
			UpdateMotionActionState(DeltaTime, Context);
		} break;
	}

	FAnimChannelState& CurrentChannel = BlendChannels.Last();

	CreateTickRecordForNode(Context, true, PlaybackRate);

#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	//Visualize the motion matching search / optimisation debugging
	const int32 SearchDebugLevel = CVarMMSearchDebug.GetValueOnAnyThread();
	if (SearchDebugLevel == 1)
	{
		DrawSearchCounts(Context.AnimInstanceProxy);
	}

	//Visualize the trajectroy debugging
	const int32 TrajDebugLevel = CVarMMTrajectoryDebug.GetValueOnAnyThread();
	
	if (TrajDebugLevel > 0)
	{
		if (TrajDebugLevel == 2)
		{
			//Draw chosen trajectory
			DrawChosenTrajectoryDebug(Context.AnimInstanceProxy);
		}

		//Draw Input trajectory
		DrawTrajectoryDebug(Context.AnimInstanceProxy);
	}

	int32 PoseDebugLevel = CVarMMPoseDebug.GetValueOnAnyThread();

	if (PoseDebugLevel > 0)
	{
		DrawChosenPoseDebug(Context.AnimInstanceProxy, PoseDebugLevel > 1);
	}

	//Debug the current animation data being played by the motion matching node
	int32 AnimDebugLevel = CVarMMAnimDebug.GetValueOnAnyThread();

	if(AnimDebugLevel > 0)
	{
		DrawAnimDebug(Context.AnimInstanceProxy);
	}
	
#endif
	
	// 更新DebugInfo
	if (CustomDebugData != nullptr) {
		UpdateDebugInfo(Context.AnimInstanceProxy);
	}

}

void FAnimNode_MotionMatching::Evaluate_AnyThread(FPoseContext& Output)
{
	if (!MotionData 
	|| !MotionData->bIsProcessed
	|| !IsLODEnabled(Output.AnimInstanceProxy))
	{
		return;
	}

	const int32 ChannelCount = BlendChannels.Num();

	if (ChannelCount == 0)
	{
		Output.Pose.ResetToRefPose();
		return;
	}
	
	if (ChannelCount > 1 && BlendTime > 0.00001f)
	{
		EvaluateBlendPose(Output, Output.AnimInstanceProxy->GetDeltaSeconds());
	}
	else
	{
		EvaluateSinglePose(Output, Output.AnimInstanceProxy->GetDeltaSeconds());
	}
}

void FAnimNode_MotionMatching::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
}

void FAnimNode_MotionMatching::EvaluateSinglePose(FPoseContext& Output, const float DeltaTime)
{
	FAnimChannelState& PrimaryChannel = BlendChannels.Last();
	float AnimTime = PrimaryChannel.AnimTime;

	switch (PrimaryChannel.AnimType)
	{
		case EMotionAnimAssetType::Sequence:
		{
			const FMotionAnimSequence& MotionSequence = MotionData->GetSourceAnimAtIndex(PrimaryChannel.AnimId);
			UAnimSequence* AnimSequence = MotionSequence.Sequence;


			if(MotionSequence.bLoop)
			{
				AnimTime = FMotionMatchingUtils::WrapAnimationTime(AnimTime, AnimSequence->GetPlayLength());
			}

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
			FAnimationPoseData AnimationPoseData(Output);
			AnimSequence->GetAnimationPose(AnimationPoseData, FAnimExtractContext(PrimaryChannel.AnimTime, true));
#else
			AnimSequence->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(PrimaryChannel.AnimTime, true));
#endif
		} break;

		case EMotionAnimAssetType::BlendSpace:
		{
			const FMotionBlendSpace& MotionBlendSpace = MotionData->GetSourceBlendSpaceAtIndex(PrimaryChannel.AnimId);
			UBlendSpaceBase* BlendSpace = MotionBlendSpace.BlendSpace;

			if (!BlendSpace)
			{
				break;
			}

			if (MotionBlendSpace.bLoop)
			{
				AnimTime = FMotionMatchingUtils::WrapAnimationTime(AnimTime, BlendSpace->AnimLength);
			}

			for (int32 i = 0; i < PrimaryChannel.BlendSampleDataCache.Num(); ++i)
			{
				PrimaryChannel.BlendSampleDataCache[i].Time = AnimTime;
			}

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
			FAnimationPoseData AnimationPoseData(Output);
			BlendSpace->GetAnimationPose(PrimaryChannel.BlendSampleDataCache, AnimationPoseData);
#else
			BlendSpace->GetAnimationPose(PrimaryChannel.BlendSampleDataCache, Output.Pose, Output.Curve);
#endif
		} break;

		case EMotionAnimAssetType::Composite:
		{
			const FMotionComposite& MotionComposite = MotionData->GetSourceCompositeAtIndex(PrimaryChannel.AnimId);
			UAnimComposite* Composite = MotionComposite.AnimComposite;

			if(!Composite)
			{
				break;
			}

			if(MotionComposite.bLoop)
			{
				AnimTime = FMotionMatchingUtils::WrapAnimationTime(AnimTime, Composite->GetPlayLength());
			}

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
			FAnimationPoseData AnimationPoseData(Output);
			Composite->GetAnimationPose(AnimationPoseData, FAnimExtractContext(PrimaryChannel.AnimTime, true));
#else
			Composite->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(PrimaryChannel.AnimTime, true));
#endif

		} break;
	}

	if (PrimaryChannel.bMirrored)
	{
		FMotionMatchingUtils::MirrorPose(Output.Pose, MotionData->MirroringProfile, MirroringData, 
			Output.AnimInstanceProxy->GetSkelMeshComponent());
	}
}

void FAnimNode_MotionMatching::EvaluateBlendPose(FPoseContext& Output, const float DeltaTime)
{
	const int32 PoseCount = BlendChannels.Num();

	if (PoseCount > 0)
	{
		//Prepare containers for blending

		TArray<FCompactPose, TInlineAllocator<8>> ChannelPoses;
		ChannelPoses.AddZeroed(PoseCount);

		TArray<FBlendedCurve, TInlineAllocator<8>> ChannelCurves;
		ChannelCurves.AddZeroed(PoseCount);

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
		TArray<FStackCustomAttributes, TInlineAllocator<8>> ChannelAttributes;
		ChannelAttributes.AddZeroed(PoseCount);
#endif

		TArray<float, TInlineAllocator<8>> ChannelWeights;
		ChannelWeights.AddZeroed(PoseCount);

		TArray<FTransform> ChannelRootMotions;
		ChannelRootMotions.AddZeroed(PoseCount);

		const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();

		for (int32 i = 0; i < ChannelPoses.Num(); ++i)
		{
			ChannelPoses[i].SetBoneContainer(&BoneContainer);
			ChannelCurves[i].InitFrom(Output.Curve);
		}

		//Extract poses from each channel

		float TotalBlendPower = 0.0f;
		for (int32 i = 0; i < PoseCount; ++i)
		{
			FCompactPose& Pose = ChannelPoses[i];
			FAnimChannelState& AnimChannel = BlendChannels[i];

			const float Weight = AnimChannel.Weight * ((((float)(i + 1)) / ((float)PoseCount)));
			ChannelWeights[i] = Weight;
			TotalBlendPower += Weight;

			float AnimTime = AnimChannel.AnimTime;

			switch (AnimChannel.AnimType)
			{
				case EMotionAnimAssetType::Sequence:
				{
					const FMotionAnimSequence& MotionAnim = MotionData->GetSourceAnimAtIndex(AnimChannel.AnimId);
					UAnimSequence* AnimSequence = MotionAnim.Sequence;

					if (MotionAnim.bLoop)
					{
						AnimTime = FMotionMatchingUtils::WrapAnimationTime(AnimTime, AnimSequence->GetPlayLength());
					}

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
					FAnimationPoseData AnimationPoseData = { Pose, ChannelCurves[i], ChannelAttributes[i] };
					MotionAnim.Sequence->GetAnimationPose(AnimationPoseData, FAnimExtractContext(AnimTime, true));
#else
					MotionAnim.Sequence->GetAnimationPose(Pose, ChannelCurves[i], FAnimExtractContext(AnimTime, true));
#endif
				} break;
				case EMotionAnimAssetType::BlendSpace:
				{
					const FMotionBlendSpace& MotionBlendSpace = MotionData->GetSourceBlendSpaceAtIndex(AnimChannel.AnimId);
					UBlendSpaceBase* BlendSpace = MotionBlendSpace.BlendSpace;

					if(!BlendSpace)
						break;

					if (MotionBlendSpace.bLoop)
						AnimTime = FMotionMatchingUtils::WrapAnimationTime(AnimTime, BlendSpace->AnimLength);

					for (int32 k = 0; k < AnimChannel.BlendSampleDataCache.Num(); ++k)
					{
						AnimChannel.BlendSampleDataCache[k].Time = AnimTime;
					}

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
					FAnimationPoseData AnimationPoseData = { Pose, ChannelCurves[i], ChannelAttributes[i] };
					BlendSpace->GetAnimationPose(AnimChannel.BlendSampleDataCache, AnimationPoseData);
#else
					BlendSpace->GetAnimationPose(AnimChannel.BlendSampleDataCache, Pose, ChannelCurves);
#endif
				}
				break;
				default: ;
			}

			if(AnimChannel.bMirrored)
			{
				FMotionMatchingUtils::MirrorPose(Pose, MotionData->MirroringProfile, MirroringData, Output.AnimInstanceProxy->GetSkelMeshComponent());
			}
		}

		//Blend poses together according to their weights
		if (TotalBlendPower > 0.0f)
		{
			for (int32 i = 0; i < PoseCount; ++i)
			{
				ChannelWeights[i] = ChannelWeights[i] / TotalBlendPower;
			}

			TArrayView<FCompactPose> ChannelPoseView(ChannelPoses);
	
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
			FAnimationPoseData AnimationPoseData(Output);
			FAnimationRuntime::BlendPosesTogether(ChannelPoseView, ChannelCurves, ChannelAttributes, ChannelWeights, AnimationPoseData);
#else
			FAnimationRuntime::BlendPosesTogether(ChannelPoseView, ChannelCurves, ChannelWeights, Output.Pose, Output.Curve);
				
#endif

			Output.Pose.NormalizeRotations();
		}
		else
		{

			UAnimSequenceBase* PrimaryAnim = GetPrimaryAnim();

			if(PrimaryAnim)
			{
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
				FAnimationPoseData AnimationPoseData(Output);
				PrimaryAnim->GetAnimationPose(AnimationPoseData, FAnimExtractContext(BlendChannels.Last().AnimTime, true));
#else 
				PrimaryAnim->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(BlendChannels.Last().AnimTime, true));
#endif
			}
		}
	}
	else
	{
		UAnimSequenceBase* PrimaryAnim = GetPrimaryAnim();
		if (PrimaryAnim)
		{
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
			FAnimationPoseData AnimationPoseData(Output);
			PrimaryAnim->GetAnimationPose(AnimationPoseData, FAnimExtractContext(BlendChannels.Last().AnimTime, true));
#else
			PrimaryAnim->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(BlendChannels.Last().AnimTime, true));
#endif
		}
	}
}


void FAnimNode_MotionMatching::CreateTickRecordForNode(const FAnimationUpdateContext& Context, bool bLooping, float PlayRate)
{
	// Create a tick record and fill it out
	const float FinalBlendWeight = Context.GetFinalBlendWeight();

	FAnimGroupInstance* SyncGroup;

#if ENGINE_MAJOR_VERSION > 4
	const FName GroupNameToUse = ((GroupRole < EAnimGroupRole::TransitionLeader) || bHasBeenFullWeight) ? GroupName : NAME_None;
	FAnimTickRecord& TickRecord = Context.AnimInstanceProxy->CreateUninitializedTickRecordInScope(/*out*/ SyncGroup, GroupNameToUse, EAnimSyncGroupScope::Local);
#elif ENGINE_MINOR_VERSION > 25
	const FName GroupNameToUse = ((GroupRole < EAnimGroupRole::TransitionLeader) || bHasBeenFullWeight) ? GroupName : NAME_None;
	FAnimTickRecord& TickRecord = Context.AnimInstanceProxy->CreateUninitializedTickRecordInScope(/*out*/ SyncGroup, GroupNameToUse, GroupScope);
#else
	const int32 GroupIndexToUse = ((GroupRole < EAnimGroupRole::TransitionLeader) || bHasBeenFullWeight) ? GroupIndex : INDEX_NONE;
	FAnimTickRecord& TickRecord = Context.AnimInstanceProxy->CreateUninitializedTickRecord(GroupIndexToUse, /*out*/ SyncGroup);
#endif

	TickRecord.SourceAsset = MotionData;
	TickRecord.TimeAccumulator = &InternalTimeAccumulator;
	TickRecord.MarkerTickRecord = &MarkerTickRecord;
	TickRecord.PlayRateMultiplier = PlayRate;
	TickRecord.EffectiveBlendWeight = FinalBlendWeight;
	TickRecord.bLooping = true;
	TickRecord.bCanUseMarkerSync = false;
	TickRecord.BlendSpace.BlendSpacePositionX = 0.0f;
	TickRecord.BlendSpace.BlendSpacePositionY = 0.0f;
	TickRecord.BlendSpace.BlendFilter = nullptr;
	TickRecord.BlendSpace.BlendSampleDataCache = reinterpret_cast<TArray<FBlendSampleData>*>(&BlendChannels);
	TickRecord.RootMotionWeightModifier = Context.GetRootMotionWeightModifier();

	// Update the sync group if it exists
	if (SyncGroup != NULL)
	{
		SyncGroup->TestTickRecordForLeadership(GroupRole);
	}

	TRACE_ANIM_TICK_RECORD(Context, TickRecord);
}

void FAnimNode_MotionMatching::PerformLinearSearchComparison(const FAnimationUpdateContext& Context, int32 ComparePoseId, FPoseMotionData& NextPose)
{
	int32 LowestPoseId = LowestPoseId = GetLowestCostPoseId_Linear(NextPose);

	const bool SamePoseChosen = LowestPoseId == ComparePoseId;

	LowestPoseId = FMath::Clamp(LowestPoseId, 0, MotionData->Poses.Num() - 1);

	float LinearChosenPoseCost = 0.0f;
	float ActualChosenPoseCost = 0.0f;

	float LinearChosenTrajectoryCost = 0.0f;
	float ActualChosenTrajectoryCost = 0.0f;
	
	if (!SamePoseChosen)
	{
		FPoseMotionData& LinearPose = MotionData->Poses[LowestPoseId];
		FPoseMotionData& ActualPose = MotionData->Poses[ComparePoseId];

		LinearChosenTrajectoryCost = FMotionMatchingUtils::ComputeTrajectoryCost(CurrentInterpolatedPose.Trajectory, LinearPose.Trajectory,
			1.0f, 0.0f);

		ActualChosenTrajectoryCost = FMotionMatchingUtils::ComputeTrajectoryCost(CurrentInterpolatedPose.Trajectory, ActualPose.Trajectory,
			1.0f, 0.0f);

		LinearChosenPoseCost = FMotionMatchingUtils::ComputePoseCost(CurrentInterpolatedPose.JointData, LinearPose.JointData,
			1.0f, 0.0f);

		LinearChosenPoseCost = FMotionMatchingUtils::ComputePoseCost(CurrentInterpolatedPose.JointData, ActualPose.JointData,
			1.0f, 0.0f);
	}

	UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;

	const float TrajectorySearchError = FMath::Abs(ActualChosenTrajectoryCost - LinearChosenTrajectoryCost) / MMConfig->TrajectoryTimes.Num();
	const float PoseSearchError = FMath::Abs(ActualChosenPoseCost - LinearChosenPoseCost) / MMConfig->PoseBones.Num();

	const FString OverallMessage = FString::Printf(TEXT("Linear Search Error %f"), PoseSearchError + TrajectorySearchError);
	const FString PoseMessage = FString::Printf(TEXT("Linear Search Pose Error %f"), PoseSearchError);
	const FString TrajectoryMessage = FString::Printf(TEXT("Linear Search Trajectory Error %f"), TrajectorySearchError);
	Context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(OverallMessage, FColor::Black);
	Context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(PoseMessage, FColor::Red);
	Context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(TrajectoryMessage, FColor::Blue);
}

UAnimSequence* FAnimNode_MotionMatching::GetAnimAtIndex(const int32 AnimId)
{
	if(AnimId < 0 
	|| AnimId >= BlendChannels.Num())
	{
		return nullptr;
	}

	FAnimChannelState& AnimChannel = BlendChannels[AnimId];

	return MotionData->GetSourceAnimAtIndex(AnimChannel.AnimId).Sequence;
}

UAnimSequenceBase* FAnimNode_MotionMatching::GetPrimaryAnim()
{
	if (BlendChannels.Num() == 0)
	{
		return nullptr;
	}

	FAnimChannelState& CurrentChannel = BlendChannels.Last();

	switch (CurrentChannel.AnimType)
	{
		case EMotionAnimAssetType::Sequence: return MotionData->GetSourceAnimAtIndex(CurrentChannel.AnimId).Sequence;
		case EMotionAnimAssetType::Composite: return MotionData->GetSourceCompositeAtIndex(CurrentChannel.AnimId).AnimComposite;
	}

	return nullptr;
}

void FAnimNode_MotionMatching::DrawTrajectoryDebug(FAnimInstanceProxy* InAnimInstanceProxy)
{
	if(!InAnimInstanceProxy
	|| DesiredTrajectory.TrajectoryPoints.Num() == 0)
	{
		return;
	}

	UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;

	const FTransform& ActorTransform = InAnimInstanceProxy->GetActorTransform();
	const FTransform& MeshTransform = InAnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	FVector ActorLocation = MeshTransform.GetLocation();
	FVector LastPoint;

	for (int32 i = 0; i < DesiredTrajectory.TrajectoryPoints.Num(); ++i)
	{
		FColor Color = FColor::Green;
		if (MMConfig->TrajectoryTimes[i] < 0.0f)
		{
			Color = FColor(0, 128, 0);
		}

		FTrajectoryPoint& TrajPoint = DesiredTrajectory.TrajectoryPoints[i];

		FVector PointPosition = MeshTransform.TransformPosition(TrajPoint.Position);
		InAnimInstanceProxy->AnimDrawDebugSphere(PointPosition, 5.0f, 32, Color, false, -1.0f, 0.0f);

		FQuat ArrowRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(TrajPoint.RotationZ));
		FVector DrawTo = PointPosition + (ArrowRotation * ActorTransform.TransformVector(FVector::ForwardVector) * 30.0f);

		// 提高箭头
		FVector ArrowStart = PointPosition + FVector(0, 0, 30.0f);
		FVector ArrowEnd = DrawTo + FVector(0, 0, 30.0f);
		InAnimInstanceProxy->AnimDrawDebugLine(PointPosition, ArrowStart, Color, false, -1.0f, 2.0f);

		InAnimInstanceProxy->AnimDrawDebugDirectionalArrow(ArrowStart, ArrowEnd, 40.0f, Color, false, -1.0f, 2.0f);

		if(i > 0)
		{
			if (MMConfig->TrajectoryTimes[i - 1 ] < 0.0f && MMConfig->TrajectoryTimes[i] > 0.0f)
			{
				InAnimInstanceProxy->AnimDrawDebugLine(LastPoint, ActorLocation, FColor::Blue, false, -1.0f, 2.0f);
				InAnimInstanceProxy->AnimDrawDebugLine(ActorLocation, PointPosition, FColor::Blue, false, -1.0f, 2.0f);
				InAnimInstanceProxy->AnimDrawDebugSphere(ActorLocation, 5.0f, 32, FColor::Blue, false, -1.0f);
			}
			else
			{
				InAnimInstanceProxy->AnimDrawDebugLine(LastPoint, PointPosition, Color, false, -1.0f, 2.0f);
			}
		}

		LastPoint = PointPosition;
	}
}

void FAnimNode_MotionMatching::DrawChosenTrajectoryDebug(FAnimInstanceProxy* InAnimInstanceProxy)
{
	if (InAnimInstanceProxy == nullptr 
	|| CurrentChosenPoseId > MotionData->Poses.Num() - 1)
	{
		return;
	}

	TArray<FTrajectoryPoint>& CurrentTrajectory = MotionData->Poses[CurrentChosenPoseId].Trajectory;

	if (CurrentTrajectory.Num() == 0)
	{
		return;
	}

	UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;

	const FTransform& ActorTransform = InAnimInstanceProxy->GetActorTransform();
	const FTransform& MeshTransform = InAnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	FVector ActorLocation = MeshTransform.GetLocation();
	FVector LastPoint;

	for (int32 i = 0; i < CurrentTrajectory.Num(); ++i)
	{
		FColor Color = FColor::Red;
		if (MMConfig->TrajectoryTimes[i] < 0.0f)
		{
			Color = FColor(128, 0, 0);
		}

		FTrajectoryPoint& TrajPoint = CurrentTrajectory[i];

		FVector PointPosition = MeshTransform.TransformPosition(TrajPoint.Position);
		InAnimInstanceProxy->AnimDrawDebugSphere(PointPosition, 5.0f, 32, Color, false, -1.0f, 0.0f);

		FQuat ArrowRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(TrajPoint.RotationZ));
		FVector DrawTo = PointPosition + (ArrowRotation * ActorTransform.TransformVector(FVector::ForwardVector) * 30.0f);

		// 提高箭头
		FVector ArrowStart = PointPosition + FVector(0, 0, 30.0f);
		FVector ArrowEnd = DrawTo + FVector(0, 0, 30.0f);
		InAnimInstanceProxy->AnimDrawDebugLine(PointPosition, ArrowStart, Color, false, -1.0f, 2.0f);

		InAnimInstanceProxy->AnimDrawDebugDirectionalArrow(ArrowStart, ArrowEnd, 40.0f, Color, false, -1.0f, 2.0f);

		if (i > 0)
		{
			if (MMConfig->TrajectoryTimes[i - 1] < 0.0f && MMConfig->TrajectoryTimes[i] > 0.0f)
			{
				InAnimInstanceProxy->AnimDrawDebugLine(LastPoint, ActorLocation, FColor::Orange, false, -1.0f, 2.0f);
				InAnimInstanceProxy->AnimDrawDebugLine(ActorLocation, PointPosition, FColor::Orange, false, -1.0f, 2.0f);
				InAnimInstanceProxy->AnimDrawDebugSphere(ActorLocation, 5.0f, 32, FColor::Orange, false, -1.0f);
			}
			else
			{
				InAnimInstanceProxy->AnimDrawDebugLine(LastPoint, PointPosition, Color, false, -1.0f, 2.0f);
			}
		}

		LastPoint = PointPosition;
	}
}

void FAnimNode_MotionMatching::DrawChosenPoseDebug(FAnimInstanceProxy* InAnimInstanceProxy, bool bDrawVelocity)
{
	if (InAnimInstanceProxy == nullptr)
	{
		return;
	}

	FPoseMotionData& ChosenPose = CurrentInterpolatedPose;
	TArray<FJointData>& PoseJoints = ChosenPose.JointData;

	if (PoseJoints.Num() == 0)
	{
		return;
	}

	const FTransform& ActorTransform = InAnimInstanceProxy->GetActorTransform();
	const FTransform& MeshTransform = InAnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	FVector ActorLocation = MeshTransform.GetLocation();

	//Draw Body Velocity
	InAnimInstanceProxy->AnimDrawDebugSphere(ActorLocation, 5.0f, 32, FColor::Blue, false, -1.0f, 0.0f);
	InAnimInstanceProxy->AnimDrawDebugDirectionalArrow(ActorLocation, MeshTransform.TransformPosition(
		ChosenPose.LocalVelocity), 80.0f, FColor::Blue, false, -1.0f, 3.0f);

	for (int32 i = 0; i < PoseJoints.Num(); ++i)
	{
		FJointData& Joint = PoseJoints[i];

		FColor Color = FColor::Yellow;

		FVector JointPosition = MeshTransform.TransformPosition(Joint.Position);
		InAnimInstanceProxy->AnimDrawDebugSphere(JointPosition, 5.0f, 32, Color, false, -1.0f, 0.0f);

		if (bDrawVelocity)
		{
			FVector DrawTo = MeshTransform.TransformPosition(Joint.Position + (Joint.Velocity * 0.33333f));
			InAnimInstanceProxy->AnimDrawDebugDirectionalArrow(JointPosition, DrawTo, 40.0f, Color, false, -1.0f, 2.0f);
		}
	}
}

void FAnimNode_MotionMatching::DrawCandidateTrajectories(TArray<FPoseMotionData>* PoseCandidates)
{
	if (!AnimInstanceProxy
	|| !PoseCandidates)
	{
		return;
	}

	FTransform CharTransform = AnimInstanceProxy->GetActorTransform();
	CharTransform.ConcatenateRotation(FQuat::MakeFromEuler(FVector(0.0f, 0.0f, -90.0f)));

	for (FPoseMotionData& Candidate : *PoseCandidates)
	{
		DrawPoseTrajectory(AnimInstanceProxy, Candidate, CharTransform);
	}
}

void FAnimNode_MotionMatching::DrawPoseTrajectory(FAnimInstanceProxy* InAnimInstanceProxy, FPoseMotionData& Pose, FTransform& CharTransform)
{
	if (!InAnimInstanceProxy)
	{
		return;
	}

	FVector LastPoint = CharTransform.TransformPosition(Pose.Trajectory[0].Position);
	LastPoint.Z -= 87.0f;

	for (int32 i = 1; i < Pose.Trajectory.Num(); ++i)
	{
		FVector ThisPoint = CharTransform.TransformPosition(Pose.Trajectory[i].Position);
		ThisPoint.Z -= 87.0f;
		InAnimInstanceProxy->AnimDrawDebugLine(LastPoint, ThisPoint, FColor::Orange, false, 0.1f, 1.0f);
		LastPoint = ThisPoint;
	}
}

void FAnimNode_MotionMatching::DrawSearchCounts(FAnimInstanceProxy* InAnimInstanceProxy)
{
	if (!InAnimInstanceProxy)
	{
		return;
	}

	int32 MaxCount = -1;
	int32 MinCount = 100000000;
	int32 AveCount = 0;
	const int32 LatestCount = HistoricalPosesSearchCounts.Last();
	for (int32 Count : HistoricalPosesSearchCounts)
	{
		AveCount += Count;

		if (Count > MaxCount)
		{
			MaxCount = Count;
		}

		if (Count < MinCount)
		{
			MinCount = Count;
		}
	}

	AveCount /= HistoricalPosesSearchCounts.Num();

	const int32 PoseCount = MotionData->Poses.Num();

	const FString TotalMessage = FString::Printf(TEXT("Total Poses: %02d"), PoseCount);
	const FString LastMessage = FString::Printf(TEXT("Poses Searched: %02d (%f % Reduction)"), LatestCount, ((float)PoseCount - (float)LatestCount) / (float)PoseCount * 100.0f);
	const FString AveMessage = FString::Printf(TEXT("Average: %02d (%f % Reduction)"), AveCount, ((float)PoseCount - (float)AveCount) / (float)PoseCount * 100.0f);
	const FString MaxMessage = FString::Printf(TEXT("High: %02d (%f % Reduction)"), MaxCount, ((float)PoseCount - (float)MaxCount) / (float)PoseCount * 100.0f);
	const FString MinMessage = FString::Printf(TEXT("Low: %02d (%f % Reduction)\n"), MinCount, ((float)PoseCount - (float)MinCount) / (float)PoseCount * 100.0f);
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(TotalMessage, FColor::Black);
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(LastMessage, FColor::Purple);
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(AveMessage, FColor::Blue);
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(MaxMessage, FColor::Red);
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(MinMessage, FColor::Green);
}

void FAnimNode_MotionMatching::DrawAnimDebug(FAnimInstanceProxy* InAnimInstanceProxy) const
{
	if(!InAnimInstanceProxy)
	{
		return;
	}

	FPoseMotionData& CurrentPose = MotionData->Poses[FMath::Clamp(CurrentInterpolatedPose.PoseId,
			0, MotionData->Poses.Num())];

	//Print Pose Information
	FString Message = FString::Printf(TEXT("Pose Id: %02d \nPoseFavour: %f \nMirrored: "),
		CurrentPose.PoseId, CurrentPose.Favour);
	
	if(CurrentPose.bMirrored)
	{
		Message += FString(TEXT("True\n"));
	}
	else
	{
		Message += FString(TEXT("False\n"));
	}
	
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(Message, FColor::Green);
	
	//Print Animation Information
	FString AnimMessage = FString::Printf(TEXT("Anim Id: %02d \nAnimType: "), CurrentPose.AnimId);

	switch(CurrentPose.AnimType)
	{
		case EMotionAnimAssetType::Sequence: AnimMessage += FString(TEXT("Sequence \n")); break;
		case EMotionAnimAssetType::BlendSpace: AnimMessage += FString(TEXT("Blend Space \n")); break;
		case EMotionAnimAssetType::Composite: AnimMessage += FString(TEXT("Composite \n")); break;
		default: AnimMessage += FString(TEXT("Invalid \n")); break;
	}
	const FAnimChannelState& AnimChannel = BlendChannels.Last();
	AnimMessage += FString::Printf(TEXT("Anim Time: %0f \nAnimName: "), AnimChannel.AnimTime);
	
	FMotionAnimAsset* MotionAnimAsset = MotionData->GetSourceAnim(CurrentPose.AnimId, CurrentPose.AnimType);
	if(MotionAnimAsset && MotionAnimAsset->AnimAsset)
	{
		AnimMessage += MotionAnimAsset->AnimAsset->GetName();
	}
	else
	{
		AnimMessage += FString::Printf(TEXT("Invalid \n"));
	}

	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(AnimMessage, FColor::Blue);

	//Last Chosen Pose
	//Last Pose Cost
}
/*---------------------------xc-------------------------------------*/
void FAnimNode_MotionMatching::UpdateDebugInfo(FAnimInstanceProxy* InAnimInstanceProxy)
{
	/*Get Debug Info*/
	const FPoseMotionData& CurrentPose = MotionData->Poses[FMath::Clamp(CurrentInterpolatedPose.PoseId,
		0, MotionData->Poses.Num())];
	const FAnimChannelState& AnimChannel = BlendChannels.Last();
	const FMotionAnimAsset* MotionAnimAsset = MotionData->GetSourceAnim(CurrentPose.AnimId, CurrentPose.AnimType);

	CustomDebugData->CurrentPose = CurrentPose;
	CustomDebugData->PoseId = CurrentPose.PoseId;
	CustomDebugData->AnimId = CurrentPose.AnimId;
	CustomDebugData->AnimType = CurrentPose.AnimType;
	CustomDebugData->Favour = CurrentPose.Favour;
	CustomDebugData->bMirrored = CurrentPose.bMirrored;

	CustomDebugData->AnimTime = AnimChannel.AnimTime;
	if (MotionAnimAsset && MotionAnimAsset->AnimAsset)
	{
		CustomDebugData->AnimName = MotionAnimAsset->AnimAsset->GetName();
	}
	else
	{
		CustomDebugData->AnimName = FString::Printf(TEXT("Invalid \n"));
	}
	/*Get Optimisation Info*/
	
	int32 MaxCount = -1;
	int32 MinCount = 100000000;
	int32 AveCount = 0;
	for (int32 Count : CustomDebugData->PoseSearchCountsForDebug)
	{
		AveCount += Count;
		if (Count > MaxCount)
		{
			MaxCount = Count;
		}
		if (Count < MinCount)
		{
			MinCount = Count;
		}
	}
	AveCount /= CustomDebugData->PoseSearchCountsForDebug.Num();

	CustomDebugData->TotalPoses = MotionData->Poses.Num();
	CustomDebugData->LatestCount = CustomDebugData->PoseSearchCountsForDebug.Last();
	CustomDebugData->AveCount = AveCount;
	CustomDebugData->MaxCount = MaxCount;
	CustomDebugData->MinCount = MinCount;

	/*Draw Debug*/
	if (CustomDebugData->PoseDebugLevel > 0)
	{
		DrawChosenPoseDebug(InAnimInstanceProxy, CustomDebugData->PoseDebugLevel > 1);
	}

	if (CustomDebugData->TrajDebugLevel > 0)
	{
		if (CustomDebugData->TrajDebugLevel == 2)
		{
			//Draw chosen trajectory
			DrawChosenTrajectoryDebug(InAnimInstanceProxy);
		}

		//Draw Input trajectory
		DrawTrajectoryDebug(InAnimInstanceProxy);
	}
}

void FAnimNode_MotionMatching::UpdateLowestPoses(FPoseMotionData& Pose, TArray<FPoseCostInfo>& LowestPoses, float Cost, float MomentumCost, float RotationalCost, float TrajectoryCost, float PoseCost)
{
	const FMotionAnimAsset* MotionAnimAsset = MotionData->GetSourceAnim(Pose.AnimId, Pose.AnimType);
	FString AnimName = FString("Invalid");
	if (MotionAnimAsset && MotionAnimAsset->AnimAsset)
	{
		AnimName = MotionAnimAsset->AnimAsset->GetName();
	}
	FPoseCostInfo CurrentPoseCostInfo(Pose.PoseId, AnimName, Pose.AnimId, Pose.Favour, 1, Cost, MomentumCost, RotationalCost, TrajectoryCost, PoseCost);

	if (LowestPoses.Num() < 10) {
		LowestPoses.Emplace(CurrentPoseCostInfo);
		LowestPoses.Sort([](const FPoseCostInfo& Info1, const FPoseCostInfo& Info2) {
			return Info1.TotalCost < Info2.TotalCost;
			});
	}
	else {
		LowestPoses.RemoveAt(9);
		LowestPoses.Emplace(CurrentPoseCostInfo);
		LowestPoses.Sort([](const FPoseCostInfo& Info1, const FPoseCostInfo& Info2) {
			return Info1.TotalCost < Info2.TotalCost;
			});
	}
}
