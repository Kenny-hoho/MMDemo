// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Data/PoseMotionData.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "Objects/Assets/MotionDataAsset.h"
#include "Containers/Map.h"
#include "MotionMatchingDebugInfo.generated.h"

/* Gather Pose Cost Info */
USTRUCT(BlueprintType)
struct FPoseCostInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 PoseId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString AnimName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AnimId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Favour;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CurrentPoseFavour;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TotalCost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, float> FeatureCostMap;

	FPoseCostInfo();
	FPoseCostInfo(
		int32 pose_id, FString anim_name, int32 anim_id, float favour, float current_pose_favour,
		float total_cost, TMap<FName, float>& FeatureCostMap
	) :PoseId(pose_id),
		AnimName(anim_name),
		AnimId(anim_id),
		Favour(favour),
		CurrentPoseFavour(current_pose_favour),
		TotalCost(total_cost),
		FeatureCostMap(FeatureCostMap){};
};

UCLASS()
class MOTIONSYMPHONY_API AMotionMatchingDebugInfo : public AInfo
{
	GENERATED_BODY()
	
public:
	/*AnimInfo to show on HUD*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float timepassed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPoseMotionData CurrentPose;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 PoseId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Favour;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bMirrored;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AnimId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EMotionAnimAssetType AnimType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float AnimTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString AnimName;


	/* 记录Cost前十小的姿势id */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FPoseCostInfo> LowestCostCandidates;


	/* Show Search Optimisation*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<int32> PoseSearchCountsForDebug;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 TotalPoses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SearchCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 LatestCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AveCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MaxCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinCount;


	/*Config to control how to draw debug*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PoseDebugLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TrajDebugLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SearchDebugLevel;

public:
	AMotionMatchingDebugInfo();
};
