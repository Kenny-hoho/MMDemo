// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "AnimGraphNode_TransitionMatching.h"
#include "AnimationGraphSchema.h"
#include "EditorCategoryUtils.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "GraphEditorActions.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "BlueprintActionFilter.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "Animation/AnimComposite.h"

#define LOCTEXT_NAMESPACE "MoSymphNodes"

UAnimGraphNode_TransitionMatching::UAnimGraphNode_TransitionMatching(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_TransitionMatching::GetNodeTitleColor() const
{
	return FColor(200, 100, 100);
}

FText UAnimGraphNode_TransitionMatching::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Transition Matching");

	//if (!HasValidAnimations())
	//{
		//return LOCTEXT("NodeToolTip", "Multi Pose Matching");
	//}

	//Additive Not Supported
	//const bool bAdditive = Node.Sequence->IsValidAdditive();
	//return GetTitleGivenAssetInfo(FText::FromString(Node.Sequence->GetPathName()), false);
}

FText UAnimGraphNode_TransitionMatching::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (HasValidAnimations())
	{
		return LOCTEXT("TransitionMatchTitle", "Transition Matching");
	}
	else
	{
		return LOCTEXT("TransitionMatchNullTitle", "Transition Matching (None)");
	}

}

EAnimAssetHandlerType UAnimGraphNode_TransitionMatching::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UAnimSequence::StaticClass()) || AssetClass->IsChildOf(UAnimComposite::StaticClass()))
	{
		return EAnimAssetHandlerType::Supported;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

FString UAnimGraphNode_TransitionMatching::GetNodeCategory() const
{
	return FString("Motion Symphony");
}

void UAnimGraphNode_TransitionMatching::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (!Context->bIsDebugging)
	{
		// add an option to convert to single frame
		//FToolMenuSection& Section = Menu->AddSection("AnimGraphNodePoseMatching", NSLOCTEXT("A3Nodes", "PoseMatchHeading", "Pose Matching"));
		//Section.AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
	}
}

//void UAnimGraphNode_TransitionMatching::SetAnimationAsset(UAnimationAsset * Asset)
//{
//	if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(Asset))
//	{
//		Node.Sequence = Seq;
//	}
//}

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25 
void UAnimGraphNode_TransitionMatching::OnProcessDuringCompilation(
	IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{

}
#endif

FText UAnimGraphNode_TransitionMatching::GetTitleGivenAssetInfo(const FText& AssetName, bool bKnownToBeAdditive)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), AssetName);

	return FText::Format(LOCTEXT("TransitionMatchNodeTitle", "Transition Matching \n {AssetName}"), Args);
}

FText UAnimGraphNode_TransitionMatching::GetNodeTitleForSequence(ENodeTitleType::Type TitleType, UAnimSequenceBase* InSequence) const
{
	const FText BasicTitle = GetTitleGivenAssetInfo(FText::FromName(InSequence->GetFName()), false);

	if (SyncGroup.GroupName == NAME_None)
	{
		return BasicTitle;
	}
	else
	{
		const FText SyncGroupName = FText::FromName(SyncGroup.GroupName);

		FFormatNamedArguments Args;
		Args.Add(TEXT("Title"), BasicTitle);
		Args.Add(TEXT("SyncGroup"), SyncGroupName);

		if (TitleType == ENodeTitleType::FullTitle)
		{
			return FText::Format(LOCTEXT("TransitionMatchingNodeGroupWithSubtitleFull", "{Title}\nSync group {SyncGroup}"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("TransitionMatchingNodeGroupWithSubtitleList", "{Title} (Sync group {SyncGroup})"), Args);
		}
	}
}

bool UAnimGraphNode_TransitionMatching::HasValidAnimations() const
{
	if (Node.TransitionAnimData.Num() > 0)
	{
		for (const FTransitionAnimData& TransitionData : Node.TransitionAnimData)
		{
			UAnimSequence* Sequence = TransitionData.AnimSequence;
			if (Sequence != nullptr && Sequence->IsValidToPlay())
			{
				return true;
			}
		}

		return false;
	}

	return false;
}

FString UAnimGraphNode_TransitionMatching::GetControllerDescription() const
{
	return TEXT("Transition Matching Animation Node");
}

//void UAnimGraphNode_TransitionMatching::CreateOutputPins()
//{
//	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
//	CreatePin(EGPD_Output, Schema->PC_Struct, TEXT(""), FPoseLink::StaticStruct(), /*bIsArray=*/ false, /*bIsReference=*/ false, TEXT("Pose"));
//}

void UAnimGraphNode_TransitionMatching::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);

	TArray<UAnimSequence*> SequencesToCheck;
	SequencesToCheck.Empty(Node.TransitionAnimData.Num());

	for (FTransitionAnimData& TransitionData : Node.TransitionAnimData)
	{
		UAnimSequence* Sequence = TransitionData.AnimSequence;
		UAnimSequenceBase* SequenceToCheck = Cast<UAnimSequenceBase>(Sequence);

		if (SequenceToCheck == nullptr)
		{
			MessageLog.Error(TEXT("@@ references to unknown sequence in animation list"), this);
		}
		else if (SupportsAssetClass(SequenceToCheck->GetClass()) == EAnimAssetHandlerType::NotSupported)
		{
			MessageLog.Error(*FText::Format(LOCTEXT("UnsupportedAssetError", "@@ is trying to play a {0} as a sequence, which is not allowed."), SequenceToCheck->GetClass()->GetDisplayNameText()).ToString(), this);
		}
		else if (SequenceToCheck->IsValidAdditive())
		{
			MessageLog.Error(TEXT("@@ is trying to play an additive animation sequence, which is not allowed."), this);
		}
		else
		{
			USkeleton* SeqSkeleton = SequenceToCheck->GetSkeleton();
			if (SeqSkeleton &&
				!SeqSkeleton->IsCompatible(ForSkeleton))
			{
				MessageLog.Error(TEXT("@@ references sequence that uses different skeleton @@"), this, SeqSkeleton);
			}
		}
	}
}

