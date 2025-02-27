// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "AddNewAnimDialog.h"
//#include "MotionSymphony.h"
#include "MotionPreProcessToolkit.h"

#include "CoreMinimal.h"
#include "ContentBrowserModule.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"

#include "IContentBrowserSingleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimComposite.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyEditor"

namespace MotionSymphonyEditorConstants
{
	const int32 ThumbnailSize = 128;
	const int32 ThumnailSizeSmall = 16;
}

void SAddNewAnimDialog::Construct(const FArguments& InArgs, TSharedPtr<FMotionPreProcessToolkit> InMotionPreProcessToolkitPtr)
{
	MotionPreProcessToolkitPtr = InMotionPreProcessToolkitPtr;
	SkeletonName = MotionPreProcessToolkitPtr.Get()->GetSkeletonName();

	TSharedPtr<SVerticalBox> MainBox = SNew(SVerticalBox);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Add(UAnimSequence::StaticClass()->GetFName());
	AssetPickerConfig.Filter.ClassNames.Add(UBlendSpace::StaticClass()->GetFName());
	AssetPickerConfig.Filter.ClassNames.Add(UBlendSpace1D::StaticClass()->GetFName());
	AssetPickerConfig.Filter.ClassNames.Add(UAnimComposite::StaticClass()->GetFName());
	AssetPickerConfig.SelectionMode = ESelectionMode::Multi;
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SAddNewAnimDialog::FilterAnim);
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
	AssetPickerConfig.ThumbnailScale = MotionSymphonyEditorConstants::ThumbnailSize;
	
	MainBox->AddSlot()
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryTop"))
			.Padding(FMargin(1.0f, 1.0f, 1.0f, 1.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[

					SNew(SScrollBox)
					+ SScrollBox::Slot()
					.Padding(1.0f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						MainBox.ToSharedRef()
					]

				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SUniformGridPanel)
						.SlotPadding(1)
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
							.ForegroundColor(FLinearColor::White)
							.Text(LOCTEXT("AddButton", "Add"))
							.OnClicked(this, &SAddNewAnimDialog::AddClicked)
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "FlatButton")
							.ForegroundColor(FLinearColor::White)
							.Text(LOCTEXT("CancelButton", "Cancel"))
							.OnClicked(this, &SAddNewAnimDialog::CancelClicked)
						]
					]
				]
			]
		];
}

SAddNewAnimDialog::~SAddNewAnimDialog()
{

}

bool SAddNewAnimDialog::ShowWindow(TSharedPtr<FMotionPreProcessToolkit> InMotionPreProcessToolkit)
{
	const FText TitleText = NSLOCTEXT("MotionPreProcessToolkit", "MotionPreProcessToolkit_AddNewAnim", "Add NewAnimation");
	TSharedRef<SWindow> AddNewAnimationsWindow = SNew(SWindow)
		.Title(TitleText)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(1100.f, 600.f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false);

	TSharedRef<SAddNewAnimDialog> AddNewAnimsDialog = SNew(SAddNewAnimDialog, InMotionPreProcessToolkit);

	AddNewAnimationsWindow->SetContent(AddNewAnimsDialog);
	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (RootWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(AddNewAnimationsWindow, RootWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(AddNewAnimationsWindow);
	}

	return false;
}

bool SAddNewAnimDialog::FilterAnim(const FAssetData& AssetData)
{
	if (!AssetData.IsAssetLoaded())
	{
		AssetData.GetPackage()->FullyLoad();
	}

	if (MotionPreProcessToolkitPtr.Get()->AnimationAlreadyAdded(AssetData.AssetName))
		return true;

	UAnimSequence* Sequence = Cast<UAnimSequence>(AssetData.GetAsset());
	if (Sequence)
	{
		return SkeletonName != Sequence->GetSkeleton()->GetName();
	}

	UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(AssetData.GetAsset());
	if (BlendSpace)
	{
		return SkeletonName != BlendSpace->GetSkeleton()->GetName();
	}

	UAnimComposite* Composite = Cast<UAnimComposite>(AssetData.GetAsset());
	if(Composite)
	{
		return SkeletonName != Composite->GetSkeleton()->GetName();
	}

	
	return true;
}

FReply SAddNewAnimDialog::AddClicked()
{
	TArray<FAssetData> SelectionArray = GetCurrentSelectionDelegate.Execute();

	if (SelectionArray.Num() > 0)
	{
		TArray<UAnimSequence*> StoredSequences;
		TArray<UBlendSpaceBase*> StoredBlendSpaces;
		TArray<UAnimComposite*> StoredComposites;

		for (int i = 0; i < SelectionArray.Num(); ++i)
		{
			if (SelectionArray[i].IsAssetLoaded())
			{
				UAnimSequence* NewSequence = Cast<UAnimSequence>(SelectionArray[i].GetAsset());
				UBlendSpaceBase* NewBlendSpace = Cast<UBlendSpaceBase>(SelectionArray[i].GetAsset());
				UAnimComposite* NewComposite = Cast<UAnimComposite>(SelectionArray[i].GetAsset());
				if (NewSequence)
				{
					StoredSequences.Add(NewSequence);
				}
				else if (NewBlendSpace)
				{
					StoredBlendSpaces.Add(NewBlendSpace);
				}
				else if (NewComposite)
				{
					StoredComposites.Add(NewComposite);
				}
			}
		}

		if (StoredSequences.Num() > 0)
		{
			MotionPreProcessToolkitPtr.Get()->AddNewAnimSequences(StoredSequences);
		}

		if (StoredBlendSpaces.Num() > 0)
		{
			MotionPreProcessToolkitPtr.Get()->AddNewBlendSpaces(StoredBlendSpaces);
		}

		if (StoredComposites.Num() > 0)
		{
			MotionPreProcessToolkitPtr.Get()->AddNewComposites(StoredComposites);
		}

		CloseContainingWindow();
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoAnimationsSelected", "No Animations Selected."));
	}

	return FReply::Handled();
}

FReply SAddNewAnimDialog::CancelClicked()
{
	CloseContainingWindow();
	return FReply::Handled();
}

void SAddNewAnimDialog::CloseContainingWindow()
{
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE