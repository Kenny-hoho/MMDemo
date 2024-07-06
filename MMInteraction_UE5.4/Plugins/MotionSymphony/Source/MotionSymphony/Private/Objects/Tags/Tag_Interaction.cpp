// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/Tags/Tag_Interaction.h"
#include "MotionAnimObject.h"
#include "Kismet/KismetMathLibrary.h"


void UTag_Interaction::PreProcessTag(TObjectPtr<UMotionAnimObject> OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Super::PreProcessTag(OutMotionAnim, OutMotionData, StartTime, EndTime);
}

void UTag_Interaction::PreProcessPose(FPoseMotionData& OutPose, TObjectPtr<UMotionAnimObject> OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Super::PreProcessPose(OutPose, OutMotionAnim, OutMotionData, StartTime, EndTime);

	// Get Root Transform
	FTransform CurrentRootTransform = FTransform::Identity;
	UMotionSequenceObject* MotionSequence = Cast<UMotionSequenceObject>(OutMotionAnim);
	MotionSequence->Sequence->GetBoneTransform(CurrentRootTransform, FSkeletonPoseBoneIndex(0), OutPose.Time, false);

	// 转换为相对于当前根骨骼的相对坐标
	// FVector InteractPointInitialLocation_CS = UKismetMathLibrary::InverseTransformLocation(CurrentRootTransform, InteractPointInitialLocation);
	FVector InteractPointInitialLocation_CS = InteractPointInitialLocation - CurrentRootTransform.GetLocation();

	FPoseMatrix& LookupPoseMatrix = OutMotionData->LookupPoseMatrix;
	LookupPoseMatrix.PoseArray[OutPose.PoseId * LookupPoseMatrix.AtomCount + 1] = InteractPointInitialLocation_CS.X;
	LookupPoseMatrix.PoseArray[OutPose.PoseId * LookupPoseMatrix.AtomCount + 2] = InteractPointInitialLocation_CS.Y;
	LookupPoseMatrix.PoseArray[OutPose.PoseId * LookupPoseMatrix.AtomCount + 3] = InteractPointInitialLocation_CS.Z;
}

void UTag_Interaction::CopyTagData(UTagSection* CopyTag)
{
}
