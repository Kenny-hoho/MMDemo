// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Data/PoseMotionData.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "CustomAssets/MotionDataAsset.h"
#include "MotionMatchDebugInfo.generated.h"

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
	float BodyMomentumCost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BodyRotationCost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TrajectoryCost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float PoseCost;

	FPoseCostInfo();
	FPoseCostInfo(
		int32 pose_id, FString anim_name, int32 anim_id, float favour, float current_pose_favour,
		float total_cost, float momentum_cost, float rotation_cost,
		float trajectory_cost, float pose_cost
	):PoseId(pose_id),
	AnimName(anim_name),
	AnimId(anim_id),
	Favour(favour),
	CurrentPoseFavour(current_pose_favour),
	TotalCost(total_cost),
	BodyMomentumCost(momentum_cost),
	BodyRotationCost(rotation_cost),
	TrajectoryCost(trajectory_cost),
	PoseCost(pose_cost) {};
};

UCLASS()
class MOTIONSYMPHONY_API AMotionMatchDebugInfo : public AInfo
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
	AMotionMatchDebugInfo();
};
