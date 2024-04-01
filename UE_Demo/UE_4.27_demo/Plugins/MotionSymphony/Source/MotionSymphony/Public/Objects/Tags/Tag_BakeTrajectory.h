// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Objects/Tags/TagSection.h"
#include "Tag_BakeTrajectory.generated.h"

/**
 * 
 */
UCLASS()
class MOTIONSYMPHONY_API UTag_BakeTrajectory : public UTagSection
{
	GENERATED_BODY()

private:
	virtual void PreProcessPose(FPoseMotionData& OutPose, FMotionAnimAsset& OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime) override;
	void SaveTrajectory(FPoseMotionData& OutPose, TSharedRef<TJsonWriter<>> JsonWriter);
};
