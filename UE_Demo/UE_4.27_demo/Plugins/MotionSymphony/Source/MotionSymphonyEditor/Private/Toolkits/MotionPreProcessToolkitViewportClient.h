// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "InputCoreTypes.h"
#include "PreviewScene.h"
#include "CustomAssets/MotionDataAsset.h"
#include "Data/Trajectory.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "MotionPreProcessToolkit.h"
#include "AssetViewerSettings.h"

class USphereReflectionCaptureComponent;
class UMaterialInstanceConstant;
class UPostProcessComponent;
class UAssetViewerSettings;

class FMotionPreProcessToolkitViewportClientRoot : public FEditorViewportClient
{
public:
	explicit FMotionPreProcessToolkitViewportClientRoot(const TWeakPtr<class SEditorViewport>& InEditorViewportWidget = nullptr);
	~FMotionPreProcessToolkitViewportClientRoot();
};

class FMotionPreProcessToolkitViewportClient : public FMotionPreProcessToolkitViewportClientRoot
{
protected:
	//FPaperEditorViewportClient interface
	virtual FBox GetDesiredFocusBounds() const;
	//End of FPaperEditorViewportClient interface

	USkyLightComponent* SkyLightComponent;
	UStaticMeshComponent* SkyComponent;
	USphereReflectionCaptureComponent* SphereReflectionComponent;
	UMaterialInstanceConstant* InstancedSkyMaterial;
	UPostProcessComponent* PostProcessComponent;
	UStaticMeshComponent* FloorMeshComponent;
	UAssetViewerSettings* DefaultSettings;

private:
	TWeakPtr<class FMotionPreProcessToolkit> MotionPreProcessToolkitPtr;

	FTrajectory CurrentTrajectory;
	FPreviewScene OwnedPreviewScene;

	TAttribute<class UMotionDataAsset*> MotionData;

	TWeakObjectPtr<UDebugSkelMeshComponent> AnimatedRenderComponent;
	TArray<TWeakObjectPtr<UDebugSkelMeshComponent>> InteractionAnimatedRenderComponents;
	UMotionMatchConfig* CurrentMotionConfig;

	bool bShowPivot;
	bool bShowMatchBones;
	bool bShowTrajectory;
	bool bShowPose;
	bool bShowOptimizationDebug;

public:
	FMotionPreProcessToolkitViewportClient(const TAttribute<class UMotionDataAsset*>& InMotionData, TWeakPtr<class FMotionPreProcessToolkit> InMotionPreProcessToolkitPtr);

	// FViewportClient interface
	virtual void Draw(const FSceneView* SceneView, FPrimitiveDrawInterface* DrawInterface) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& SceneView, FCanvas& Canvas) override;
	virtual void Tick(float DeltaSeconds) override;
	// End of FViewportClinet interface

	// FEditorViewportClient interface
	virtual bool InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad) override;
	virtual FLinearColor GetBackgroundColor() const override;
	// End of FEditorViewportClient interface

	void ToggleShowPivot();
	bool IsShowPivotChecked() const;
	void ToggleShowMatchBones();
	bool IsShowMatchBonesChecked() const;
	void ToggleShowTrajectory();
	bool IsShowTrajectoryChecked() const;
	void ToggleShowPose();
	bool IsShowPoseChecked() const;
	void ToggleShowOptimizationDebug();
	bool IsShowOptimizationDebugChecked() const;

	void DrawMatchBones(FPrimitiveDrawInterface* DrawInterface, const UWorld* World) const;
	void DrawCurrentTrajectory(FPrimitiveDrawInterface* DrawInterface) const;
	void DrawCurrentPose(FPrimitiveDrawInterface* DrawInterface, const UWorld* World) const;
	void DrawOptimisationDebug(FPrimitiveDrawInterface* DrawInterface, const UWorld* World) const;
	void SetCurrentTrajectory(const FTrajectory InTrajectory);

	UDebugSkelMeshComponent* GetPreviewComponent() const;
	TArray<UDebugSkelMeshComponent*> GetInteractionPreviewComponents() const;

	virtual void RequestFocusOnSelection(bool bInstant);

	void SetupAnimatedRenderComponent();
	void SetupInteractionAnimatedRenderComponents(TArray<USkeleton*> InteractionSkeletons);
private:
	void SetupSkylight(FPreviewSceneProfile& Profile);
	void SetupSkySphere(FPreviewSceneProfile& Profile);
	void SetupPostProcess(FPreviewSceneProfile& Profile);
	void SetupFloor();
};