// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Objects/Tags/TagSection.h"
#include "Tag_Interaction.generated.h"

/**
 * 
 */
UCLASS()
class MOTIONSYMPHONY_API UTag_Interaction : public UTagSection
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Tag")
	FBoneReference BoneReference;

	UPROPERTY(EditAnywhere, Category = "Tag")
	float InteractAngle;

	FVector InteractPointInitialLocation;

public:
	virtual void PreProcessTag(TObjectPtr<UMotionAnimObject> OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime) override;
	virtual void PreProcessPose(FPoseMotionData& OutPose, TObjectPtr<UMotionAnimObject> OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime) override;

	virtual void CopyTagData(UTagSection* CopyTag) override;
};
