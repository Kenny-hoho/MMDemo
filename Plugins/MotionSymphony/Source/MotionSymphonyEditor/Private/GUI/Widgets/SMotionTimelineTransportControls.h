// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "Input/Reply.h"
#include "ITransportControl.h"

class UDebugSkelMeshComponent;
class UAnimSequenceBase;
class UAnimSingleNodeInstance;

class SMotionTimelineTransportControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMotionTimelineTransportControls) {}

	SLATE_END_ARGS()

private:
	UDebugSkelMeshComponent* DebugSkelMesh;
	UAnimationAsset* AnimAsset;
	EMotionAnimAssetType AnimType;
	TArray<UDebugSkelMeshComponent*> InteractionDebugSkelMeshs;
	float FrameRate;
	float PlayLength;

public:
	void Construct(const FArguments& InArgs, UDebugSkelMeshComponent* InDebugMesh, 
		UAnimationAsset* InAnimAsset, EMotionAnimAssetType InAnimType,TArray<UDebugSkelMeshComponent*> InteractionDebugSkelMeshCoponents);

private:
	UAnimSingleNodeInstance* GetPreviewInstance() const;
	TArray<UAnimSingleNodeInstance*> GetInteractionPreviewInstances() const;
	//TSharedRef<IPersonaPreviewScene> GetPreviewScene() const { return WeakPreviewScene.Pin().ToSharedRef(); }

	FReply OnClick_Forward_Step();
	FReply OnClick_Forward_End();
	FReply OnClick_Backward_Step();
	FReply OnClick_Backward_End();
	FReply OnClick_Forward();
	FReply OnClick_Backward();
	FReply OnClick_ToggleLoop();
	FReply OnClick_Record();

	bool IsLoopStatusOn() const;

	EPlaybackMode::Type GetPlaybackMode() const;

	bool IsRecording() const;
};