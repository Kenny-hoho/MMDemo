// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "AnimGraphNode_MultiPoseMatching.h"
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
//#include "Runtime/Launch/Resources/Version.h"
#include "Animation/AnimComposite.h"

#define LOCTEXT_NAMESPACE "MoSymphNodes"

UAnimGraphNode_MultiPoseMatching::UAnimGraphNode_MultiPoseMatching(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_MultiPoseMatching::GetNodeTitleColor() const
{
	return FColor(200, 100, 100);
}

FText UAnimGraphNode_MultiPoseMatching::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Multi Pose Matching");

	//if (!HasValidAnimations())
	//{
		//return LOCTEXT("NodeToolTip", "Multi Pose Matching");
	//}

	//Additive Not Supported
	//const bool bAdditive = Node.Sequence->IsValidAdditive();
	//return GetTitleGivenAssetInfo(FText::FromString(Node.Sequence->GetPathName()), false);
}

FText UAnimGraphNode_MultiPoseMatching::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (HasValidAnimations())
	{
		return LOCTEXT("MultiPoseMatchTitle", "Multi Pose Matching");
	}
	else
	{
		return LOCTEXT("MultiPoseMatchNullTitle", "Multi Pose Matching (None)");
	}
	
}

EAnimAssetHandlerType UAnimGraphNode_MultiPoseMatching::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UAnimSequence::StaticClass()) || AssetClass->IsChildOf(UAnimComposite::StaticClass()))
	{
		return EAnimAssetHandlerType::PrimaryHandler;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

FString UAnimGraphNode_MultiPoseMatching::GetNodeCategory() const
{
	return FString("Motion Symphony");
}

void UAnimGraphNode_MultiPoseMatching::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (!Context->bIsDebugging)
	{
		// add an option to convert to single frame
		//FToolMenuSection& Section = Menu->AddSection("AnimGraphNodePoseMatching", NSLOCTEXT("A3Nodes", "PoseMatchHeading", "Pose Matching"));
		//Section.AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
	}

}

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25 
void UAnimGraphNode_MultiPoseMatching::OnProcessDuringCompilation(
	IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{

}
#endif

//void UAnimGraphNode_MultiPoseMatching::SetAnimationAsset(UAnimationAsset * Asset)
//{
//	if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(Asset))
//	{
//		Node.Sequence = Seq;
//	}
//}

FText UAnimGraphNode_MultiPoseMatching::GetTitleGivenAssetInfo(const FText & AssetName, bool bKnownToBeAdditive)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), AssetName);

	return FText::Format(LOCTEXT("MultiPoseMatchNodeTitle", "Multi Pose Matching \n {AssetName}"), Args);
}

FText UAnimGraphNode_MultiPoseMatching::GetNodeTitleForSequence(ENodeTitleType::Type TitleType, UAnimSequenceBase * InSequence) const
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
			return FText::Format(LOCTEXT("PoseMatchNodeGroupWithSubtitleFull", "{Title}\nSync group {SyncGroup}"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("PoseMatchNodeGroupWithSubtitleList", "{Title} (Sync group {SyncGroup})"), Args);
		}
	}
}

bool UAnimGraphNode_MultiPoseMatching::HasValidAnimations() const
{
	if (Node.Animations.Num() > 0)
	{
		for (UAnimSequence* Sequence : Node.Animations)
		{
			if (Sequence && Sequence->IsValidToPlay())
			{
				return true;
			}
		}

		return false;
	}

	return false;
}

FString UAnimGraphNode_MultiPoseMatching::GetControllerDescription() const
{
	return TEXT("Multi Pose Matching Animation Node");
}

//void UAnimGraphNode_MultiPoseMatching::CreateOutputPins()
//{
//	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
//	CreatePin(EGPD_Output, Schema->PC_Struct, TEXT(""), FPoseLink::StaticStruct(), /*bIsArray=*/ false, /*bIsReference=*/ false, TEXT("Pose"));
//}



