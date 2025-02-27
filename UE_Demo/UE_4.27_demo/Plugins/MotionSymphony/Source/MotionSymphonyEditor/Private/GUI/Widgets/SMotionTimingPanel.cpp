// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "SMotionTimingPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Animation/AnimMontage.h"
#include "Preferences/PersonaOptions.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "STimingTrackMM.h"
#include "ISkeletonEditorModule.h"
#include "Modules/ModuleManager.h"
#include "IEditableSkeleton.h"
#include "Editor.h"
#include "Controls/MotionModel.h"
#include "Preferences/PersonaOptions.h"
#include "FrameNumberDisplayFormat.h"

#define LOCTEXT_NAMESPACE "MotionTimingPanel"

namespace MotionTimingConstants
{
	static const float DefaultNodeSize = 18.0f;
	static const int32 FontSize = 10;
}

class SMotionTimingNodeTooltip : public SToolTip
{
public:
	void Construct(const FArguments& InArgs, const TSharedRef<FTimingRelevantElementBase>& InElement)
	{
		Element = InElement;
		DescriptionBox = SNew(SVerticalBox);

		FArguments Args = InArgs;
		Args._TextMargin = FMargin(1.0f);
		Args._BorderImage = FEditorStyle::GetBrush("ContentBrowser.TileViewToolTip.ToolTipBorder");
		Args._Content.Widget =
			SNew(SBorder)
			.Padding(3)
			.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.NonContentBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 3)
				[
					SNew(SBorder)
					.Padding(6)
					.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
					[
						SNew(SBox)
						.HAlign(HAlign_Left)
						[
						SNew(STextBlock)
						.Text(FText::FromName(Element->GetTypeName()))
						.Font(FEditorStyle::GetFontStyle("ContentBrowser.TileViewTooltip.NameFont"))
						]
					]
				]

				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SBorder)
						.Padding(3)
						.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
						[
							DescriptionBox.ToSharedRef()
						]
					]
				]
			];

		SToolTip::Construct(Args);
	}

	virtual void OnOpening() override
	{
		DescriptionBox->ClearChildren();

		TMap<FString, FText> DescriptionItems;
		Element->GetDescriptionItems(DescriptionItems);
		for (TPair<FString, FText> ItemPair : DescriptionItems)
		{
			DescriptionBox->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 3, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("Item", "{0}: "), FText::FromString(ItemPair.Key)))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(ItemPair.Value)
				.ColorAndOpacity(FSlateColor::UseForeground())
				]
				];
		}
	}

	TSharedPtr<SVerticalBox> DescriptionBox;
	TSharedPtr<FTimingRelevantElementBase> Element;
};

void SMotionTimingNode::Construct(const FArguments& InArgs)
{
	Element = InArgs._InElement;

	const FSlateBrush* StyleInfo = FEditorStyle::GetBrush(TEXT("SpecialEditableTextImageNormal"));
	static FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", MotionTimingConstants::FontSize);

	UPersonaOptions* EditorOptions = UPersonaOptions::StaticClass()->GetDefaultObject<UPersonaOptions>();
	check(EditorOptions);

	// Pick the colour of the node from the type of the element
	FLinearColor NodeColour = FLinearColor::White;
	switch (Element->GetType())
	{
	case ETimingElementType::QueuedNotify:
	case ETimingElementType::NotifyStateBegin:
	case ETimingElementType::NotifyStateEnd:
	{
		NodeColour = EditorOptions->NotifyTimingNodeColor;
		break;
	}
	case ETimingElementType::BranchPointNotify:
	{
		NodeColour = EditorOptions->BranchingPointTimingNodeColor;
		break;
	}
	case ETimingElementType::Section:
	{
		NodeColour = EditorOptions->SectionTimingNodeColor;
		break;
	}
	default:
		break;
	}

	this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(StyleInfo)
			.BorderBackgroundColor(NodeColour)
			[
				SNew(STextBlock)
				.Justification(ETextJustify::Center)
				.Text(FText::AsNumber(Element->TriggerIdx))
				.Font(LabelFont)
				.ColorAndOpacity(FSlateColor(FLinearColor::Black))
			]
		];

	if (InArgs._bUseTooltip)
	{
		SetToolTip(SNew(SMotionTimingNodeTooltip, Element.ToSharedRef()));
	}
}

