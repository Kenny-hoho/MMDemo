//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MatchFeatureBase.h"
#include "MatchFeature_BoneAxis.generated.h"

struct FPoseMatrix;

/** Motion matching feature which stores and matches bone velocity relative to the character root */
UCLASS(BlueprintType)
class MOTIONSYMPHONY_API UMatchFeature_BoneAxis : public UMatchFeatureBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Match Feature|Bone Axis")
	TEnumAsByte<EAxis::Type> Axis;
	
	UPROPERTY(EditAnywhere, Category = "Match Feature|Bone Axis")
	FBoneReference BoneReference;

public:
	virtual bool IsSetupValid() const override;
	
	virtual bool IsMotionSnapshotCompatible() const override;
	
	virtual int32 Size() const override;
	virtual void EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
	                                const float Time, const float PoseInterval, const bool bMirror,
	                                UMirrorDataTable* MirrorDataTable, ::TObjectPtr<UMotionAnimObject> InMotionObject) override;
	virtual void EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite,
	                                const float Time, const float PoseInterval, const bool bMirror,
	                                UMirrorDataTable* MirrorDataTable, TObjectPtr<UMotionAnimObject> InAnimObject) override;
	virtual void EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace,
	                                const float Time, const float PoseInterval, const bool bMirror,
	                                UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr<UMotionAnimObject> InAnimObject) override;

	virtual void CacheMotionBones(const FAnimInstanceProxy* InAnimInstanceProxy) override;
	virtual void ExtractRuntime(FCSPose<FCompactPose>& CSPose, float* ResultLocation, float* FeatureCacheLocation, FAnimInstanceProxy*
	                            AnimInstanceProxy, float DeltaTime) override;
	
	virtual bool CanBeQualityFeature() const override;

	virtual FName GetFeatureName() const override;

#if WITH_EDITOR	
	virtual void DrawPoseDebugEditor(UMotionDataAsset* MotionData, UDebugSkelMeshComponent* DebugSkeletalMesh,
		const int32 PreviewIndex, const int32 FeatureOffset, const UWorld* World, FPrimitiveDrawInterface* DrawInterface) override;
	virtual void DrawDebugDesiredRuntime(FAnimInstanceProxy* AnimInstanceProxy, FMotionMatchingInputData& InputData,
		const int32 FeatureOffset, UMotionMatchConfig* MMConfig) override;
	virtual void DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy, TObjectPtr<const UMotionDataAsset> MotionData,
	                                     const TArray<float>& CurrentPoseArray, const int32 FeatureOffset) override;
#endif
};