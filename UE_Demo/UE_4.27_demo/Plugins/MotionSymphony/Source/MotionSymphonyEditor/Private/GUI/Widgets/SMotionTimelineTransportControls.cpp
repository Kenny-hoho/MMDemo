// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "SMotionTimelineTransportControls.h"
#include "EditorWidgetsModule.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimComposite.h"
#include "Animation/BlendSpace.h"
#include "AnimPreviewInstance.h"
#include "Modules/ModuleManager.h"

void SMotionTimelineTransportControls::Construct(const FArguments& InArgs, UDebugSkelMeshComponent* InDebugMesh,
	UAnimationAsset* InAnimAsset, EMotionAnimAssetType InAnimType, TArray<UDebugSkelMeshComponent*> InteractionDebugSkelMeshCoponents)
{
	DebugSkelMesh = InDebugMesh;
	AnimAsset = InAnimAsset;
	AnimType = InAnimType;
	InteractionDebugSkelMeshs = InteractionDebugSkelMeshCoponents;

	FrameRate = 30.0f;
	PlayLength = 0.0f;
	switch (AnimType)
	{
		case EMotionAnimAssetType::Sequence:
		{
			UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimAsset);
			PlayLength = AnimSequence ? AnimSequence->GetPlayLength() : 0.0f;
#if ENGINE_MAJOR_VERSION < 5
			FrameRate = AnimSequence ? AnimSequence->GetFrameRate() : 30.0f;
#else
			FrameRate = AnimSequence ? AnimSequence->GetSamplingFrameRate().AsDecimal() : 30.0f;
#endif

		} break;
		case EMotionAnimAssetType::BlendSpace:
		{
			UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(AnimAsset);

			if (BlendSpace)
			{
				PlayLength = BlendSpace->AnimLength;

				TArray<UAnimationAsset*> BSSequences;
				BlendSpace->GetAllAnimationSequencesReferred(BSSequences, false);

				if (BSSequences.Num() > 0)
				{
					UAnimSequence* AnimSequence = Cast<UAnimSequence>(BSSequences[0]);
#if ENGINE_MAJOR_VERSION < 5
					FrameRate = AnimSequence ? AnimSequence->GetFrameRate() : FrameRate;
#else
					FrameRate = AnimSequence ? AnimSequence->GetSamplingFrameRate().AsDecimal() : FrameRate;
#endif
				}
			}

		} break;
		case EMotionAnimAssetType::Composite:
		{
			UAnimComposite* AnimComposite = Cast<UAnimComposite>(AnimAsset);
			if (AnimComposite && AnimComposite->AnimationTrack.AnimSegments.Num() > 0)
			{
				PlayLength = AnimComposite->GetPlayLength();
				UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimComposite->AnimationTrack.AnimSegments[0].AnimReference);
#if ENGINE_MAJOR_VERSION < 5
				FrameRate = AnimSequence ? AnimSequence->GetFrameRate() : FrameRate;
#else
				FrameRate = AnimSequence ? AnimSequence->GetFrameRate() : FrameRate;
#endif
				
			}
		} break;
	}

	//check(AnimSequenceBase);

	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");

	FTransportControlArgs TransportControlArgs;
	TransportControlArgs.OnForwardPlay = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Forward);
	TransportControlArgs.OnRecord = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Record);
	TransportControlArgs.OnBackwardPlay = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Backward);
	TransportControlArgs.OnForwardStep = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Forward_Step);
	TransportControlArgs.OnBackwardStep = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Backward_Step);
	TransportControlArgs.OnForwardEnd = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Forward_End);
	TransportControlArgs.OnBackwardEnd = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Backward_End);
	TransportControlArgs.OnToggleLooping = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_ToggleLoop);
	TransportControlArgs.OnGetLooping = FOnGetLooping::CreateSP(this, &SMotionTimelineTransportControls::IsLoopStatusOn);
	TransportControlArgs.OnGetPlaybackMode = FOnGetPlaybackMode::CreateSP(this, &SMotionTimelineTransportControls::GetPlaybackMode);
	TransportControlArgs.OnGetRecording = FOnGetRecording::CreateSP(this, &SMotionTimelineTransportControls::IsRecording);

	ChildSlot
	[
		EditorWidgetsModule.CreateTransportControl(TransportControlArgs)
	];


}

UAnimSingleNodeInstance* SMotionTimelineTransportControls::GetPreviewInstance() const
{
	if (DebugSkelMesh && DebugSkelMesh->IsPreviewOn())
	{
		return DebugSkelMesh->PreviewInstance;
	}

	return nullptr;
}

TArray<UAnimSingleNodeInstance*> SMotionTimelineTransportControls::GetInteractionPreviewInstances() const
{
	if (InteractionDebugSkelMeshs.Num() > 0)
	{
		TArray<UAnimSingleNodeInstance*> InteractionPreviewInstances;
		for (int32 i = 0; i < InteractionDebugSkelMeshs.Num(); i++)
		{
			if (InteractionDebugSkelMeshs[i] && InteractionDebugSkelMeshs[i]->IsPreviewOn())
			{
				InteractionPreviewInstances.Add(InteractionDebugSkelMeshs[i]->PreviewInstance);
			}
		}
		return InteractionPreviewInstances;
	}
	else
	{
		return TArray<UAnimSingleNodeInstance*>();
	}
}