FVector2D SMotionTimingNode::ComputeDesiredSize(float) const
{
	// Desired height is always the same (a little less than the track height) but the width depends on the text we display
	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	static FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	float TextWidth = FontMeasureService->Measure(FString::FromInt(Element->TriggerIdx), LabelFont).X;
	return FVector2D(FMath::Max(MotionTimingConstants::DefaultNodeSize, TextWidth), MotionTimingConstants::DefaultNodeSize);
}

void SMotionTimingTrackNode::Construct(const FArguments& InArgs)
{
	TAttribute<float> TimeAttr = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(InArgs._Element.ToSharedRef(), &FTimingRelevantElementBase::GetElementTime));

	SGenericTrackNode::Construct(SGenericTrackNode::FArguments()
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.DataStartPos(TimeAttr)
		.NodeName(InArgs._NodeName)
		.CenterOnPosition(true)
		.AllowDrag(false)
		.OverrideContent()
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
		/*[
			SNew(SMotionTimingNode)
			.InElement(InArgs._Element)
			.bUseTooltip(InArgs._bUseTooltip)
		]*/
		]
	);
}

void SMotionTimingPanel::Construct(const FArguments& InArgs/*, const TSharedRef<FAnimModel_AnimMontage>& InModel*/)
{
	//WeakModel = InModel;

	SMotionTrackPanel::Construct(SMotionTrackPanel::FArguments()
		.WidgetWidth(InArgs._WidgetWidth)
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.InputMin(InArgs._InputMin)
		.InputMax(InArgs._InputMax)
		.OnSetInputViewRange(InArgs._OnSetInputViewRange));

	AnimSequence = InArgs._InSequence;

	check(AnimSequence);

	this->ChildSlot
		[
			SAssignNew(PanelArea, SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Padding(0.0f)
			.ColorAndOpacity(FLinearColor::White)
		];

	Update();

	// Register to some delegates to update the interface
	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	TSharedPtr<IEditableSkeleton> EditableSkeleton = SkeletonEditorModule.CreateEditableSkeleton(AnimSequence->GetSkeleton());
	EditableSkeleton->RegisterOnNotifiesChanged(FSimpleDelegate::CreateSP(this, &SMotionTimingPanel::RefreshTrackNodes));

	//InModel->OnTracksChanged().Add(FSimpleDelegate::CreateSP(this, &SMotionTimingPanel::RefreshTrackNodes));
}

void SMotionTimingPanel::Update()
{
	check(PanelArea.IsValid());

	TSharedPtr<SVerticalBox> TimingSlots;

	PanelArea->SetContent(SAssignNew(TimingSlots, SVerticalBox));

#if ENGINE_MAJOR_VERSION > 4
	int32 NumSampledKeys = AnimSequence->GetNumberOfSampledKeys();
#else
	int32 NumSampledKeys = AnimSequence->GetNumberOfFrames();
#endif

	TimingSlots->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			SAssignNew(Track, STimingTrackMM)
			.ViewInputMin(ViewInputMin)
			.ViewInputMax(ViewInputMax)
			.TrackMinValue(InputMin)
			.TrackMaxValue(InputMax)
			.TrackNumDiscreteValues(NumSampledKeys)
		];

	RefreshTrackNodes();
}

void SMotionTimingPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SMotionTrackPanel::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SMotionTimingPanel::RefreshTrackNodes()
{
	Elements.Empty();
	GetTimingRelevantElements(AnimSequence, Elements);

	Track->ClearTrack();

	int32 NumElements = Elements.Num();
	for (int32 ElementIdx = 0; ElementIdx < NumElements; ++ElementIdx)
	{
		/*TSharedPtr<FTimingRelevantElementBase> Element = Elements[ElementIdx];
		if (WeakModel.Pin()->IsTimingElementDisplayEnabled(Element->GetType()))
		{
			Track->AddTrackNode(
				SNew(SMotionTimingTrackNode)
				.ViewInputMin(ViewInputMin)
				.ViewInputMax(ViewInputMax)
				.DataStartPos(Element.ToSharedRef(), &FTimingRelevantElementBase::GetElementTime)
				.NodeName(FString::FromInt(ElementIdx + 1))
				.NodeColor(FLinearColor::Yellow)
				.Element(Element)
			);
		}*/
	}
}

