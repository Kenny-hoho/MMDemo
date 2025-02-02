// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNode_MotionRecorder.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "CustomAssets/MirroringProfile.h"
#include "Data/AnimMirroringData.h"
#include "Data/JointData.h"
#include "AnimNode_PoseMatchBase.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FPoseMatchData
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	int32 PoseId;

	UPROPERTY()
	int32 AnimId;

	UPROPERTY()
	bool bMirror;

	UPROPERTY()
	float Time;

	UPROPERTY()
	FVector LocalVelocity;

	UPROPERTY()
	TArray<FJointData> BoneData;

public:
	FPoseMatchData();
	FPoseMatchData(int32 InPoseId, int32 InAnimId, float InTime, FVector& InLocalVelocity, bool bMirror);
};

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FMatchBone
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, Category = PoseCalibration)
	FBoneReference Bone;

	UPROPERTY(EditAnywhere, Category = PoseCalibration)
	float PositionWeight;

	UPROPERTY(EditAnywhere, Category = PoseCalibration)
	float VelocityWeight;

public:
	FMatchBone();
};

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FAnimNode_PoseMatchBase : public FAnimNode_SequencePlayer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PoseMatching, meta = (ClampMin = 0.01f))
	float PoseInterval;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PoseMatching, meta = (ClampMin = 0.01f))
	float PosesEndTime;

	UPROPERTY(EditAnywhere, Category = PoseCalibration, meta = (ClampMin = 0.0f))
	float BodyVelocityWeight;

	UPROPERTY(EditAnywhere, Category = PoseCalibration)
	TArray<FMatchBone> PoseConfig;

	UPROPERTY(EditAnywhere, Category = Mirroring)
	bool bEnableMirroring;

	UPROPERTY(EditAnywhere, Category = Mirroring)
	UMirroringProfile* MirroringProfile;

protected:
	//bool bPreProcessed;
	bool bInitialized;
	bool bInitPoseSearch;

	//Baked poses
	UPROPERTY()
	TArray<FPoseMatchData> Poses;

	//Pose Data extracted from Motion Recorder
	FVector CurrentLocalVelocity;
	TArray<FJointData> CurrentPose;

	//The chosen animation data
	FPoseMatchData* MatchPose;

	FAnimInstanceProxy* AnimInstanceProxy;

	//For Mirroring
	FAnimMirroringData MirroringData;

private:
	TArray<int32> PoseBoneRemap;

public:
	FAnimNode_PoseMatchBase();

#if WITH_EDITOR
	virtual void PreProcess();
#endif

protected:
#if WITH_EDITOR
	virtual void PreProcessAnimation(UAnimSequence* Anim, int32 AnimIndex, bool bMirror = false);
#endif
	virtual void FindMatchPose(const FAnimationUpdateContext& Context); 
	virtual UAnimSequenceBase*	FindActiveAnim();
	void ComputeCurrentPose(const FCachedMotionPose& MotionPose);
	int32 GetMinimaCostPoseId();
	int32 GetMinimaCostPoseId(float& OutCost, int32 StartPose, int32 EndPose);

	void InitializePoseBoneRemap(const FAnimationUpdateContext& Context);

	// FAnimNode_Base interface
	virtual bool NeedsOnInitializeAnimInstance() const override;
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InAnimInstanceProxy, const UAnimInstance* InAnimInstance) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	// End of FAnimNode_Base interface
};