FReply SMotionTimelineTransportControls::OnClick_Forward_Step()
{
	if(!DebugSkelMesh)
		return FReply::Handled();
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		bool bShouldStepCloth = FMath::Abs(PreviewInstance->GetLength() - PreviewInstance->GetCurrentTime()) > SMALL_NUMBER;

		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepForward();

		if (DebugSkelMesh && bShouldStepCloth)
		{
			DebugSkelMesh->bPerformSingleClothingTick = true;
		}
	}
	else if (DebugSkelMesh)
	{


		// Advance a single frame, leaving it paused afterwards
		DebugSkelMesh->GlobalAnimRateScale = 1.0f;
		DebugSkelMesh->TickAnimation(1.0f / FrameRate, false);
		DebugSkelMesh->GlobalAnimRateScale = 0.0f;
	}

	if (InteractionDebugSkelMeshs.Num() > 0)
	{
		TArray<UAnimSingleNodeInstance*> InteractionPreviewInstances = GetInteractionPreviewInstances();
		for (int32 i = 0; i < InteractionPreviewInstances.Num(); i++)
		{
			bool bShouldStepCloth = FMath::Abs(InteractionPreviewInstances[i]->GetLength() - InteractionPreviewInstances[i]->GetCurrentTime()) > SMALL_NUMBER;
			InteractionPreviewInstances[i]->SetPlaying(false);
			InteractionPreviewInstances[i]->StepForward();

			if (InteractionDebugSkelMeshs[i] && bShouldStepCloth)
			{
				InteractionDebugSkelMeshs[i]->bPerformSingleClothingTick = true;
			}
			else if (InteractionDebugSkelMeshs[i])
			{
				// Advance a single frame, leaving it paused afterwards
				InteractionDebugSkelMeshs[i]->GlobalAnimRateScale = 1.0f;
				InteractionDebugSkelMeshs[i]->TickAnimation(1.0f / FrameRate, false);
				InteractionDebugSkelMeshs[i]->GlobalAnimRateScale = 0.0f;
			}
		}
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Forward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	TArray<UAnimSingleNodeInstance*> InteractionPreviewInstances = GetInteractionPreviewInstances();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(PreviewInstance->GetLength(), false);
	}
	if (InteractionPreviewInstances.Num() > 0)
	{
		for (int32 i = 0; i < InteractionPreviewInstances.Num(); i++)
		{
			InteractionPreviewInstances[i]->SetPlaying(false);
			InteractionPreviewInstances[i]->SetPosition(InteractionPreviewInstances[i]->GetLength(), false);
		}
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Backward_Step()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	TArray<UAnimSingleNodeInstance*> InteractionPreviewInstances = GetInteractionPreviewInstances();
	if (PreviewInstance)
	{
		bool bShouldStepCloth = PreviewInstance->GetCurrentTime() > SMALL_NUMBER;

		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepBackward();

		if (DebugSkelMesh && bShouldStepCloth)
		{
			DebugSkelMesh->bPerformSingleClothingTick = true;
		}
	}
	if (InteractionPreviewInstances.Num() > 0)
	{
		for (int32 i = 0; i < InteractionPreviewInstances.Num(); i++)
		{
			bool bShouldStepCloth = InteractionPreviewInstances[i]->GetCurrentTime() > SMALL_NUMBER;

			InteractionPreviewInstances[i]->SetPlaying(false);
			InteractionPreviewInstances[i]->StepBackward();

			if (InteractionDebugSkelMeshs[i] && bShouldStepCloth)
			{
				InteractionDebugSkelMeshs[i]->bPerformSingleClothingTick = true;
			}
		}
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Backward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	TArray<UAnimSingleNodeInstance*> InteractionPreviewInstances = GetInteractionPreviewInstances();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(0.f, false);
	}
	if (InteractionPreviewInstances.Num() > 0)
	{
		for (int32 i = 0; i < InteractionPreviewInstances.Num(); i++)
		{
			InteractionPreviewInstances[i]->SetPlaying(false);
			InteractionPreviewInstances[i]->SetPosition(0.f, false);
		}
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Forward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	TArray<UAnimSingleNodeInstance*> InteractionPreviewInstances = GetInteractionPreviewInstances();

	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if current bIsReverse and bIsPlaying, we'd like to just turn off reverse
		if (bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(false);
		}
		// already playing, simply pause
		else if (bIsPlaying)
		{
			PreviewInstance->SetPlaying(false);

			if (DebugSkelMesh && DebugSkelMesh->bPauseClothingSimulationWithAnim)
			{
				DebugSkelMesh->SuspendClothingSimulation();
			}
		}
		// if not playing, play forward
		else
		{
			//if we're at the end of the animation, jump back to the beginning before playing
			if (PreviewInstance->GetCurrentTime() >= PlayLength)
			{
				PreviewInstance->SetPosition(0.0f, false);
			}

			PreviewInstance->SetReverse(false);
			PreviewInstance->SetPlaying(true);

			if (DebugSkelMesh && DebugSkelMesh->bPauseClothingSimulationWithAnim)
			{
				DebugSkelMesh->ResumeClothingSimulation();
			}
		}
	}
	else if (DebugSkelMesh)
	{
		DebugSkelMesh->GlobalAnimRateScale = (DebugSkelMesh->GlobalAnimRateScale > 0.0f) ? 0.0f : 1.0f;
	}

	if (InteractionPreviewInstances.Num() > 0)
	{
		for (int32 i = 0; i < InteractionPreviewInstances.Num(); i++)
		{
			if (InteractionPreviewInstances[i])
			{
				bool bIsReverse = InteractionPreviewInstances[i]->IsReverse();
				bool bIsPlaying = InteractionPreviewInstances[i]->IsPlaying();
				// if current bIsReverse and bIsPlaying, we'd like to just turn off reverse
				if (bIsReverse && bIsPlaying)
				{
					InteractionPreviewInstances[i]->SetReverse(false);
				}
				// already playing, simply pause
				else if (bIsPlaying)
				{
					InteractionPreviewInstances[i]->SetPlaying(false);

					if (InteractionDebugSkelMeshs[i] && InteractionDebugSkelMeshs[i]->bPauseClothingSimulationWithAnim)
					{
						InteractionDebugSkelMeshs[i]->SuspendClothingSimulation();
					}
				}
				// if not playing, play forward
				else
				{
					//if we're at the end of the animation, jump back to the beginning before playing
					if (InteractionPreviewInstances[i]->GetCurrentTime() >= PlayLength)
					{
						InteractionPreviewInstances[i]->SetPosition(0.0f, false);
					}

					InteractionPreviewInstances[i]->SetReverse(false);
					InteractionPreviewInstances[i]->SetPlaying(true);

					if (InteractionDebugSkelMeshs[i] && InteractionDebugSkelMeshs[i]->bPauseClothingSimulationWithAnim)
					{
						InteractionDebugSkelMeshs[i]->ResumeClothingSimulation();
					}
				}
			}
			else if (InteractionDebugSkelMeshs[i])
			{
				InteractionDebugSkelMeshs[i]->GlobalAnimRateScale = (InteractionDebugSkelMeshs[i]->GlobalAnimRateScale > 0.0f) ? 0.0f : 1.0f;
			}
		}
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Backward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	TArray<UAnimSingleNodeInstance*> InteractionPreviewInstances = GetInteractionPreviewInstances();
	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if currently playing forward, just simply turn on reverse
		if (!bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(true);
		}
		else if (bIsPlaying)
		{
			PreviewInstance->SetPlaying(false);
		}
		else
		{
			//if we're at the beginning of the animation, jump back to the end before playing
			if (PreviewInstance->GetCurrentTime() <= 0.0f)
			{
				PreviewInstance->SetPosition(PlayLength, false);
			}

			PreviewInstance->SetPlaying(true);
			PreviewInstance->SetReverse(true);
		}
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_ToggleLoop()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	TArray<UAnimSingleNodeInstance*> InteractionPreviewInstances = GetInteractionPreviewInstances();
	if (PreviewInstance)
	{
		bool bIsLooping = PreviewInstance->IsLooping();
		PreviewInstance->SetLooping(!bIsLooping);
	}
	if (InteractionPreviewInstances.Num() > 0)
	{
		for (int32 i = 0; i < InteractionPreviewInstances.Num(); i++)
		{
			bool bIsLooping = InteractionPreviewInstances[i]->IsLooping();
			InteractionPreviewInstances[i]->SetLooping(!bIsLooping);
		}
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Record()
{
	//StaticCastSharedRef<FAnimationEditorPreviewScene>(GetPreviewScene())->RecordAnimation();

	return FReply::Handled();
}

bool SMotionTimelineTransportControls::IsLoopStatusOn() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	return (PreviewInstance && PreviewInstance->IsLooping());
}

EPlaybackMode::Type SMotionTimelineTransportControls::GetPlaybackMode() const
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		if (PreviewInstance->IsPlaying())
		{
			return PreviewInstance->IsReverse() ? EPlaybackMode::PlayingReverse : EPlaybackMode::PlayingForward;
		}
		return EPlaybackMode::Stopped;
	}
	else if (DebugSkelMesh)
	{
		return (DebugSkelMesh->GlobalAnimRateScale > 0.0f) ? EPlaybackMode::PlayingForward : EPlaybackMode::Stopped;
	}

	return EPlaybackMode::Stopped;
}

bool SMotionTimelineTransportControls::IsRecording() const
{
	//return StaticCastSharedRef<FAnimationEditorPreviewScene>(GetPreviewScene())->IsRecording();

	return false;
}