bool SMotionTimingPanel::IsElementDisplayEnabled(ETimingElementType::Type ElementType) const
{
	//return WeakModel.Pin()->IsTimingElementDisplayEnabled(ElementType);
	return false;
}

void SMotionTimingPanel::OnElementDisplayEnabledChanged(ETimingElementType::Type ElementType)
{
	//WeakModel.Pin()->ToggleTimingElementDisplayEnabled(ElementType);
	Update();
}

ECheckBoxState SMotionTimingPanel::IsElementDisplayChecked(ETimingElementType::Type ElementType) const
{
	//return WeakModel.Pin()->IsTimingElementDisplayEnabled(ElementType) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	return ECheckBoxState::Unchecked;
}

EVisibility SMotionTimingPanel::IsElementDisplayVisible(ETimingElementType::Type ElementType) const
{
	//return WeakModel.Pin()->IsTimingElementDisplayEnabled(ElementType) ? EVisibility::Visible : EVisibility::Hidden;
	return EVisibility::Hidden;
}

void SMotionTimingPanel::GetTimingRelevantElements(UAnimSequenceBase* Sequence, TArray<TSharedPtr<FTimingRelevantElementBase>>& Elements)
{
	if (Sequence)
	{
		// Grab notifies
		int32 NumNotifies = Sequence->Notifies.Num();
		for (int32 NotifyIdx = 0; NotifyIdx < NumNotifies; ++NotifyIdx)
		{
			FAnimNotifyEvent& Notify = Sequence->Notifies[NotifyIdx];

			FTimingRelevantElement_Notify* Element = new FTimingRelevantElement_Notify;
			Element->Sequence = Sequence;
			Element->NotifyIndex = NotifyIdx;
			Elements.Add(TSharedPtr<FTimingRelevantElementBase>(Element));

			if (Notify.NotifyStateClass)
			{
				// Add the end marker
				FTimingRelevantElement_NotifyStateEnd* EndElement = new FTimingRelevantElement_NotifyStateEnd;
				EndElement->Sequence = Sequence;
				EndElement->NotifyIndex = NotifyIdx;
				Elements.Add(TSharedPtr<FTimingRelevantElementBase>(EndElement));
			}
		}

		// Check for a montage and extract Montage elements
		if (UAnimMontage* Montage = Cast<UAnimMontage>(Sequence))
		{
			// Add sections
			int32 NumSections = Montage->CompositeSections.Num();

			for (int32 SectionIdx = 0; SectionIdx < NumSections; ++SectionIdx)
			{
				FCompositeSection& Section = Montage->CompositeSections[SectionIdx];

				FTimingRelevantElement_Section* Element = new FTimingRelevantElement_Section;
				Element->Montage = Montage;
				Element->SectionIdx = SectionIdx;
				Elements.Add(TSharedPtr<FTimingRelevantElementBase>(Element));
			}
		}

		// Sort everything and give them trigger orders
		Elements.Sort([](const TSharedPtr<FTimingRelevantElementBase>& A, const TSharedPtr<FTimingRelevantElementBase>& B)
			{
				return A->Compare(*B);
			});

		int32 NumElements = Elements.Num();
		for (int32 Idx = 0; Idx < NumElements; ++Idx)
		{
			Elements[Idx]->TriggerIdx = Idx;
		}
	}
}

FName FTimingRelevantElement_Section::GetTypeName()
{
	return FName("Montage Section");
}

float FTimingRelevantElement_Section::GetElementTime() const
{
	check(Montage)
		if (Montage->CompositeSections.IsValidIndex(SectionIdx))
		{
			return Montage->CompositeSections[SectionIdx].GetTime();
		}
	return -1.0f;
}

ETimingElementType::Type FTimingRelevantElement_Section::GetType()
{
	return ETimingElementType::Section;
}

