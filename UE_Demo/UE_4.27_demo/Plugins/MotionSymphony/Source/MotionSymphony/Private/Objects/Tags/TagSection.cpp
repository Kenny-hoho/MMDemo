// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "Tags/TagSection.h"

UTagSection::UTagSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UTagSection::PreProcessTag(FMotionAnimAsset& OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Received_PreProcessTag(OutMotionAnim, OutMotionData, StartTime, EndTime);
}

void UTagSection::PreProcessPose(FPoseMotionData& OutPose, FMotionAnimAsset& OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Received_PreProcessPose(OutPose, OutMotionAnim, OutMotionData, StartTime, EndTime);
}