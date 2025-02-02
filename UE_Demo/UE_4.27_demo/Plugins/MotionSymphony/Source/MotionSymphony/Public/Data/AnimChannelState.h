// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PoseMotionData.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "MotionSymphony.h"
#include "Animation/AnimationAsset.h"
#include "AnimChannelState.generated.h"

/** A data structure for tracking animation channels within a motion matching animation stack. 
Every time a new pose is picked from the animation database, a channel is created for managing
its state. */
USTRUCT(BlueprintType)
struct MOTIONSYMPHONY_API FAnimChannelState
{
	GENERATED_USTRUCT_BODY()

public:
	/** The current weight of the animation in this channel */
	UPROPERTY()
	float Weight;

	/** The highest weight that this channel has ever been since it's creation. */
	UPROPERTY()
	float HighestWeight;
	
	/** Id of the animation used for this channel */
	UPROPERTY()
	int32 AnimId;

	/** Type of the animation used for this channel */
	UPROPERTY()
	EMotionAnimAssetType AnimType;

	/** Id of the pose that started this channel*/
	UPROPERTY()
	int32 StartPoseId;

	/** Start time within the animation that this channel started*/
	UPROPERTY()
	float StartTime;

	/** Position within the blend space that this channel uses (if it is a blend space channel) */
	UPROPERTY()
	FVector2D BlendSpacePosition;

	/** How long the channel has been alive */
	UPROPERTY()
	float Age;

	/** How long the channel has been blending out (i.e. time since it was no longer the chosen channel) */
	UPROPERTY()
	float DecayAge;

	/** Current time of the animation in this channel */
	UPROPERTY()
	float AnimTime;

	/** The current status of this channel. Whether it is chosen, dominant, decaying or active. */
	UPROPERTY()
	EBlendStatus BlendStatus = EBlendStatus::Inactive; //0 == Inactive, 1 == Decay, 2 == Chosen, 3 == Dominant

	/** Does the animation in this channel loop? */
	UPROPERTY()
	bool bLoop;

	/** Is the animation for this channel mirrored */
	UPROPERTY()
	bool bMirrored;

	/** The length of the animation in this channel */
	UPROPERTY()
	float AnimLength;

	/** Blend sample data cache used for blend spaces*/
	TArray<FBlendSampleData> BlendSampleDataCache;

public:
	float Update(const float DeltaTime, const float BlendTime, const bool bCurrent);

	FAnimChannelState();
	FAnimChannelState(const FPoseMotionData& InPose, EBlendStatus InBlendStatus, 
		float InWeight, float InAnimLength, bool bInLoop = false, 
		bool bInMirrored = false, float InTimeOffset=0.0f, float InPoseOffset=0.0f);
};