void FTimingRelevantElement_Section::GetDescriptionItems(TMap<FString, FText>& Items)
{
	check(Montage);
	FCompositeSection& Section = Montage->CompositeSections[SectionIdx];

	Items.Add(LOCTEXT("SectionName", "Name").ToString(), FText::FromName(Section.SectionName));
	if (GetDefault<UPersonaOptions>()->TimelineDisplayFormat == EFrameNumberDisplayFormats::Frames)
	{
		Items.Add(LOCTEXT("SectionTriggerFrame", "Trigger Frame").ToString(), FText::Format(LOCTEXT("SectionTriggerFrameValue", "{0}"), FText::AsNumber(Montage->GetFrameAtTime(Section.GetTime()))));
	}
	else
	{
		FNumberFormattingOptions NumberOptions;
		NumberOptions.MinimumFractionalDigits = 3;
		Items.Add(LOCTEXT("SectionTriggerTime", "Trigger Time").ToString(), FText::Format(LOCTEXT("SectionTriggerTimeValue", "{0}s"), FText::AsNumber(Section.GetTime(), &NumberOptions)));
	}
}

FName FTimingRelevantElement_Notify::GetTypeName()
{
	FName TypeName;
	switch (GetType())
	{
	case ETimingElementType::NotifyStateBegin:
		TypeName = FName("Notify State (Begin)");
		break;
	case ETimingElementType::BranchPointNotify:
		TypeName = FName("Branching Point");
		break;
	default:
		TypeName = FName("Notify");
	}

	return TypeName;
}

float FTimingRelevantElement_Notify::GetElementTime() const
{
	check(Sequence);
	if (Sequence->Notifies.IsValidIndex(NotifyIndex))
	{
		return Sequence->Notifies[NotifyIndex].GetTriggerTime();
	}
	return -1.0f;
}

ETimingElementType::Type FTimingRelevantElement_Notify::GetType()
{
	check(Sequence);
	FAnimNotifyEvent& Event = Sequence->Notifies[NotifyIndex];

	if (Event.IsBranchingPoint())
	{
		return ETimingElementType::BranchPointNotify;
	}
	else
	{
		if (Event.NotifyStateClass)
		{
			return ETimingElementType::NotifyStateBegin;
		}
		else
		{
			return ETimingElementType::QueuedNotify;
		}
	}
}

void FTimingRelevantElement_Notify::GetDescriptionItems(TMap<FString, FText>& Items)
{
	check(Sequence);
	FAnimNotifyEvent& Event = Sequence->Notifies[NotifyIndex];

	FNumberFormattingOptions NumberOptions;
	NumberOptions.MinimumFractionalDigits = 3;

	Items.Add(LOCTEXT("NotifyName", "Name").ToString(), FText::FromName(Event.NotifyName));

	if (GetDefault<UPersonaOptions>()->TimelineDisplayFormat == EFrameNumberDisplayFormats::Frames)
	{
		Items.Add(LOCTEXT("NotifyTriggerFrame", "Trigger Frame").ToString(), FText::Format(LOCTEXT("NotifyTriggerFrame_Val", "{0}"), FText::AsNumber(Sequence->GetFrameAtTime(Event.GetTime()))));
	}
	else
	{
		Items.Add(LOCTEXT("NotifyTriggerTime", "Trigger Time").ToString(), FText::Format(LOCTEXT("NotifyTriggerTime_Val", "{0}s"), FText::AsNumber(Event.GetTime(), &NumberOptions)));
	}

	// +1 as we start at 1 when showing tracks to the user
	Items.Add(LOCTEXT("TrackIdx", "Track").ToString(), FText::AsNumber(Event.TrackIndex + 1));

	if (Event.NotifyStateClass)
	{
		Items.Add(LOCTEXT("NotifyDuration", "Duration").ToString(), FText::Format(LOCTEXT("NotifyDuration_Val", "{0}s"), FText::AsNumber(Event.GetDuration(), &NumberOptions)));
	}
}

int32 FTimingRelevantElement_Notify::GetElementSortPriority() const
{
	check(Sequence);
	FAnimNotifyEvent& Event = Sequence->Notifies[NotifyIndex];
	return Event.TrackIndex;
}

FName FTimingRelevantElement_NotifyStateEnd::GetTypeName()
{
	return FName("Notify State (End)");
}

float FTimingRelevantElement_NotifyStateEnd::GetElementTime() const
{
	check(Sequence);
	FAnimNotifyEvent& Event = Sequence->Notifies[NotifyIndex];
	check(Event.NotifyStateClass);
	return Event.GetEndTriggerTime();
}

ETimingElementType::Type FTimingRelevantElement_NotifyStateEnd::GetType()
{
	return ETimingElementType::NotifyStateEnd;
}

#undef LOCTEXT_NAMESPACE