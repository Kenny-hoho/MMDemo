// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/MatchFeatures/MatchFeature_Interaction.h"
#include "MMPreProcessUtils.h"
#include "MotionAnimAsset.h"
#include "MotionDataAsset.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/MirrorDataTable.h"
#include "InteractionGPComponent.h"

#if WITH_EDITOR
#include "Animation/DebugSkelMeshComponent.h"
#include "MotionSymphonySettings.h"
#endif

bool UMatchFeature_Interaction::IsMotionSnapshotCompatible() const
{
	return true;
}

int32 UMatchFeature_Interaction::Size() const
{
	return 3;
}

void UMatchFeature_Interaction::EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence, const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<UMotionAnimObject> InMotionObject)
{
	if (!InSequence) {
		*ResultLocation = 0.0f;
		ResultLocation++;
		*ResultLocation = 0.0f;
		ResultLocation++;
		*ResultLocation = 0.0f;
		return;
	}

	// Extract Interaction Point Location

	FTransform JointTransform_CS = FTransform::Identity;
	TArray<FName> BonesToRoot;
	FName BoneName = BoneReference.BoneName;

	if (bMirror && MirrorDataTable)
	{
		BoneName = FMMPreProcessUtils::FindMirrorBoneName(InSequence->GetSkeleton(), MirrorDataTable, BoneName);
	}

	FMMPreProcessUtils::FindBonePathToRoot(InSequence, BoneName, BonesToRoot);
	if (BonesToRoot.Num() > 0)
	{
		BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
	}

	FMMPreProcessUtils::GetJointTransform_RootRelative(JointTransform_CS, InSequence, BonesToRoot, Time);
	const FVector BoneLocation = JointTransform_CS.GetLocation();

	*ResultLocation = bMirror ? -BoneLocation.X : BoneLocation.X;
	++ResultLocation;
	*ResultLocation = BoneLocation.Y;
	++ResultLocation;
	*ResultLocation = BoneLocation.Z;
}

void UMatchFeature_Interaction::EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite, const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<UMotionAnimObject> InAnimObject)
{
	if (!InComposite)
	{
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		return;
	}

	FTransform JointTransform_CS = FTransform::Identity;
	TArray<FName> BonesToRoot;
	FName BoneName = BoneReference.BoneName;

	if (bMirror && MirrorDataTable)
	{
		BoneName = FMMPreProcessUtils::FindMirrorBoneName(InComposite->GetSkeleton(), MirrorDataTable, BoneName);
	}

	FMMPreProcessUtils::FindBonePathToRoot(InComposite, BoneName, BonesToRoot);
	if (BonesToRoot.Num() > 0)
	{
		BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
	}

	FMMPreProcessUtils::GetJointTransform_RootRelative(JointTransform_CS, InComposite, BonesToRoot, Time);
	const FVector BoneLocation = JointTransform_CS.GetLocation();

	*ResultLocation = bMirror ? -BoneLocation.X : BoneLocation.X;
	++ResultLocation;
	*ResultLocation = BoneLocation.Y;
	++ResultLocation;
	*ResultLocation = BoneLocation.Z;
}

void UMatchFeature_Interaction::EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace, const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr<UMotionAnimObject> InAnimObject)
{
	if (!InBlendSpace)
	{
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		return;
	}

	TArray<FBlendSample> BlendSamples = InBlendSpace->GetBlendSamples();
	TArray<FBlendSampleData> SampleDataList;
	int32 CachedTriangulationIndex = -1;
	InBlendSpace->GetSamplesFromBlendInput(FVector(BlendSpacePosition.X, BlendSpacePosition.Y, 0.0f),
		SampleDataList, CachedTriangulationIndex, false);

	FTransform JointTransform_CS = FTransform::Identity;
	TArray<FName> BonesToRoot;
	FName BoneName = BoneReference.BoneName;

	if (bMirror && MirrorDataTable)
	{
		BoneName = FMMPreProcessUtils::FindMirrorBoneName(InBlendSpace->GetSkeleton(), MirrorDataTable, BoneName);
	}

	FMMPreProcessUtils::FindBonePathToRoot(BlendSamples[0].Animation.Get(), BoneName, BonesToRoot);
	if (BonesToRoot.Num() > 0)
	{
		BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
	}

	FMMPreProcessUtils::GetJointTransform_RootRelative(JointTransform_CS, SampleDataList, BonesToRoot, Time);
	const FVector BoneLocation = JointTransform_CS.GetLocation();

	*ResultLocation = bMirror ? -BoneLocation.X : BoneLocation.X;
	++ResultLocation;
	*ResultLocation = BoneLocation.Y;
	++ResultLocation;
	*ResultLocation = BoneLocation.Z;
}

void UMatchFeature_Interaction::CacheMotionBones(const FAnimInstanceProxy* InAnimInstanceProxy)
{
	BoneReference.Initialize(InAnimInstanceProxy->GetRequiredBones());
}

