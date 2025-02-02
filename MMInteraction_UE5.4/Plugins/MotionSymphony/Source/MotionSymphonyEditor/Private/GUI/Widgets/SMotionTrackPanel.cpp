//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionTrackPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"


#define LOCTEXT_NAMESPACE "MotionTrackPanel"

//////////////////////////////////////////////////////////////////////////
// S2ColumnWidget

void S2ColumnWidget::Construct(const FArguments& InArgs)
{
	this->ChildSlot
		[
			SNew(SBorder)
			.Padding(FMargin(2.f, 2.f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.FillWidth(1)
		[
			SAssignNew(LeftColumn, SVerticalBox)
		]

	+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.WidthOverride(InArgs._WidgetWidth)
		.HAlign(HAlign_Center)
		[
			SAssignNew(RightColumn, SVerticalBox)
		]
		]
		]
		];
}

//////////////////////////////////////////////////////////////////////////
// SMotionTrackPanel

void SMotionTrackPanel::Construct(const FArguments& InArgs)
{
	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;
	InputMin = InArgs._InputMin;
	InputMax = InArgs._InputMax;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;

	WidgetWidth = InArgs._WidgetWidth;
}

TSharedRef<class S2ColumnWidget> SMotionTrackPanel::Create2ColumnWidget(TSharedRef<SVerticalBox> Parent)
{
	TSharedPtr<S2ColumnWidget> NewTrack;
	Parent->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			SAssignNew(NewTrack, S2ColumnWidget)
			.WidgetWidth(WidgetWidth)
		];

	return NewTrack.ToSharedRef();
}

void SMotionTrackPanel::PanInputViewRange(int32 ScreenDelta, FVector2D ScreenViewSize)
{
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, ScreenViewSize);

	float InputDeltaX = ScaleInfo.PixelsPerInput > 0.0f ? ScreenDelta / ScaleInfo.PixelsPerInput : 0.0f;

	float NewViewInputMin = ViewInputMin.Get() + InputDeltaX;
	float NewViewInputMax = ViewInputMax.Get() + InputDeltaX;

	InputViewRangeChanged(NewViewInputMin, NewViewInputMax);
}

void SMotionTrackPanel::InputViewRangeChanged(float ViewMin, float ViewMax)
{
	OnSetInputViewRange.ExecuteIfBound(ViewMin, ViewMax);
}

#undef LOCTEXT_NAMESPACE