void UAnimGraphNode_TransitionMatching::PreloadRequiredAssets()
{
	for (FTransitionAnimData& TransitionData : Node.TransitionAnimData)
	{
		UAnimSequence* Sequence = TransitionData.AnimSequence;
		if (Sequence != nullptr)
		{
			PreloadObject(Sequence);
		}
	}

	Super::PreloadRequiredAssets();
}

void UAnimGraphNode_TransitionMatching::BakeDataDuringCompilation(FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25
	Node.GroupName = SyncGroup.GroupName;
#else
	Node.GroupIndex = AnimBlueprint->FindOrAddGroup(SyncGroup.GroupName);
#endif

	Node.GroupRole = SyncGroup.GroupRole;

	//Pre-Process the pose data here
	Node.PreProcess();
}

void UAnimGraphNode_TransitionMatching::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	for (const FTransitionAnimData& TransitionData : Node.TransitionAnimData)
	{
		UAnimSequence* Sequence = TransitionData.AnimSequence;
		if (Sequence != nullptr)
		{
			HandleAnimReferenceCollection(Sequence, AnimationAssets);
		}
	}

	if (Node.Sequence)
	{
		HandleAnimReferenceCollection(Node.Sequence, AnimationAssets);
	}
}

void UAnimGraphNode_TransitionMatching::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	for (int32 i = 0; i < Node.TransitionAnimData.Num(); ++i)
	{
		FTransitionAnimData& TransitionData = Node.TransitionAnimData[i];

		UAnimSequence* CacheOriginalAsset = TransitionData.AnimSequence;

		if (UAnimationAsset* const* ReplacementAsset = AnimAssetReplacementMap.Find(CacheOriginalAsset))
		{
			TransitionData.AnimSequence = Cast<UAnimSequence>(*ReplacementAsset);
		}
	}

	HandleAnimReferenceReplacement(Node.Sequence, AnimAssetReplacementMap);
}

bool UAnimGraphNode_TransitionMatching::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_TransitionMatching::GetAnimationAsset() const
{
	if (Node.TransitionAnimData.Num() == 0)
		return nullptr;

	return Node.TransitionAnimData[0].AnimSequence;
}

const TCHAR* UAnimGraphNode_TransitionMatching::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_TransitionMatching::GetTimePropertyStruct() const
{
	return FAnimNode_TransitionMatching::StaticStruct();
}

void UAnimGraphNode_TransitionMatching::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TransitionMatching, PlayRate))
	{
		if (!Pin->bHidden)
		{
			// Draw value for PlayRateBasis if the pin is not exposed
			UEdGraphPin* PlayRateBasisPin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TransitionMatching, PlayRateBasis));
			if (!PlayRateBasisPin || PlayRateBasisPin->bHidden)
			{
				if (Node.PlayRateBasis != 1.f)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
					Args.Add(TEXT("PlayRateBasis"), FText::AsNumber(Node.PlayRateBasis));
					Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_TransitionMatching_PlayRateBasis_Value", "({PinFriendlyName} / {PlayRateBasis})"), Args);
				}
			}
			else // PlayRateBasisPin is visible
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
				Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_TransitionMatching_PlayRateBasis_Name", "({PinFriendlyName} / PlayRateBasis)"), Args);
			}

			Pin->PinFriendlyName = Node.PlayRateScaleBiasClamp.GetFriendlyName(Pin->PinFriendlyName);
		}
	}
}

void UAnimGraphNode_TransitionMatching::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);

	// Reconstruct node to show updates to PinFriendlyNames.
	if ((PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TransitionMatching, PlayRateBasis))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, bMapRange))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputRange, Min))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputRange, Max))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, Scale))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, Bias))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, bClampResult))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, ClampMin))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, ClampMax))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, bInterpResult))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, InterpSpeedIncreasing))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, InterpSpeedDecreasing)))
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimGraphNode_TransitionMatching::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
	NodeSpawner->DefaultMenuSignature.MenuName = FText::FromString(TEXT("Transition Matching"));
	NodeSpawner->DefaultMenuSignature.Tooltip = FText::FromString(TEXT("Special sequence player which uses pose and direction matching to pick the sequence to use and it's starting point."));
	ActionRegistrar.AddBlueprintAction(NodeSpawner);
}

#undef LOCTEXT_NAMESPACE