void UMatchFeature_Interaction::ExtractRuntime(FCSPose<FCompactPose>& CSPose, float* ResultLocation, float* FeatureCacheLocation, FAnimInstanceProxy* AnimInstanceProxy, float DeltaTime)
{
	if (BoneReference.CachedCompactPoseIndex == -1)
	{
		return;
	}

	const FVector BoneLocation = CSPose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneReference.CachedCompactPoseIndex)).GetLocation();

	*ResultLocation = BoneLocation.X;
	++ResultLocation;
	*ResultLocation = BoneLocation.Y;
	++ResultLocation;
	*ResultLocation = BoneLocation.Z;
}

void UMatchFeature_Interaction::CalculateDistanceSqrToMeanArrayForStandardDeviations(TArray<float>& OutDistToMeanSqrArray, const TArray<float>& InMeanArray, const TArray<float>& InPoseArray, const int32 FeatureOffset, const int32 PoseStartIndex) const
{
	const FVector Location
	{
		InPoseArray[PoseStartIndex + FeatureOffset],
		InPoseArray[PoseStartIndex + FeatureOffset + 1],
		InPoseArray[PoseStartIndex + FeatureOffset + 2]
	};

	const FVector MeanLocation
	{
		InMeanArray[FeatureOffset],
		InMeanArray[FeatureOffset + 1],
		InMeanArray[FeatureOffset + 2]
	};

	const float DistanceToMean = FVector::DistSquared(Location, MeanLocation);

	OutDistToMeanSqrArray[FeatureOffset] += DistanceToMean;
	OutDistToMeanSqrArray[FeatureOffset + 1] += DistanceToMean;
	OutDistToMeanSqrArray[FeatureOffset + 2] += DistanceToMean;
}

bool UMatchFeature_Interaction::CanBeQualityFeature() const
{
	return true;
}

void UMatchFeature_Interaction::SourceInputData(TArray<float>& OutFeatureArray, const int32 FeatureOffset, AActor* InActor)
{
	if (!InActor) {
		UMatchFeatureBase::SourceInputData(OutFeatureArray, FeatureOffset, nullptr);
		return;
	}

	if (UInteractionGPComponent* InteractComp = InActor->GetComponentByClass<UInteractionGPComponent>()) {
		const FVector InteractPointLocation = InteractComp->GetCurrentInteractPointLocation();

		if (OutFeatureArray.Num() > FeatureOffset + 2) {
			OutFeatureArray[FeatureOffset] = InteractPointLocation.X;
			OutFeatureArray[FeatureOffset + 1] = InteractPointLocation.Y;
			OutFeatureArray[FeatureOffset + 2] = InteractPointLocation.Z;
		}
	}
}

FName UMatchFeature_Interaction::GetFeatureName() const
{
	return FName("Interact");
}

void UMatchFeature_Interaction::DrawPoseDebugEditor(UMotionDataAsset* MotionData, UDebugSkelMeshComponent* DebugSkeletalMesh, const int32 PreviewIndex, const int32 FeatureOffset, const UWorld* World, FPrimitiveDrawInterface* DrawInterface)
{
	if (!MotionData || !DebugSkeletalMesh || !World)
	{
		return;
	}

	TArray<float>& PoseArray = MotionData->LookupPoseMatrix.PoseArray;
	const int32 StartIndex = PreviewIndex * MotionData->LookupPoseMatrix.AtomCount + FeatureOffset;

	if (PoseArray.Num() < StartIndex + Size())
	{
		return;
	}

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();

	const FTransform PreviewTransform = DebugSkeletalMesh->GetComponentTransform();
	const FVector BonePos = PreviewTransform.TransformPosition(FVector(PoseArray[StartIndex],
		PoseArray[StartIndex + 1], PoseArray[StartIndex + 2]));

	DrawDebugSphere(World, BonePos, 8.0f, 8, FColor::Red, true, -1.0f, -1);
}

void UMatchFeature_Interaction::DrawDebugDesiredRuntime(FAnimInstanceProxy* AnimInstanceProxy, FMotionMatchingInputData& InputData, const int32 FeatureOffset, UMotionMatchConfig* MMConfig)
{
}

void UMatchFeature_Interaction::DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy, TObjectPtr<const UMotionDataAsset> MotionData, const TArray<float>& CurrentPoseArray, const int32 FeatureOffset)
{
	if (!MotionData || !AnimInstanceProxy)
	{
		return;
	}

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();
	const FColor DebugColor = Settings->DebugColor_Joint;

	const FTransform PreviewTransform = AnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	const FVector BonePos = PreviewTransform.TransformPosition(FVector(CurrentPoseArray[FeatureOffset],
		CurrentPoseArray[FeatureOffset + 1], CurrentPoseArray[FeatureOffset + 2]));

	AnimInstanceProxy->AnimDrawDebugSphere(BonePos, 8.0f, 8, FColor::Red, false, -1, 0, SDPG_Foreground);
}