void UAnimGraphNode_MultiPoseMatching::ValidateAnimNodeDuringCompilation(USkeleton * ForSkeleton, FCompilerResultsLog & MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);

	TArray<UAnimSequence*> SequencesToCheck;
	SequencesToCheck.Empty(Node.Animations.Num());

	for (UAnimSequence* Sequence : Node.Animations)
	{
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

void UAnimGraphNode_MultiPoseMatching::PreloadRequiredAssets()
{
	for (UAnimSequence* Sequence : Node.Animations)
	{
		if(Sequence != nullptr)
		{
			PreloadObject(Sequence);
		}
	}

	Super::PreloadRequiredAssets();
}

void UAnimGraphNode_MultiPoseMatching::BakeDataDuringCompilation(FCompilerResultsLog & MessageLog)
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

void UAnimGraphNode_MultiPoseMatching::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	for (UAnimSequence* Sequence : Node.Animations)
	{
		if(Sequence != nullptr)
		{
			HandleAnimReferenceCollection(Sequence, AnimationAssets);
		}
	}

	if (Node.Sequence)
	{
		HandleAnimReferenceCollection(Node.Sequence, AnimationAssets);
	}
}

void UAnimGraphNode_MultiPoseMatching::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	for (int32 i = 0; i < Node.Animations.Num(); ++i)
	{
		UAnimSequence* CacheOriginalAsset = Node.Animations[i];

		if (UAnimationAsset* const* ReplacementAsset = AnimAssetReplacementMap.Find(CacheOriginalAsset))
		{
			Node.Animations[i] = Cast<UAnimSequence>(*ReplacementAsset);
		}
	}

	HandleAnimReferenceReplacement(Node.Sequence, AnimAssetReplacementMap);
}

bool UAnimGraphNode_MultiPoseMatching::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset * UAnimGraphNode_MultiPoseMatching::GetAnimationAsset() const
{
	if(Node.Animations.Num() == 0)
		return nullptr;

	return Node.Animations[0];
}

const TCHAR* UAnimGraphNode_MultiPoseMatching::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_MultiPoseMatching::GetTimePropertyStruct() const
{
	return FAnimNode_MultiPoseMatching::StaticStruct();
}

void UAnimGraphNode_MultiPoseMatching::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_MultiPoseMatching, PlayRate))
	{
		if (!Pin->bHidden)
		{
			// Draw value for PlayRateBasis if the pin is not exposed
			UEdGraphPin* PlayRateBasisPin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_MultiPoseMatching, PlayRateBasis));
			if (!PlayRateBasisPin || PlayRateBasisPin->bHidden)
			{
				if (Node.PlayRateBasis != 1.f)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
					Args.Add(TEXT("PlayRateBasis"), FText::AsNumber(Node.PlayRateBasis));
					Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_MultiPoseMatching_PlayRateBasis_Value", "({PinFriendlyName} / {PlayRateBasis})"), Args);
				}
			}
			else // PlayRateBasisPin is visible
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
				Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_MultiPoseMatching_PlayRateBasis_Name", "({PinFriendlyName} / PlayRateBasis)"), Args);
			}

			Pin->PinFriendlyName = Node.PlayRateScaleBiasClamp.GetFriendlyName(Pin->PinFriendlyName);
		}
	}
}

void UAnimGraphNode_MultiPoseMatching::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);

	// Reconstruct node to show updates to PinFriendlyNames.
	if ((PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_MultiPoseMatching, PlayRateBasis))
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

void UAnimGraphNode_MultiPoseMatching::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
	NodeSpawner->DefaultMenuSignature.MenuName = FText::FromString(TEXT("Multi Pose Matching"));
	NodeSpawner->DefaultMenuSignature.Tooltip = FText::FromString(TEXT("Special sequence player which uses pose matching to pick the animation to use and its starting point"));
	ActionRegistrar.AddBlueprintAction(NodeSpawner);
}

#undef LOCTEXT_NAMESPACE
