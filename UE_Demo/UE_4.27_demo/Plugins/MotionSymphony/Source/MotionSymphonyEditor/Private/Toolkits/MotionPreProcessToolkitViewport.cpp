// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "MotionPreProcessToolkitViewport.h"
#include "MotionPreProcessorToolkitCommands.h"

#define LOCTEXT_NAMESPACE "MotionPreProcessToolkit"


void SMotionPreProcessToolkitViewport::Construct(const FArguments& InArgs, TWeakPtr<FMotionPreProcessToolkit> InMotionPreProcessToolkitPtr)
{
	MotionDataBeingEdited = InArgs._MotionDataBeingEdited;
	MotionPreProcessToolkitPtr = InMotionPreProcessToolkitPtr;
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

void SMotionPreProcessToolkitViewport::SetupAnimatedRenderComponent()
{
	EditorViewportClient->SetupAnimatedRenderComponent();
}

void SMotionPreProcessToolkitViewport::SetupInteractionAnimatedRenderComponents(TArray<USkeleton*> InteractionSkeletons)
{
	EditorViewportClient->SetupInteractionAnimatedRenderComponents(InteractionSkeletons);
}

void SMotionPreProcessToolkitViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FMotionPreProcessToolkitCommands& Commands = FMotionPreProcessToolkitCommands::Get();

	TSharedRef<FMotionPreProcessToolkitViewportClient> EditorViewportClientRef = EditorViewportClient.ToSharedRef();

	CommandList->MapAction(
		Commands.SetShowGrid,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FEditorViewportClient::SetShowGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FEditorViewportClient::IsSetShowGridChecked));

	CommandList->MapAction(
		Commands.SetShowBounds,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FEditorViewportClient::ToggleShowBounds),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FEditorViewportClient::IsSetShowBoundsChecked));

	CommandList->MapAction(
		Commands.SetShowCollision,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FEditorViewportClient::SetShowCollision),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FEditorViewportClient::IsSetShowCollisionChecked));

	CommandList->MapAction(
		Commands.SetShowPivot,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::ToggleShowPivot),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::IsShowPivotChecked));

	CommandList->MapAction(
		Commands.SetShowMatchBones,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::ToggleShowMatchBones),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::IsShowMatchBonesChecked));

	CommandList->MapAction(
		Commands.SetShowTrajectory,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::ToggleShowTrajectory),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::IsShowTrajectoryChecked));

	CommandList->MapAction(
		Commands.SetShowPose,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::ToggleShowPose),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::IsShowPoseChecked));

	CommandList->MapAction(
		Commands.SetShowOptimisationDebug,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::ToggleShowOptimizationDebug),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FMotionPreProcessToolkitViewportClient::IsShowOptimizationDebugChecked));
}

TSharedRef<FEditorViewportClient> SMotionPreProcessToolkitViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable(new FMotionPreProcessToolkitViewportClient
		(MotionDataBeingEdited, MotionPreProcessToolkitPtr));

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SMotionPreProcessToolkitViewport::MakeViewportToolbar()
{
	return SNew(SMotionPreProcessToolkitViewportToolbar, SharedThis(this));
}

EVisibility SMotionPreProcessToolkitViewport::GetTransformToolbarVisibility() const
{
	return EVisibility::Visible;
}

void SMotionPreProcessToolkitViewport::OnFocusViewportToSelection()
{
	EditorViewportClient->RequestFocusOnSelection(false);
}

TSharedRef<class SEditorViewport> SMotionPreProcessToolkitViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SMotionPreProcessToolkitViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SMotionPreProcessToolkitViewport::OnFloatingButtonClicked()
{

}

UDebugSkelMeshComponent* SMotionPreProcessToolkitViewport::GetPreviewComponent() const
{
	return EditorViewportClient->GetPreviewComponent();
}

TArray<UDebugSkelMeshComponent*> SMotionPreProcessToolkitViewport::GetInteractionPreviewMeshComponents() const
{
	return EditorViewportClient->GetInteractionPreviewComponents();
}

#undef LOCTEXT_NAMESPACE
