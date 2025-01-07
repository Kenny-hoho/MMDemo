// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "RMFixToolEditor.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Interfaces/IPluginManager.h"
#include "Framework/Application/SlateApplication.h"
#include "TickableEditorObject.h"
#include "EditorReimportHandler.h"
#include "IPersonaToolkit.h"
#include "IPersonaPreviewScene.h"
#include "PersonaModule.h"
#include "DetailLayoutBuilder.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "ContentBrowserModule.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimSequence.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "BoneSelectionWidget.h"
#include <ISkeletonEditorModule.h>

#define LOCTEXT_NAMESPACE "FRMFixToolEditorModule"

typedef IAnimationDataModel DataModelType;

const float slotPadding = 8.f;
const float contentPadding = 12.f;
const float internalPadding = 2.f;

void SetFrames(UAnimSequence* target, const UAnimSequence* source)
{
	IAnimationDataController& Controller = target->GetController();

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ScopedBracket_SetFrames", "SetFrames"));

	DataModelType* targetModel = target->GetDataModel();

	const int32 targetNum = targetModel->GetNumberOfKeys();

	const DataModelType* sourceModel = source->GetDataModel();

	TArray<FName> TrackNames;
	sourceModel->GetBoneTrackNames(TrackNames);

	TArray<FTransform> BoneTransforms;
	const int32 Num = sourceModel->GetNumberOfKeys();
	BoneTransforms.Reserve(Num);

	TArray<FVector> PosKeys;
	PosKeys.SetNum(Num);
	TArray<FQuat> RotKeys;
	RotKeys.SetNum(Num);
	TArray<FVector> ScaleKeys;
	ScaleKeys.SetNum(Num);

	for (const FName& TrackName : TrackNames)
	{
		BoneTransforms.Reset();
		sourceModel->GetBoneTrackTransforms(TrackName, BoneTransforms);

		for (int32 TransformIndex = 0; TransformIndex < Num; ++TransformIndex)
		{
			PosKeys[TransformIndex] = BoneTransforms[TransformIndex].GetLocation();
			RotKeys[TransformIndex] = BoneTransforms[TransformIndex].GetRotation();
			ScaleKeys[TransformIndex] = BoneTransforms[TransformIndex].GetScale3D();
		}

		Controller.SetBoneTrackKeys(TrackName, PosKeys, RotKeys, ScaleKeys);
	}

	if (targetNum != Num)
	{
		Controller.ResizeInFrames(Num - 1, 0, Num - 1);
	}
}

void RemoveRootMotion(UAnimSequence* animSequence, const FName rootBoneName)
{
	IAnimationDataController& Controller = animSequence->GetController();

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ScopedBracket_RemoveRootMotion", "RemoveRootMotion"));

	DataModelType* Model = animSequence->GetDataModel();

	const int32 Num = Model->GetNumberOfKeys();

	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	if (TrackNames.Contains(rootBoneName))
	{
		TArray<FTransform> transforms;
		transforms.Reserve(Num);

		Model->GetBoneTrackTransforms(rootBoneName, transforms);

		TArray<FVector> PosKeys;
		PosKeys.SetNum(Num);
		TArray<FQuat> RotKeys;
		RotKeys.SetNum(Num);
		TArray<FVector> ScaleKeys;
		ScaleKeys.SetNum(Num);

		for (size_t i = 0; i < Num; i++)
		{
			PosKeys[i] = FVector::ZeroVector;
			RotKeys[i] = transforms[i].GetRotation();
			ScaleKeys[i] = transforms[i].GetScale3D();
		}

		Controller.SetBoneTrackKeys(rootBoneName, PosKeys, RotKeys, ScaleKeys);
	}
}

void ClearRootZeroFrameRotation(UAnimSequence* animSequence, const FName rootBoneName, const FReferenceSkeleton& RefSkeleton)
{
	IAnimationDataController& Controller = animSequence->GetController();

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ScopedBracket_ClearRootZeroFrameRotation", "ClearRootZeroFrameRotation"));

	DataModelType* Model = animSequence->GetDataModel();

	const int32 Num = Model->GetNumberOfKeys();

	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	if (TrackNames.Contains(rootBoneName))
	{
		TArray<FTransform> transforms;
		transforms.Reserve(Num);

		Model->GetBoneTrackTransforms(rootBoneName, transforms);

		const TArray<FTransform>& RefSkeletonPose = RefSkeleton.GetRefBonePose();

		TArray<FVector> PosKeys;
		PosKeys.SetNum(Num);
		TArray<FQuat> RotKeys;
		RotKeys.SetNum(Num);
		TArray<FVector> ScaleKeys;
		ScaleKeys.SetNum(Num);

		const FQuat deltaRot = RefSkeletonPose[0].GetRotation() * transforms[0].GetRotation().Inverse();

		PosKeys[0] = transforms[0].GetLocation();
		RotKeys[0] = RefSkeletonPose[0].GetRotation();
		ScaleKeys[0] = transforms[0].GetScale3D();

		const double customAngleRad = FMath::DegreesToRadians(deltaRot.Rotator().Yaw);
		const double cosPhiRad = FMath::Cos(customAngleRad);
		const double sinPhiRad = FMath::Sin(customAngleRad);

		for (size_t i = 1; i < Num; i++)
		{
			FVector boneLocationNew;
			boneLocationNew.X = transforms[i].GetLocation().X * cosPhiRad - transforms[i].GetLocation().Y * sinPhiRad;
			boneLocationNew.Y = transforms[i].GetLocation().X * sinPhiRad + transforms[i].GetLocation().Y * cosPhiRad;
			boneLocationNew.Z = transforms[i].GetLocation().Z;

			PosKeys[i] = boneLocationNew;
			RotKeys[i] = deltaRot * transforms[i].GetRotation();
			ScaleKeys[i] = transforms[i].GetScale3D();
		}

		Controller.SetBoneTrackKeys(rootBoneName, PosKeys, RotKeys, ScaleKeys);
	}
}

void ClearZeroFrameOffset(UAnimSequence* animSequence, const TMap<FGuid, FName>& entries, const TMap<FGuid, UE::Math::TIntVector3<bool>>& flags)
{
	IAnimationDataController& Controller = animSequence->GetController();

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ScopedBracket_ClearZeroFrameOffset", "ClearZeroFrameOffset"));

	DataModelType* Model = animSequence->GetDataModel();

	const int32 Num = Model->GetNumberOfKeys();

	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	for (const TPair<FGuid, FName>& entry : entries)
	{
		if (flags.Contains(entry.Key))
		{
			if (TrackNames.Contains(entry.Value))
			{
				TArray<FTransform> transforms;
				transforms.Reserve(Num);

				Model->GetBoneTrackTransforms(entry.Value, transforms);

				TArray<FVector> PosKeys;
				PosKeys.SetNum(Num);
				TArray<FQuat> RotKeys;
				RotKeys.SetNum(Num);
				TArray<FVector> ScaleKeys;
				ScaleKeys.SetNum(Num);

				FVector zeroFrameLocation = transforms[0].GetLocation();
				if (!flags[entry.Key].X)
				{
					zeroFrameLocation.X = 0;
				}
				if (!flags[entry.Key].Y)
				{
					zeroFrameLocation.Y = 0;
				}
				if (!flags[entry.Key].Z)
				{
					zeroFrameLocation.Z = 0;
				}

				for (size_t i = 0; i < Num; i++)
				{
					PosKeys[i] = transforms[i].GetLocation() - zeroFrameLocation;
					RotKeys[i] = transforms[i].GetRotation();
					ScaleKeys[i] = transforms[i].GetScale3D();
				}

				Controller.SetBoneTrackKeys(entry.Value, PosKeys, RotKeys, ScaleKeys);
			}
		}
	}
}

void AddOffset(UAnimSequence* animSequence, const TMap<FGuid, FName>& entries, const TMap<FGuid, FVector>& values)
{
	IAnimationDataController& Controller = animSequence->GetController();

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ScopedBracket_AddOffset", "AddOffset"));

	DataModelType* Model = animSequence->GetDataModel();

	const int32 Num = Model->GetNumberOfKeys();

	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	for (const TPair<FGuid, FName>& entry : entries)
	{
		if (values.Contains(entry.Key) && !values[entry.Key].IsNearlyZero())
		{
			if (TrackNames.Contains(entry.Value))
			{
				TArray<FTransform> transforms;
				transforms.Reserve(Num);

				Model->GetBoneTrackTransforms(entry.Value, transforms);

				TArray<FVector> PosKeys;
				PosKeys.SetNum(Num);
				TArray<FQuat> RotKeys;
				RotKeys.SetNum(Num);
				TArray<FVector> ScaleKeys;
				ScaleKeys.SetNum(Num);

				for (size_t i = 0; i < Num; i++)
				{
					PosKeys[i] = transforms[i].GetLocation() + values[entry.Key];
					RotKeys[i] = transforms[i].GetRotation();
					ScaleKeys[i] = transforms[i].GetScale3D();
				}

				Controller.SetBoneTrackKeys(entry.Value, PosKeys, RotKeys, ScaleKeys);
			}
		}
	}
}

void CalculateHierarchyTransforms(const DataModelType* Model, const int32 j, const TArray<FName>& hierarchy, TArray<FTransform>& hierarchyTransforms, TArray<FTransform>& localTransforms, TArray<FTransform>& parentCompSpaceTransforms)
{
	for (size_t i = 0; i < hierarchy.Num(); i++)
	{
		hierarchyTransforms[i] = Model->GetBoneTrackTransform(hierarchy[i], j);
	}

	localTransforms[j] = hierarchyTransforms[0];
	
	parentCompSpaceTransforms[j] = FTransform::Identity;
	
	if (hierarchy.Num() > 0)
	{
		for (int32 i = hierarchy.Num() - 1; i > 0; i--)
		{
			parentCompSpaceTransforms[j] = hierarchyTransforms[i] * parentCompSpaceTransforms[j];
		}
	}
}

void CalculateHierarchyTransforms(const FName snapBoneName, const FName targetBoneName, const TMap<int32, FName>& indexNameMapping, const FReferenceSkeleton& RefSkeleton, const DataModelType* Model, const int32 Num
	, TArray<FTransform>& snapLocalTransforms, TArray<FTransform>& snapParentCompSpaceTransforms, FTransform& snapParentRefTransform
	, TArray<FTransform>& targetLocalTransforms, TArray<FTransform>& targetParentCompSpaceTransforms, FTransform& targetParentRefTransform, TArray<FTransform>& commonParentCompSpaceTransforms)
{
	FName commonParentBoneName = indexNameMapping[0]; // Root Bone Name by default

	TArray<FName> hierarchy;
	FName currentBoneName;

	const TArray<FTransform>& RefSkeletonPose = RefSkeleton.GetRefBonePose();

	// Snap

	snapParentRefTransform = FTransform::Identity;

	hierarchy.Empty();
	
	currentBoneName = snapBoneName;
	while (currentBoneName != NAME_None)
	{
		hierarchy.Add(currentBoneName); // boneName, parentBoneName, parentParentBoneName, ...
		const int32 parentBoneIndex = RefSkeleton.GetParentIndex(RefSkeleton.FindBoneIndex(currentBoneName));
		currentBoneName = indexNameMapping.Contains(parentBoneIndex) ? indexNameMapping[parentBoneIndex] : NAME_None;
		snapParentRefTransform = snapParentRefTransform * (indexNameMapping.Contains(parentBoneIndex) ? RefSkeletonPose[parentBoneIndex].GetRotation() : FQuat::Identity);
	}

	if (hierarchy.Contains(targetBoneName))
	{
		commonParentBoneName = targetBoneName;
	}

	TArray<FTransform> hierarchyTransforms;
	hierarchyTransforms.SetNum(hierarchy.Num());
	
	snapLocalTransforms.SetNum(Num);	
	snapParentCompSpaceTransforms.SetNum(Num);

	for (int32 j = 0; j < Num; j++)
	{
		CalculateHierarchyTransforms(Model, j, hierarchy, hierarchyTransforms, snapLocalTransforms, snapParentCompSpaceTransforms);
	}

	// Target

	targetParentRefTransform = FTransform::Identity;

	hierarchy.Empty();

	currentBoneName = targetBoneName;
	while (currentBoneName != NAME_None)
	{
		hierarchy.Add(currentBoneName); // boneName, parentBoneName, parentParentBoneName, ...
		const int32 parentBoneIndex = RefSkeleton.GetParentIndex(RefSkeleton.FindBoneIndex(currentBoneName));
		currentBoneName = indexNameMapping.Contains(parentBoneIndex) ? indexNameMapping[parentBoneIndex] : NAME_None;
		targetParentRefTransform = targetParentRefTransform * (indexNameMapping.Contains(parentBoneIndex) ? RefSkeletonPose[parentBoneIndex].GetRotation() : FQuat::Identity);
	}

	if (hierarchy.Contains(snapBoneName))
	{
		commonParentBoneName = snapBoneName;
	}
	
	hierarchyTransforms.SetNum(hierarchy.Num());

	targetLocalTransforms.SetNum(Num);
	targetParentCompSpaceTransforms.SetNum(Num);
	
	for (int32 j = 0; j < Num; j++)
	{
		CalculateHierarchyTransforms(Model, j, hierarchy, hierarchyTransforms, targetLocalTransforms, targetParentCompSpaceTransforms);
	}

	if (commonParentBoneName != indexNameMapping[0])
	{
		hierarchy.Empty();

		currentBoneName = targetBoneName;
		while (currentBoneName != NAME_None)
		{
			hierarchy.Add(currentBoneName); // boneName, parentBoneName, parentParentBoneName, ...
			const int32 parentBoneIndex = RefSkeleton.GetParentIndex(RefSkeleton.FindBoneIndex(currentBoneName));
			currentBoneName = indexNameMapping.Contains(parentBoneIndex) ? indexNameMapping[parentBoneIndex] : NAME_None;
		}

		hierarchyTransforms.SetNum(hierarchy.Num());

		TArray<FTransform> commonLocalTransforms;
		commonLocalTransforms.SetNum(Num);
		commonParentCompSpaceTransforms.SetNum(Num);

		for (int32 j = 0; j < Num; j++)
		{
			CalculateHierarchyTransforms(Model, j, hierarchy, hierarchyTransforms, commonLocalTransforms, commonParentCompSpaceTransforms);
		}
	}
}

void TransferAnimation(UAnimSequence* animSequence, const FName snapBoneName, const FName targetBoneName, const bool clearZeroFrameOffset, const UE::Math::TIntVector3<bool>& locationFlags, const UE::Math::TIntVector3<bool>& rotationFlags, const TMap<int32, FName>& indexNameMapping, const FReferenceSkeleton& RefSkeleton)
{
	IAnimationDataController& Controller = animSequence->GetController();

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ScopedBracket_AddOffset", "AddOffset"));

	DataModelType* Model = animSequence->GetDataModel();

	const int32 Num = Model->GetNumberOfKeys();

	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	if (TrackNames.Contains(snapBoneName) && TrackNames.Contains(targetBoneName) && (!locationFlags.IsZero() || !rotationFlags.IsZero()))
	{
		TArray<FTransform> snapBoneLocalTransforms;
		TArray<FTransform> snapBoneParentCompSpaceTransforms;
		FTransform snapParentRefTransform;
		TArray<FTransform> targetBoneLocalTransforms;
		TArray<FTransform> targetBoneParentCompSpaceTransforms;
		FTransform targetParentRefTransform;
		TArray<FTransform> commonParentCompSpaceTransforms;

		CalculateHierarchyTransforms(snapBoneName, targetBoneName, indexNameMapping, RefSkeleton, Model, Num
			, snapBoneLocalTransforms, snapBoneParentCompSpaceTransforms, snapParentRefTransform
			, targetBoneLocalTransforms, targetBoneParentCompSpaceTransforms, targetParentRefTransform, commonParentCompSpaceTransforms);
		
		FVector locationMask;
		locationMask.X = locationFlags.X ? 1 : 0;
		locationMask.Y = locationFlags.Y ? 1 : 0;
		locationMask.Z = locationFlags.Z ? 1 : 0;

		const TArray<FTransform>& RefSkeletonPose = RefSkeleton.GetRefBonePose();
		const int32 targetBoneIndex = RefSkeleton.GetParentIndex(RefSkeleton.FindBoneIndex(targetBoneName));
		const FVector zeroFrameOffset = RefSkeletonPose.IsValidIndex(targetBoneIndex) ? (clearZeroFrameOffset ? ((RefSkeletonPose[targetBoneIndex] * targetParentRefTransform).GetTranslation() - (targetBoneLocalTransforms[0] * targetBoneParentCompSpaceTransforms[0]).GetTranslation()) : FVector::ZeroVector) * locationMask : FVector::ZeroVector;

		// Calculate Deltas

		TArray<FVector> deltaLocations;
		deltaLocations.SetNum(Num);
		TArray<FRotator> deltaRotations;
		deltaRotations.SetNum(Num);

		for (int32 i = 1; i < Num; i++)
		{
			const FTransform prevTransform = targetBoneLocalTransforms[i - 1] * targetBoneParentCompSpaceTransforms[i - 1];
			const FTransform nextTransform = targetBoneLocalTransforms[i] * targetBoneParentCompSpaceTransforms[i];

			deltaLocations[i] = nextTransform.GetTranslation() - prevTransform.GetTranslation() - (targetBoneParentCompSpaceTransforms[i].GetTranslation() - targetBoneParentCompSpaceTransforms[i - 1].GetTranslation());
			deltaRotations[i] = (nextTransform.GetRotation() * prevTransform.GetRotation().Inverse() * (targetBoneParentCompSpaceTransforms[i - 1].GetRotation() * targetBoneParentCompSpaceTransforms[i].GetRotation().Inverse())).Rotator();
		}

		TArray<FVector> PosKeys;
		PosKeys.SetNum(Num);
		TArray<FQuat> RotKeys;
		RotKeys.SetNum(Num);
		TArray<FVector> ScaleKeys;
		ScaleKeys.SetNum(Num);

		// Calculate Target Bone

		PosKeys[0] = targetBoneLocalTransforms[0].GetLocation() + (FTransform(zeroFrameOffset) * targetBoneParentCompSpaceTransforms[0].Inverse()).GetTranslation();
		RotKeys[0] = targetBoneLocalTransforms[0].GetRotation();
		ScaleKeys[0] = targetBoneLocalTransforms[0].GetScale3D();

		for (int32 i = 1; i < Num; i++)
		{
			FVector deltaLocation = deltaLocations[i] * (FVector::OneVector - locationMask);

			FRotator deltaRotation = deltaRotations[i];
			deltaRotation.Roll *= rotationFlags.X ? 0 : 1;
			deltaRotation.Pitch *= rotationFlags.Y ? 0 : 1;
			deltaRotation.Yaw *= rotationFlags.Z ? 0 : 1;

			FTransform parentCompSpaceTransformNoTranslation(targetBoneParentCompSpaceTransforms[i].GetRotation(), FVector::ZeroVector, targetBoneParentCompSpaceTransforms[i].GetScale3D());
			const FTransform deltaTransform = FTransform(deltaLocation) * parentCompSpaceTransformNoTranslation.Inverse();

			PosKeys[i] = PosKeys[i - 1] + deltaTransform.GetLocation();
			RotKeys[i] = rotationFlags.IsZero()
				? targetBoneLocalTransforms[i].GetRotation()
				: targetParentRefTransform.GetRotation().Inverse() * (deltaRotation.Quaternion() * RotKeys[i - 1] * targetBoneParentCompSpaceTransforms[i - 1].GetRotation()) * targetBoneParentCompSpaceTransforms[i].GetRotation().Inverse() * targetParentRefTransform.GetRotation();
			ScaleKeys[i] = targetBoneLocalTransforms[i].GetScale3D();
		}

		Controller.SetBoneTrackKeys(targetBoneName, PosKeys, RotKeys, ScaleKeys);

		CalculateHierarchyTransforms(snapBoneName, targetBoneName, indexNameMapping, RefSkeleton, Model, Num
			, snapBoneLocalTransforms, snapBoneParentCompSpaceTransforms, snapParentRefTransform
			, targetBoneLocalTransforms, targetBoneParentCompSpaceTransforms, targetParentRefTransform, commonParentCompSpaceTransforms);

		// Calculate Snap Bone

		PosKeys[0] = snapBoneLocalTransforms[0].GetLocation() - (FTransform(zeroFrameOffset) * snapBoneParentCompSpaceTransforms[0].Inverse()).GetTranslation();
		RotKeys[0] = snapBoneLocalTransforms[0].GetRotation();
		ScaleKeys[0] = snapBoneLocalTransforms[0].GetScale3D();

		FQuat deltaQuat = FQuat::Identity;

		for (int32 i = 1; i < Num; i++)
		{
			FVector deltaLocation = deltaLocations[i] * locationMask;

			FRotator deltaRotation = deltaRotations[i];
			deltaRotation.Roll *= rotationFlags.X ? 1 : 0;
			deltaRotation.Pitch *= rotationFlags.Y ? 1 : 0;
			deltaRotation.Yaw *= rotationFlags.Z ? 1 : 0;

			deltaQuat *= deltaRotation.Quaternion();

			FTransform parentCompSpaceTransformNoTranslation(snapBoneParentCompSpaceTransforms[i].GetRotation(), FVector::ZeroVector, snapBoneParentCompSpaceTransforms[i].GetScale3D());
			const FTransform deltaTransform = FTransform(deltaLocation) * parentCompSpaceTransformNoTranslation.Inverse();

			PosKeys[i] = PosKeys[i - 1] + snapBoneLocalTransforms[i].GetLocation() - snapBoneLocalTransforms[i - 1].GetLocation() + deltaTransform.GetLocation();
            RotKeys[i] = rotationFlags.IsZero()
				? snapBoneLocalTransforms[i].GetRotation()
				: snapParentRefTransform.GetRotation().Inverse() * deltaQuat * snapParentRefTransform.GetRotation() * snapBoneLocalTransforms[i].GetRotation();
			ScaleKeys[i] = snapBoneLocalTransforms[i].GetScale3D();
		}

		Controller.SetBoneTrackKeys(snapBoneName, PosKeys, RotKeys, ScaleKeys);
	}
}

void Snap(UAnimSequence* animSequence, const TMap<FGuid, FName>& snapSources, const TMap<FGuid, FName>& snapTargets, const TMap<int32, FName>& indexNameMapping, const FReferenceSkeleton& RefSkeleton)
{
	IAnimationDataController& Controller = animSequence->GetController();

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ScopedBracket_AddOffset", "AddOffset"));

	DataModelType* Model = animSequence->GetDataModel();

	const int32 Num = Model->GetNumberOfKeys();

	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	for (const TPair<FGuid, FName>& snapSource : snapSources)
	{
		if (snapSource.Value != NAME_None && snapTargets.Contains(snapSource.Key) && snapTargets[snapSource.Key] != NAME_None)
		{
			const FName snapBoneName = snapSource.Value;
			const FName targetBoneName = snapTargets[snapSource.Key];

			if (snapBoneName == targetBoneName) continue;

			if (TrackNames.Contains(snapBoneName) && TrackNames.Contains(targetBoneName))
			{
				TArray<FTransform> snapBoneLocalTransforms;
				TArray<FTransform> snapBoneParentCompSpaceTransforms;
				FTransform snapParentRefTransform;
				TArray<FTransform> targetBoneLocalTransforms;
				TArray<FTransform> targetBoneParentCompSpaceTransforms;
				FTransform targetParentRefTransform;
				TArray<FTransform> commonParentCompSpaceTransforms;
				
				CalculateHierarchyTransforms(snapBoneName, targetBoneName, indexNameMapping, RefSkeleton, Model, Num
					, snapBoneLocalTransforms, snapBoneParentCompSpaceTransforms, snapParentRefTransform
					, targetBoneLocalTransforms, targetBoneParentCompSpaceTransforms, targetParentRefTransform, commonParentCompSpaceTransforms);

				TArray<FVector> PosKeys;
				PosKeys.SetNum(Num);
				TArray<FQuat> RotKeys;
				RotKeys.SetNum(Num);
				TArray<FVector> ScaleKeys;
				ScaleKeys.SetNum(Num);

				for (int32 i = 0; i < Num; i++)
				{
					const FTransform transform = targetBoneLocalTransforms[i] * targetBoneParentCompSpaceTransforms[i] * snapBoneParentCompSpaceTransforms[i].Inverse();

					PosKeys[i] = transform.GetLocation();
					RotKeys[i] = transform.GetRotation();
					ScaleKeys[i] = transform.GetScale3D();
				}

				Controller.SetBoneTrackKeys(snapBoneName, PosKeys, RotKeys, ScaleKeys);
			}
		}
	}
}

void FixRootMotionDirection(UAnimSequence* animSequence, const UE::Math::TIntVector3<bool>& flags, float inCustomAngle, bool rotateChildBones, const FName rootBoneName, const TMap<int32, FName>& indexNameMapping, const FReferenceSkeleton& RefSkeleton)
{
	IAnimationDataController& Controller = animSequence->GetController();

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("ScopedBracket_FixRootMotionDirection", "FixRootMotionDirection"));

	DataModelType* Model = animSequence->GetDataModel();

	const int32 Num = Model->GetNumberOfKeys();

	TArray<FName> TrackNames;
	Model->GetBoneTrackNames(TrackNames);

	if (TrackNames.Contains(rootBoneName))
	{
		TArray<FTransform> rootBoneTransforms;
		rootBoneTransforms.Reserve(Num);
		Model->GetBoneTrackTransforms(rootBoneName, rootBoneTransforms);

		FVector direction = Model->GetBoneTrackTransform(rootBoneName, Num - 1).GetLocation();
		direction.Z = 0;

		float customAngleRad = FMath::DegreesToRadians(inCustomAngle);
		float customAngle = inCustomAngle;

		if (!direction.IsNearlyZero())
		{
			if (flags.X || flags.Y)
			{
				direction = direction.GetSafeNormal2D();

				customAngleRad = FMath::Acos(direction.X);

				if (customAngleRad < 0 && direction.Y > 0 || customAngleRad > 0 && direction.Y < 0)
				{
					customAngleRad = -customAngleRad;
				}

				if (flags.Y)
				{
					customAngleRad = customAngleRad - UE_DOUBLE_PI / 2;
				}

				customAngleRad = -customAngleRad;
				customAngle = FMath::RadiansToDegrees(customAngleRad);
			}
		}
		else
		{
			customAngleRad = 0;
			customAngle = 0;
		}

		if (!FMath::IsNearlyZero(customAngle))
		{
			TArray<FVector> PosKeys;
			PosKeys.SetNum(Num);
			TArray<FQuat> RotKeys;
			RotKeys.SetNum(Num);
			TArray<FVector> ScaleKeys;
			ScaleKeys.SetNum(Num);

			FQuat rotation = FRotator(0, customAngle, 0).Quaternion();

			const double cosPhiRad = FMath::Cos(customAngleRad);
			const double sinPhiRad = FMath::Sin(customAngleRad);

			for (int32 i = 0; i < Num; i++)
			{
				FVector boneLocationNew;
				boneLocationNew.X = rootBoneTransforms[i].GetLocation().X * cosPhiRad - rootBoneTransforms[i].GetLocation().Y * sinPhiRad;
				boneLocationNew.Y = rootBoneTransforms[i].GetLocation().X * sinPhiRad + rootBoneTransforms[i].GetLocation().Y * cosPhiRad;
				boneLocationNew.Z = rootBoneTransforms[i].GetLocation().Z;

				rootBoneTransforms[i].SetLocation(boneLocationNew);

				PosKeys[i] = rootBoneTransforms[i].GetLocation();
				RotKeys[i] = rootBoneTransforms[i].GetRotation();
				ScaleKeys[i] = rootBoneTransforms[i].GetScale3D();
			}

			Controller.SetBoneTrackKeys(rootBoneName, PosKeys, RotKeys, ScaleKeys);

			if (rotateChildBones)
			{
				TArray<int32> childBonesIndexes;
				RefSkeleton.GetDirectChildBones(0, childBonesIndexes);

				for (const int32 childBonesIndex : childBonesIndexes)
				{
					if (indexNameMapping.Contains(childBonesIndex))
					{
						const FName childBoneName = indexNameMapping[childBonesIndex];

						if (TrackNames.Contains(childBoneName))
						{
							TArray<FTransform> childBoneTransforms;
							childBoneTransforms.Reserve(Num);
							Model->GetBoneTrackTransforms(childBoneName, childBoneTransforms);

							for (int32 i = 0; i < Num; i++)
							{
								FTransform transform = childBoneTransforms[i] * FTransform(rotation * rootBoneTransforms[i].GetRotation().Inverse());

								PosKeys[i] = transform.GetLocation();
								RotKeys[i] = transform.GetRotation();
								ScaleKeys[i] = transform.GetScale3D();
							}
						}

						Controller.SetBoneTrackKeys(childBoneName, PosKeys, RotKeys, ScaleKeys);
					}
				}
			}
		}
	}
}

//--------------------------------------------------------------------
// IRMFixToolBase
//--------------------------------------------------------------------

class IRMFixToolBase
{
public:
	IRMFixToolBase() { Reset(); ModificationHash = CalculateModificationHash(); }

	virtual ~IRMFixToolBase() { CleanCopies(); }

	void CleanCopies()
	{
		if (AnimSequence)
		{
			AnimSequence->RemoveFromRoot();
		}

		if (AnimSequenceSafe)
		{
			AnimSequenceSafe->RemoveFromRoot();
		}
	}

	void ApplyModifiers(UAnimSequence* animSequence, const TMap<const USkeleton*, FBoneContainer>& BoneContainers, const TMap<FName, int32>& nameIndexMapping, const TMap<int32, FName>& indexNameMapping, const FReferenceSkeleton RefSkeleton, const FName rootBoneName) const
	{
		if (bRemoveRootMotion)
		{
			RemoveRootMotion(animSequence, rootBoneName);
		}

		if (bClearRootZeroFrameRotation)
		{
			ClearRootZeroFrameRotation(animSequence, rootBoneName, RefSkeleton);
		}

		if (bClearZeroFrameOffset && ClearZeroFrameOffsetEntries.Num() > 0)
		{
			ClearZeroFrameOffset(animSequence, ClearZeroFrameOffsetEntries, ClearZeroFrameOffsetFlags);
		}

		if (bAddOffset && AddOffsetEntries.Num() > 0)
		{
			AddOffset(animSequence, AddOffsetEntries, AddOffsetValues);
		}

		if (bTransferAnimation)
		{
			TransferAnimation(animSequence, TransferAnimation_SourceBone, TransferAnimation_TargetBone, bTransferAnimationClearZeroFrameOffset, TransferAnimation_LocationFlags, TransferAnimation_RotationFlags, indexNameMapping, GetReferenceSkeleton());
		}

		if (bSnap)
		{
			Snap(animSequence, SnapSources, SnapTargets, indexNameMapping, GetReferenceSkeleton());
		}

		if (bFixRootMotionDirection)
		{
			FixRootMotionDirection(animSequence, FixRootMotionDirectionFlags, CustomAngle, bRotateChildBones, rootBoneName, indexNameMapping, GetReferenceSkeleton());
		}
	}

	template<class TCancelPredicate, class TPreDelegate, class TPostDelegate>
	void ApplyModifiersToCollection(TArray<UAnimSequence*> animSequences, TCancelPredicate cancelPredicate, TPreDelegate preDelegate, TPostDelegate postDelegate) const
	{
		TMap<const USkeleton*, UMirrorDataTable*> MirrorDataTables;
		TMap<const USkeleton*, FBoneContainer> BoneContainers;

		for (UAnimSequence* animSequence : animSequences)
		{
			if (cancelPredicate()) break;

			preDelegate();

			USkeleton* nonConstSkeleton = animSequence->GetSkeleton();
			const USkeleton* skeleton = nonConstSkeleton;

			const FReferenceSkeleton RefSkeleton = skeleton->GetReferenceSkeleton();
			const int32 refSkeletonNum = RefSkeleton.GetNum();

			const FName rootBoneName = RefSkeleton.GetBoneName(0);

			if (rootBoneName == NAME_None) continue;

			const DataModelType* Model = animSequence->GetDataModel();

			int32 Num = Model->GetNumberOfFrames();

			if (Num == 0) continue;

			if (!BoneContainers.Contains(skeleton))
			{
				FBoneContainer& RequiredBones = BoneContainers.FindOrAdd(skeleton);
				RequiredBones.SetUseRAWData(true);

				TArray<FBoneIndexType> RequiredBoneIndexArray;
				RequiredBoneIndexArray.AddUninitialized(refSkeletonNum);
				for (int32 BoneIndex = 0; BoneIndex < refSkeletonNum; ++BoneIndex)
				{
					RequiredBoneIndexArray[BoneIndex] = BoneIndex;
				}
				RequiredBones.InitializeTo(RequiredBoneIndexArray, UE::Anim::FCurveFilterSettings(), *nonConstSkeleton);
			}

			const int32 NumBones = BoneContainers[skeleton].GetCompactPoseNumBones();

			if (NumBones == 0) continue;

			TMap<FName, int32> nameIndexMapping;
			TMap<int32, FName> indexNameMapping;

			for (FCompactPoseBoneIndex BoneIndex(0); BoneIndex < NumBones; ++BoneIndex)
			{
				FName boneName = RefSkeleton.GetBoneName(BoneIndex.GetInt());
				nameIndexMapping.FindOrAdd(boneName, BoneIndex.GetInt());
				indexNameMapping.FindOrAdd(BoneIndex.GetInt(), boneName);
			}

			ApplyModifiers(animSequence, BoneContainers, nameIndexMapping, indexNameMapping, RefSkeleton, rootBoneName);

			postDelegate();
		}
	}

	void ApplyModifiers()
	{
		FScopedSlowTask scopedSlowTask(100, LOCTEXT("ScopedSlowTaskMsg", "Modifying animation assets..."));
		scopedSlowTask.MakeDialog(true);  // We display the Cancel button here

		TArray<UAnimSequence*> sequencesToApply;

		for (UAnimSequence* animSequence : AnimSequences)
		{
			if (AnimSequence->GetSkeleton() == animSequence->GetSkeleton())
			{
				sequencesToApply.Add(animSequence);
			}
		}

		const int32 num = sequencesToApply.Num();

		ApplyModifiersToCollection(sequencesToApply
			, [&scopedSlowTask]() -> bool { return scopedSlowTask.ShouldCancel(); }
		, [&scopedSlowTask, this, num]() { scopedSlowTask.EnterProgressFrame(100.f / num); }
		, []() { FPlatformProcess::SleepNoStats(0.05f); }
		);

		SetFrames(AnimSequence, AnimSequences[0]);
		SetFrames(AnimSequenceSafe, AnimSequences[0]);
	}

	// Remove Root motion

	bool GetRemoveRootMotion() const { return bRemoveRootMotion; }

	void SetRemoveRootMotion(const bool value)
	{
		bRemoveRootMotion = value;

		RefreshPreview();
	}

	// Clear Root zero frame rotation

	bool GetClearRootZeroFrameRotation() const { return bClearRootZeroFrameRotation; }

	void SetClearRootZeroFrameRotation(const bool value)
	{
		bClearRootZeroFrameRotation = value;

		RefreshPreview();
	}

	// Clear zero frame offset

	bool GetClearZeroFrameOffset() const { return bClearZeroFrameOffset; }

	void SetClearZeroFrameOffset(const bool value)
	{
		bClearZeroFrameOffset = value;

		RefreshPreview();
	}

	void AddEntry_ClearZeroFrameOffset(const FGuid& guid)
	{
		ClearZeroFrameOffsetEntries.FindOrAdd(guid);
		ClearZeroFrameOffsetFlags.FindOrAdd(guid);

		RefreshPreview();
	}

	void SetEntry_ClearZeroFrameOffset(const FGuid& guid, const FName name)
	{
		ClearZeroFrameOffsetEntries[guid] = name;

		RefreshPreview();
	}

	FName GetEntry_ClearZeroFrameOffset(const FGuid& guid) const
	{
		return ClearZeroFrameOffsetEntries[guid];
	}

	void SetFlags_ClearZeroFrameOffset(const FGuid& guid, const EAxis::Type axis, const bool value)
	{
		switch (axis)
		{
		case EAxis::X:
			ClearZeroFrameOffsetFlags[guid].X = value;
			break;
		case EAxis::Y:
			ClearZeroFrameOffsetFlags[guid].Y = value;
			break;
		case EAxis::Z:
			ClearZeroFrameOffsetFlags[guid].Z = value;
			break;
		default:
			break;
		}

		RefreshPreview();
	}

	void RemoveEntry_ClearZeroFrameOffset(const FGuid& guid)
	{
		ClearZeroFrameOffsetEntries.Remove(guid);
		ClearZeroFrameOffsetFlags.Remove(guid);

		RefreshPreview();
	}

	// Add offset

	bool GetAddOffset() const { return bAddOffset; }

	void SetAddOffset(const bool value)
	{
		bAddOffset = value;

		RefreshPreview();
	}

	void AddEntry_AddOffset(const FGuid& guid)
	{
		AddOffsetEntries.FindOrAdd(guid);
		AddOffsetValues.FindOrAdd(guid) = FVector::ZeroVector;

		RefreshPreview();
	}

	void SetEntry_AddOffset(const FGuid& guid, const FName name)
	{
		AddOffsetEntries[guid] = name;

		RefreshPreview();
	}

	FName GetEntry_AddOffset(const FGuid& guid) const
	{
		return AddOffsetEntries[guid];
	}

	FVector& SetValue_AddOffset(const FGuid& guid)
	{
		return AddOffsetValues[guid];
	}

	FVector GetValue_AddOffset(const FGuid& guid) const
	{
		return AddOffsetValues[guid];
	}

	void RemoveEntry_AddOffset(const FGuid& guid)
	{
		AddOffsetEntries.Remove(guid);
		AddOffsetValues.Remove(guid);

		RefreshPreview();
	}

	// Transfer Animation

	bool GetTransferAnimation() const { return bTransferAnimation; }

	void SetTransferAnimation(bool value)
	{
		bTransferAnimation = value;

		RefreshPreview();
	}

	FName GetTransferAnimationSourceBone() const { return TransferAnimation_SourceBone; }

	void SetTransferAnimationSourceBone(FName name)
	{
		TransferAnimation_SourceBone = name;

		RefreshPreview();
	}

	FName GetTransferAnimationTargetBone() const { return TransferAnimation_TargetBone; }

	void SetTransferAnimationTargetBone(FName name)
	{
		TransferAnimation_TargetBone = name;

		RefreshPreview();
	}

	bool GetTransferAnimationClearZeroFrameOffset() const { return bTransferAnimationClearZeroFrameOffset; }

	void SetTransferAnimationClearZeroFrameOffset(const bool value)
	{
		bTransferAnimationClearZeroFrameOffset = value;

		RefreshPreview();
	}

	UE::Math::TIntVector3<bool> GetTransferAnimationLocationFlags() const { return TransferAnimation_LocationFlags; }

	UE::Math::TIntVector3<bool>& SetTransferAnimationLocationFlags() { return TransferAnimation_LocationFlags; }

	UE::Math::TIntVector3<bool> GetTransferAnimationRotationFlags() const { return TransferAnimation_RotationFlags; }

	UE::Math::TIntVector3<bool>& SetTransferAnimationRotationFlags() { return TransferAnimation_RotationFlags; }

	// Snap

	bool GetSnap() const { return bSnap; }

	void SetSnap(bool value)
	{
		bSnap = value;

		RefreshPreview();
	}

	bool HasEntry_Snap(const FName source, const FName target)
	{
		for (const TPair<FGuid, FName>& snapSource : SnapSources)
		{
			if (snapSource.Value == source)
			{
				if (SnapTargets.Contains(snapSource.Key))
				{
					if (SnapTargets[snapSource.Key] == target)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	void AddEntry_Snap(const FGuid& guid)
	{
		SnapSources.FindOrAdd(guid);
		SnapTargets.FindOrAdd(guid);

		RefreshPreview();
	}

	void SetEntry_SnapSource(const FGuid& guid, const FName name)
	{
		SnapSources[guid] = name;

		RefreshPreview();
	}

	FName GetEntry_SnapSource(const FGuid& guid) const
	{
		return SnapSources[guid];
	}

	void SetEntry_SnapTarget(const FGuid& guid, const FName name)
	{
		SnapTargets[guid] = name;

		RefreshPreview();
	}

	FName GetEntry_SnapTarget(const FGuid& guid) const
	{
		return SnapTargets[guid];
	}

	void RemoveEntry_Snap(const FGuid& guid)
	{
		SnapSources.Remove(guid);
		SnapTargets.Remove(guid);

		RefreshPreview();
	}

	// Fix Root Motion direction

	bool GetFixRootMotionDirection() const { return bFixRootMotionDirection; }

	void SetFixRootMotionDirection(bool value)
	{
		bFixRootMotionDirection = value;

		RefreshPreview();
	}

	UE::Math::TIntVector3<bool> GetFixRootMotionDirectionFlags() const { return FixRootMotionDirectionFlags; }

	UE::Math::TIntVector3<bool>& SetFixRootMotionDirectionFlags() { return FixRootMotionDirectionFlags; }

	float GetCustomAngle() const { return CustomAngle; }

	void SetCustomAngle(float value) { CustomAngle = value; }

	bool GetRotateChildBones() const { return bRotateChildBones; }

	void SetRotateChildBones(bool value) { bRotateChildBones = value; RefreshPreview(); }

	// Fix Root Motion direction

	bool GetEnableRootMotion() const { return AnimSequence->bEnableRootMotion; }

	void SetEnableRootMotion(bool value)
	{
		AnimSequence->bEnableRootMotion = value;

		RefreshPreview();
	}

	void Reset()
	{
		bRemoveRootMotion = 0;
		bClearRootZeroFrameRotation = 0;
		bClearZeroFrameOffset = 0;

		bAddOffset = 0;

		bTransferAnimation = 0;

		bSnap = 0;

		bFixRootMotionDirection = 0;

		ClearZeroFrameOffsetEntries.Empty();
		ClearZeroFrameOffsetFlags.Empty();

		AddOffsetEntries.Empty();
		AddOffsetValues.Empty();

		TransferAnimation_SourceBone = NAME_None;
		TransferAnimation_TargetBone = NAME_None;
		bTransferAnimationClearZeroFrameOffset = 1;
		TransferAnimation_LocationFlags.X = 1;
		TransferAnimation_LocationFlags.Y = 1;
		TransferAnimation_LocationFlags.Z = 0;
		TransferAnimation_RotationFlags = UE::Math::TIntVector3<bool>::ZeroValue;

		SnapSources.Empty();
		SnapTargets.Empty();

		FixRootMotionDirectionFlags.X = 1;
		FixRootMotionDirectionFlags.Y = 0;
		FixRootMotionDirectionFlags.Z = 0;
		CustomAngle = 0;
		bRotateChildBones = 1;
	}

	bool CanApplyModifiers() const
	{
		bool result = false;

		if (bRemoveRootMotion)
		{
			result |= true;
		}

		if (bClearRootZeroFrameRotation)
		{
			result |= true;
		}

		if (bClearZeroFrameOffset)
		{
			bool canApplyModifier = false;

			for (const TPair<FGuid, FName>& entry : ClearZeroFrameOffsetEntries)
			{
				if (entry.Value != NAME_None && ClearZeroFrameOffsetFlags.Contains(entry.Key) && !ClearZeroFrameOffsetFlags[entry.Key].IsZero())
				{
					canApplyModifier = true;
					break;
				}
			}

			result |= canApplyModifier;
		}

		if (bAddOffset)
		{
			bool canApplyModifier = false;

			for (const TPair<FGuid, FName>& entry : AddOffsetEntries)
			{
				if (entry.Value != NAME_None && AddOffsetValues.Contains(entry.Key) && !AddOffsetValues[entry.Key].IsNearlyZero())
				{
					canApplyModifier = true;
					break;
				}
			}

			result |= canApplyModifier;
		}

		if (bTransferAnimation)
		{
			result |= TransferAnimation_SourceBone != NAME_None && TransferAnimation_TargetBone != NAME_None &&
				(!TransferAnimation_LocationFlags.IsZero() || !TransferAnimation_RotationFlags.IsZero());
		}

		if (bSnap)
		{
			bool canApplyModifier = false;

			for (const TPair<FGuid, FName>& entry : SnapSources)
			{
				if (entry.Value != NAME_None && SnapTargets.Contains(entry.Key) && SnapTargets[entry.Key] != NAME_None)
				{
					canApplyModifier = true;
					break;
				}
			}

			result |= canApplyModifier;
		}

		if (bFixRootMotionDirection)
		{
			result |= !FixRootMotionDirectionFlags.IsZero();
		}

		return result;
	}

	const FReferenceSkeleton& GetReferenceSkeleton() const
	{
		return AnimSequences[0]->GetSkeleton()->GetReferenceSkeleton();
	}

	template<class T>
	uint64 CalculateModificationHash_Collection(T& collection) const
	{
		uint32 result = 0;
		for (auto& item : collection)
		{
			result ^= GetTypeHash(item);
		}
		return result;
	}

	uint64 CalculateModificationHash() const
	{
		int32 i = 0;

		uint32 result = GetTypeHash(bRemoveRootMotion); i++;
		result ^= GetTypeHash(bClearRootZeroFrameRotation); i++;
		result ^= GetTypeHash(bClearZeroFrameOffset); i++;
		if (bClearZeroFrameOffset)
		{
			result ^= CalculateModificationHash_Collection(ClearZeroFrameOffsetEntries); i++;
			result ^= CalculateModificationHash_Collection(ClearZeroFrameOffsetFlags); i++;
		}

		result ^= GetTypeHash(bAddOffset); i++;
		if (bAddOffset)
		{
			result ^= CalculateModificationHash_Collection(AddOffsetEntries); i++;
			result ^= CalculateModificationHash_Collection(AddOffsetValues); i++;
		}

		result ^= GetTypeHash(bTransferAnimation); i++;
		if (bTransferAnimation)
		{
			result ^= GetTypeHash(TransferAnimation_SourceBone); i++;
			result ^= GetTypeHash(TransferAnimation_TargetBone); i++;
			result ^= GetTypeHash(bTransferAnimationClearZeroFrameOffset); i++;
			result ^= GetTypeHash(TransferAnimation_LocationFlags); i++;
			result ^= GetTypeHash(TransferAnimation_RotationFlags); i++;
		}

		result ^= GetTypeHash(bSnap); i++;
		if (bSnap)
		{
			result ^= CalculateModificationHash_Collection(SnapSources); i++;
			result ^= CalculateModificationHash_Collection(SnapTargets); i++;
		}

		result ^= GetTypeHash(bFixRootMotionDirection); i++;
		if (bFixRootMotionDirection)
		{
			result ^= GetTypeHash(FixRootMotionDirectionFlags); i++;
			result ^= GetTypeHash(CustomAngle); i++;
			result ^= GetTypeHash(bRotateChildBones); i++;
		}

		return result;
	}

	void RefreshPreview()
	{
		const uint32 newModificationHash = CalculateModificationHash();

		if (ModificationHash != newModificationHash)
		{
			ModificationHash = newModificationHash;

			if (PersonaToolkit.IsValid())
			{
				SetFrames(AnimSequence, AnimSequenceSafe);

				TArray<UAnimSequence*> previewSequences;
				previewSequences.Add(AnimSequence);

				ApplyModifiersToCollection(previewSequences
					, []() -> bool { return false; }
					, []() {}
					, []() {}
				);

				PersonaToolkit->GetPreviewScene()->InvalidateViews();
			}
		}
	}

protected:
	TArray<UAnimSequence*> AnimSequences;
	UAnimSequence* AnimSequenceSafe;
	UAnimSequence* AnimSequence;

	uint8 bRemoveRootMotion : 1;
	uint8 bClearRootZeroFrameRotation : 1;
	uint8 bClearZeroFrameOffset : 1;

	uint8 bAddOffset : 1;

	uint8 bTransferAnimation : 1;
	uint8 bTransferAnimationClearZeroFrameOffset : 1;

	uint8 bSnap : 1;

	uint8 bFixRootMotionDirection : 1;

	uint8 bEnableRootMotion : 1;
	uint8 bRotateChildBones : 1;

	TMap<FGuid, FName> ClearZeroFrameOffsetEntries;
	TMap<FGuid, UE::Math::TIntVector3<bool>> ClearZeroFrameOffsetFlags;

	TMap<FGuid, FName> AddOffsetEntries;
	TMap<FGuid, FVector> AddOffsetValues;

	FName TransferAnimation_SourceBone = NAME_None;
	FName TransferAnimation_TargetBone = NAME_None;
	UE::Math::TIntVector3<bool> TransferAnimation_LocationFlags;
	UE::Math::TIntVector3<bool> TransferAnimation_RotationFlags;

	TMap<FGuid, FName> SnapSources;
	TMap<FGuid, FName> SnapTargets;

	UE::Math::TIntVector3<bool> FixRootMotionDirectionFlags;
	float CustomAngle;

	uint64 ModificationHash;

	TSharedPtr<class IPersonaToolkit> PersonaToolkit;
};

//--------------------------------------------------------------------
// SToolWidget
//--------------------------------------------------------------------

class SToolWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SToolWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, IRMFixToolBase* context)
	{
		Context = context;

		const FRMFixToolEditorModule& RMFixToolEditorModule = FModuleManager::GetModuleChecked<FRMFixToolEditorModule>("RMFixToolEditor");

		const ISlateStyle& styleSet = RMFixToolEditorModule.GetStyleSet();

		ChildSlot
			[
				SNew(SGridPanel)
					.FillColumn(0, 1).FillColumn(1, 0).FillRow(0, 0).FillRow(1, 0).FillRow(2, 0).FillRow(3, 0).FillRow(4, 0).FillRow(5, 0).FillRow(6, 0).FillRow(7, 0).FillRow(8, 0).FillRow(9, 1)

					+ SGridPanel::Slot(1, 0)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					.Padding(slotPadding)
					[
						SAssignNew(RemoveRootMotionImagePtr, SImage).Image(styleSet.GetBrush(FRMFixToolEditorModule::GetRemoveRootMotionIconName())).IsEnabled(false)
					]

					+ SGridPanel::Slot(0, 0)
					.VAlign(VAlign_Top)
					.Padding(slotPadding)
					[
						SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0, 0, 0, slotPadding)
							[
								SNew(SCheckBox)
									.IsChecked_Lambda([this]() { return Context->GetRemoveRootMotion() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
									.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetRemoveRootMotion(checkBoxState == ECheckBoxState::Checked); })
									[
										SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_RemoveRootMotion", "Remove Root Motion (make In Place)"))
											.TextStyle(FAppStyle::Get(), "NormalText")
									]
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0, 0, 0, slotPadding)
							[
								SNew(SCheckBox)
									.IsChecked_Lambda([this]() { return Context->GetClearRootZeroFrameRotation() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
									.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetClearRootZeroFrameRotation(checkBoxState == ECheckBoxState::Checked); })
									[
										SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_ClearRootZeroFrameRotation", "Clear Root zero frame rotation"))
											.TextStyle(FAppStyle::Get(), "NormalText")
									]
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0, 0, 0, slotPadding)
							[
								SNew(SVerticalBox)

									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SCheckBox)
											.IsChecked_Lambda([this]() { return Context->GetClearZeroFrameOffset() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
											.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetClearZeroFrameOffset(checkBoxState == ECheckBoxState::Checked); })
											[
												SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_ClearZeroFrameOffset", "Clear zero frame offset"))
													.TextStyle(FAppStyle::Get(), "NormalText")
											]
									]

									+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
									[
										SAssignNew(ClearZeroFrameOffsetVerticalBoxPtr, SVerticalBox)
											.Visibility_Lambda([this]() { return Context->GetClearZeroFrameOffset() ? EVisibility::Visible : EVisibility::Collapsed; })

											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(SVerticalBox)

													+ SVerticalBox::Slot().AutoHeight()
													[
														SNew(SHorizontalBox)

															+ SHorizontalBox::Slot().FillWidth(4).Padding(internalPadding)
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_ClearZeroFrameOffset_Bone", "Bone"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
															+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding)
															[
																SNew(SGridPanel)
																	+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).HAlign(HAlign_Center)
																	[
																		SNew(STextBlock)
																			.Text(LOCTEXT("RMFixTool_ToolDialog_ClearZeroFrameOffset_X", "X"))
																			.Justification(ETextJustify::Center)
																			.TextStyle(FAppStyle::Get(), "SmallText")
																	]
																	+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).HAlign(HAlign_Center)
																	[
																		SNew(SCheckBox).Visibility(EVisibility::Hidden)
																	]
															]
															+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding)
															[
																SNew(SGridPanel)
																	+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).HAlign(HAlign_Center)
																	[
																		SNew(STextBlock)
																			.Text(LOCTEXT("RMFixTool_ToolDialog_ClearZeroFrameOffset_Y", "Y"))
																			.Justification(ETextJustify::Center)
																			.TextStyle(FAppStyle::Get(), "SmallText")
																	]
																	+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).HAlign(HAlign_Center)
																	[
																		SNew(SCheckBox).Visibility(EVisibility::Hidden)
																	]
															]
															+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding)
															[
																SNew(SGridPanel)
																	+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).HAlign(HAlign_Center)
																	[
																		SNew(STextBlock)
																			.Text(LOCTEXT("RMFixTool_ToolDialog_ClearZeroFrameOffset_Z", "Z"))
																			.Justification(ETextJustify::Center)
																			.TextStyle(FAppStyle::Get(), "SmallText")
																	]
																	+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).HAlign(HAlign_Center)
																	[
																		SNew(SCheckBox).Visibility(EVisibility::Hidden)
																	]
															]
															+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding)
															[
																SNew(SButton).Visibility(EVisibility::Hidden)
																	.ButtonStyle(FAppStyle::Get(), "SimpleButton")
																	.ContentPadding(FMargin(1, 0))
																	[
																		SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
																	]
															]
													]
											]

											+ SVerticalBox::Slot().AutoHeight()
											[
												SAssignNew(ClearZeroFrameOffsetItemsVerticalBoxPtr, SVerticalBox)
											]

											+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
											[
												SNew(SButton)
													.ButtonStyle(FAppStyle::Get(), "SimpleButton")
													.OnClicked(this, &SToolWidget::OnAddEntry_ClearZeroFrameOffset_Clicked)
													.ContentPadding(FMargin(1, 0))
													[
														SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle")).ColorAndOpacity(FSlateColor::UseForeground())
													]
											]
									]
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0, 0, 0, slotPadding)
							[
								SNew(SVerticalBox)

									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SCheckBox)
											.IsChecked_Lambda([this]() { return Context->GetAddOffset() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
											.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetAddOffset(checkBoxState == ECheckBoxState::Checked); })
											[
												SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_AddOffset", "Add offset"))
													.TextStyle(FAppStyle::Get(), "NormalText")
											]
									]

									+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
									[
										SAssignNew(AddOffsetVerticalBoxPtr, SVerticalBox)
											.Visibility_Lambda([this]() { return Context->GetAddOffset() ? EVisibility::Visible : EVisibility::Collapsed; })

											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(SVerticalBox)

													+ SVerticalBox::Slot().AutoHeight()
													[
														SNew(SHorizontalBox)

															+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_AddOffset_Bone", "Bone"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
															+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
															[
																SNew(SGridPanel)
																	+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).HAlign(HAlign_Center)
																	[
																		SNew(STextBlock)
																			.Text(LOCTEXT("RMFixTool_ToolDialog_AddOffset_Offset", "Offset"))
																			.Justification(ETextJustify::Center)
																			.TextStyle(FAppStyle::Get(), "SmallText")
																	]
															]
															+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding)
															[
																SNew(SButton).Visibility(EVisibility::Hidden)
																	.ButtonStyle(FAppStyle::Get(), "SimpleButton")
																	.ContentPadding(FMargin(1, 0))
																	[
																		SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
																	]
															]
													]
											]

											+ SVerticalBox::Slot().AutoHeight()
											[
												SAssignNew(AddOffsetItemsVerticalBoxPtr, SVerticalBox)
											]

											+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
											[
												SNew(SButton)
													.ButtonStyle(FAppStyle::Get(), "SimpleButton")
													.OnClicked(this, &SToolWidget::OnAddEntry_AddOffset_Clicked)
													.ContentPadding(FMargin(1, 0))
													[
														SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle")).ColorAndOpacity(FSlateColor::UseForeground())
													]
											]
									]
							]
					]

					+ SGridPanel::Slot(0, 1).ColumnSpan(2).Padding(slotPadding, internalPadding)
					[
						SNew(SSeparator).Orientation(EOrientation::Orient_Horizontal).Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
					]

					+ SGridPanel::Slot(1, 2)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					.Padding(slotPadding)
					[
						SAssignNew(TransferAnimationImagePtr, SImage).Image(styleSet.GetBrush(FRMFixToolEditorModule::GetTransferAnimationIconName())).IsEnabled(false)
					]

					+ SGridPanel::Slot(0, 2)
					.VAlign(VAlign_Top)
					.Padding(slotPadding)
					[
						SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0, 0, 0, slotPadding)
							[
								SNew(SVerticalBox)

									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SCheckBox)
											.IsChecked_Lambda([this]() { return Context->GetTransferAnimation() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
											.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetTransferAnimation(checkBoxState == ECheckBoxState::Checked); })
											[
												SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_TransferAnimation", "Transfer animation between bones"))
													.TextStyle(FAppStyle::Get(), "NormalText")
											]
									]

									+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
									[
										SAssignNew(TransferAnimationGridPanelPtr, SGridPanel)
											.Visibility_Lambda([this]() { return Context->GetTransferAnimation() ? EVisibility::Visible : EVisibility::Collapsed; })
											.FillColumn(0, 0).FillColumn(1, 1)
											.FillRow(0, 0).FillRow(1, 0).FillRow(2, 0).FillRow(3, 0)

											+ SGridPanel::Slot(0, 0).Padding(internalPadding).VAlign(VAlign_Center)
											[
												SNew(STextBlock)
													.Text(LOCTEXT("RMFixTool_ToolDialog_TransferAnimation_From", "From"))
													.Justification(ETextJustify::Center)
													.TextStyle(FAppStyle::Get(), "SmallText")
											]

											+ SGridPanel::Slot(1, 0)
											[
												SNew(SBoneSelectionWidget)
													.OnBoneSelectionChanged(FOnBoneSelectionChanged::CreateLambda([this](FName InName) { Context->SetTransferAnimationTargetBone(InName); }))
													.OnGetSelectedBone(FGetSelectedBone::CreateLambda([this](bool& bMultipleValues) { bMultipleValues = false; return Context->GetTransferAnimationTargetBone(); }))
													.OnGetReferenceSkeleton(this, &SToolWidget::GetReferenceSkeleton)
											]

											+ SGridPanel::Slot(0, 1).Padding(internalPadding).VAlign(VAlign_Center)
											[
												SNew(STextBlock)
													.Text(LOCTEXT("RMFixTool_ToolDialog_TransferAnimation_To", "To"))
													.Justification(ETextJustify::Center)
													.TextStyle(FAppStyle::Get(), "SmallText")
											]

											+ SGridPanel::Slot(1, 1)
											[
												SNew(SBoneSelectionWidget)
													.OnBoneSelectionChanged(FOnBoneSelectionChanged::CreateLambda([this](FName InName) { Context->SetTransferAnimationSourceBone(InName); }))
													.OnGetSelectedBone(FGetSelectedBone::CreateLambda([this](bool& bMultipleValues) { bMultipleValues = false; return Context->GetTransferAnimationSourceBone(); }))
													.OnGetReferenceSkeleton(this, &SToolWidget::GetReferenceSkeleton)
											]

											+ SGridPanel::Slot(0, 2).ColumnSpan(2)
											[
												SNew(SCheckBox)
													.IsChecked_Lambda([this]() { return Context->GetTransferAnimationClearZeroFrameOffset() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
													.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetTransferAnimationClearZeroFrameOffset(checkBoxState == ECheckBoxState::Checked); })
													[
														SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_ClearZeroFrameOffset", "Clear zero frame offset"))
															.TextStyle(FAppStyle::Get(), "NormalText")
													]
											]

											+ SGridPanel::Slot(0, 3).ColumnSpan(2).VAlign(VAlign_Center)
											[
												SNew(SHorizontalBox)

													+ SHorizontalBox::Slot().FillWidth(4).Padding(internalPadding)
													[
														SNew(SCheckBox)
															.IsChecked_Lambda([this]() { return Context->GetTransferAnimationLocationFlags().X ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
															.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetTransferAnimationLocationFlags().X = (checkBoxState == ECheckBoxState::Checked); Context->RefreshPreview(); })
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_TransferAnimation_X", "X"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
													]
													+ SHorizontalBox::Slot().FillWidth(4).Padding(internalPadding)
													[
														SNew(SCheckBox)
															.IsChecked_Lambda([this]() { return Context->GetTransferAnimationLocationFlags().Y ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
															.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetTransferAnimationLocationFlags().Y = (checkBoxState == ECheckBoxState::Checked); Context->RefreshPreview(); })
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_TransferAnimation_Y", "Y"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
													]
													+ SHorizontalBox::Slot().FillWidth(4).Padding(internalPadding)
													[
														SNew(SCheckBox)
															.IsChecked_Lambda([this]() { return Context->GetTransferAnimationLocationFlags().Z ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
															.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetTransferAnimationLocationFlags().Z = (checkBoxState == ECheckBoxState::Checked); Context->RefreshPreview(); })
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_TransferAnimation_Z", "Z"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
													]
											]

											+ SGridPanel::Slot(0, 4).ColumnSpan(2).VAlign(VAlign_Center)
											[
												SNew(SHorizontalBox)

													+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
													[
														SNew(SCheckBox)
															.IsChecked_Lambda([this]() { return Context->GetTransferAnimationRotationFlags().X ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
															.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetTransferAnimationRotationFlags().X = (checkBoxState == ECheckBoxState::Checked); Context->RefreshPreview(); })
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_TransferAnimation_RotX", "Roll"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
													]
													+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
													[
														SNew(SCheckBox)
															.IsChecked_Lambda([this]() { return Context->GetTransferAnimationRotationFlags().Y ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
															.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetTransferAnimationRotationFlags().Y = (checkBoxState == ECheckBoxState::Checked); Context->RefreshPreview(); })
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_TransferAnimation_RotY", "Pitch"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
													]
													+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
													[
														SNew(SCheckBox)
															.IsChecked_Lambda([this]() { return Context->GetTransferAnimationRotationFlags().Z ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
															.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetTransferAnimationRotationFlags().Z = (checkBoxState == ECheckBoxState::Checked); Context->RefreshPreview(); })
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_TransferAnimation_RotZ", "Yaw"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
													]
											]
									]
							]
					]

					+ SGridPanel::Slot(0, 3).ColumnSpan(2).Padding(slotPadding, internalPadding)
					[
						SNew(SSeparator).Orientation(EOrientation::Orient_Horizontal).Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
					]

					+ SGridPanel::Slot(1, 4)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					.Padding(slotPadding)
					[
						SAssignNew(TransferAnimationImagePtr, SImage).Image(styleSet.GetBrush(FRMFixToolEditorModule::GetSnapIconName())).IsEnabled(false)
					]

					+ SGridPanel::Slot(0, 4)
					.VAlign(VAlign_Top)
					.Padding(slotPadding)
					[
						SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0, 0, 0, slotPadding)
							[
								SNew(SVerticalBox)

									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SCheckBox)
											.IsChecked_Lambda([this]() { return Context->GetSnap() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
											.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetSnap(checkBoxState == ECheckBoxState::Checked); })
											[
												SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_Snap", "Snap"))
													.TextStyle(FAppStyle::Get(), "NormalText")
											]
									]

									+ SVerticalBox::Slot().AutoHeight()
									[
										SAssignNew(SnapVerticalBoxPtr, SVerticalBox)
											.Visibility_Lambda([this]() { return Context->GetSnap() ? EVisibility::Visible : EVisibility::Collapsed; })

											+ SVerticalBox::Slot().AutoHeight()
											[
												SNew(SVerticalBox)

													+ SVerticalBox::Slot().AutoHeight()
													[
														SNew(SHorizontalBox)

															+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_SnapBone", "Snap"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
															+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding)
															[
																SNew(STextBlock)
																	.Text(LOCTEXT("RMFixTool_ToolDialog_Snap_TargetBone", "Target"))
																	.Justification(ETextJustify::Center)
																	.TextStyle(FAppStyle::Get(), "SmallText")
															]
															+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding)
															[
																SNew(SButton).Visibility(EVisibility::Hidden)
																	.ButtonStyle(FAppStyle::Get(), "SimpleButton")
																	.ContentPadding(FMargin(1, 0))
																	[
																		SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
																	]
															]
													]
											]

											+ SVerticalBox::Slot().AutoHeight()
											[
												SAssignNew(SnapItemsVerticalBoxPtr, SVerticalBox)
											]

											+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
											[
												SNew(SButton)
													.ButtonStyle(FAppStyle::Get(), "SimpleButton")
													.OnClicked(this, &SToolWidget::OnAddEntry_Snap_Clicked)
													.ContentPadding(FMargin(1, 0))
													[
														SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle")).ColorAndOpacity(FSlateColor::UseForeground())
													]
											]

											+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
											[
												SNew(SButton)
													.ButtonStyle(FAppStyle::Get(), "SimpleButton")
													.OnClicked(this, &SToolWidget::OnAddEntry_Snap_IKBones)
													.ContentPadding(FMargin(1, 0))
													[
														SNew(STextBlock)
															.Text(LOCTEXT("RMFixTool_ToolDialog_Snap_AddIKBones", "Add IK Bones"))
															.Justification(ETextJustify::Center)
															.TextStyle(FAppStyle::Get(), "SmallText")
													]
											]
									]
							]
					]

					+ SGridPanel::Slot(0, 5).ColumnSpan(2).Padding(slotPadding, internalPadding)
					[
						SNew(SSeparator).Orientation(EOrientation::Orient_Horizontal).Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
					]

					+ SGridPanel::Slot(1, 6)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					.Padding(slotPadding)
					[
						SAssignNew(FixRootMotionDirectionImagePtr, SImage).Image(styleSet.GetBrush(FRMFixToolEditorModule::GetFixRootMotionDirectionIconName())).IsEnabled(false)
					]

					+ SGridPanel::Slot(0, 6).Padding(slotPadding, internalPadding)
					[
						SNew(SVerticalBox)

							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SCheckBox)
									.IsChecked_Lambda([this]() { return Context->GetFixRootMotionDirection() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
									.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetFixRootMotionDirection(checkBoxState == ECheckBoxState::Checked); })
									[
										SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_FixRootMotionDirection", "Fix Root Motion direction")).TextStyle(FAppStyle::Get(), "NormalText")
									]
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
							[
								SNew(SHorizontalBox).Visibility_Lambda([this]() { return Context->GetFixRootMotionDirection() ? EVisibility::Visible : EVisibility::Collapsed; })

									+ SHorizontalBox::Slot().FillWidth(4).Padding(internalPadding)
									[
										SNew(SCheckBox)
											.Visibility_Lambda([this]() { return Context->GetFixRootMotionDirectionFlags().X ? EVisibility::HitTestInvisible : EVisibility::Visible; })
											.IsChecked_Lambda([this]() { return Context->GetFixRootMotionDirectionFlags().X ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
											.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState)
												{
													Context->SetFixRootMotionDirectionFlags().X = true;
													Context->SetFixRootMotionDirectionFlags().Y = false;
													Context->SetFixRootMotionDirectionFlags().Z = false;
													Context->RefreshPreview();
												})
											[
												SNew(STextBlock)
													.Text(LOCTEXT("RMFixTool_ToolDialog_FixRootMotionDirection_X", "Align with X"))
													.Justification(ETextJustify::Center)
													.TextStyle(FAppStyle::Get(), "SmallText")
											]
									]
									+ SHorizontalBox::Slot().FillWidth(4).Padding(internalPadding)
									[
										SNew(SCheckBox)
											.Visibility_Lambda([this]() { return Context->GetFixRootMotionDirectionFlags().Y ? EVisibility::HitTestInvisible : EVisibility::Visible; })
											.IsChecked_Lambda([this]() { return Context->GetFixRootMotionDirectionFlags().Y ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
											.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState)
												{
													Context->SetFixRootMotionDirectionFlags().X = false;
													Context->SetFixRootMotionDirectionFlags().Y = true;
													Context->SetFixRootMotionDirectionFlags().Z = false;
													Context->RefreshPreview();
												})
											[
												SNew(STextBlock)
													.Text(LOCTEXT("RMFixTool_ToolDialog_FixRootMotionDirection_Y", "Align with Y"))
													.Justification(ETextJustify::Center)
													.TextStyle(FAppStyle::Get(), "SmallText")
											]
									]
									+ SHorizontalBox::Slot().FillWidth(4).Padding(internalPadding)
									[
										SNew(SCheckBox)
											.Visibility_Lambda([this]() { return Context->GetFixRootMotionDirectionFlags().Z && Context->GetFixRootMotionDirection() ? EVisibility::HitTestInvisible : EVisibility::Visible; })
											.IsChecked_Lambda([this]() { return Context->GetFixRootMotionDirectionFlags().Z ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
											.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState)
												{
													Context->SetFixRootMotionDirectionFlags().X = false;
													Context->SetFixRootMotionDirectionFlags().Y = false;
													Context->SetFixRootMotionDirectionFlags().Z = true;
													Context->RefreshPreview();
												})
											[
												SNew(STextBlock)
													.Text(LOCTEXT("RMFixTool_ToolDialog_FixRootMotionDirection_Z", "Custom angle"))
													.Justification(ETextJustify::Center)
													.TextStyle(FAppStyle::Get(), "SmallText")
											]
									]
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
							[
								SNew(SNumericEntryBox<float>).Visibility_Lambda([this]() { return Context->GetFixRootMotionDirectionFlags().Z ? EVisibility::Visible : EVisibility::Collapsed; })
									.AllowSpin(true)
									.MinValue(-180)
									.MaxValue(180)
									.MaxSliderValue(180)
									.MinSliderValue(-180)
									.Value_Lambda([this]() { return Context->GetCustomAngle(); })
									.OnValueChanged_Lambda([this](float value) { Context->SetCustomAngle(value); })
									.OnValueCommitted_Lambda([this](float value, ETextCommit::Type commitType) { Context->SetCustomAngle(value); Context->RefreshPreview(); })
							]

							+ SVerticalBox::Slot().AutoHeight().Padding(contentPadding, 0, 0, 0)
							[
								SNew(SBox).Padding(internalPadding)
									[
										SNew(SCheckBox).Visibility_Lambda([this]() { return Context->GetFixRootMotionDirection() ? EVisibility::Visible : EVisibility::Collapsed; })
											.IsChecked_Lambda([this]() { return Context->GetRotateChildBones() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
											.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetRotateChildBones(checkBoxState == ECheckBoxState::Checked); })
											[
												SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_RotateChildBones", "Rotate Root Bone child bones correspondingly")).TextStyle(FAppStyle::Get(), "SmallText")
											]
									]
							]
					]

					+ SGridPanel::Slot(0, 7).ColumnSpan(2).Padding(slotPadding, internalPadding)
					[
						SNew(SSeparator).Orientation(EOrientation::Orient_Horizontal).Thickness(internalPadding).SeparatorImage(FAppStyle::Get().GetBrush("Menu.Separator"))
					]

					+ SGridPanel::Slot(0, 8).ColumnSpan(2).Padding(slotPadding, internalPadding).HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
					[
						SNew(SButton)
							.TextStyle(FAppStyle::Get(), "DialogButtonText")
							.Text(LOCTEXT("SAnimModDialog_Apply", "Apply"))
							.HAlign(HAlign_Center)
							.Visibility_Lambda([this]() { return Context->CanApplyModifiers() ? EVisibility::Visible : EVisibility::Collapsed; })
							.OnClicked_Lambda([this]()
								{
									Context->ApplyModifiers();

									ResetWidgets();

									Context->Reset();

									Context->RefreshPreview();

									return FReply::Handled();
								})
					]

					+ SGridPanel::Slot(0, 9).ColumnSpan(2).Padding(slotPadding, internalPadding).HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
					[
						SNew(SCheckBox)
							.IsChecked_Lambda([this]() { return Context->GetEnableRootMotion() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this](ECheckBoxState checkBoxState) { Context->SetEnableRootMotion(checkBoxState == ECheckBoxState::Checked); })
							[
								SNew(STextBlock).Text(LOCTEXT("RMFixTool_ToolDialog_EnableRootMotion", "EnableRootMotion")).TextStyle(FAppStyle::Get(), "NormalText")
							]
					]
			];
	}

protected:

	FReply OnAddEntry_ClearZeroFrameOffset_Clicked()
	{
		FGuid guid = FGuid::NewGuid();

		Context->AddEntry_ClearZeroFrameOffset(guid);

		ClearZeroFrameOffsetItemsVerticalBoxPtr->AddSlot()
			[
				SNew(SHorizontalBox).Tag(FName(guid.ToString()))

					+ SHorizontalBox::Slot().FillWidth(4).Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SBoneSelectionWidget)
							.OnBoneSelectionChanged(FOnBoneSelectionChanged::CreateLambda([this, guid](FName InName) { Context->SetEntry_ClearZeroFrameOffset(guid, InName); }))
							.OnGetSelectedBone(FGetSelectedBone::CreateLambda([this, guid](bool& bMultipleValues) { bMultipleValues = false; return Context->GetEntry_ClearZeroFrameOffset(guid); }))
							.OnGetReferenceSkeleton(this, &SToolWidget::GetReferenceSkeleton)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
							.IsChecked(false)
							.OnCheckStateChanged_Lambda([this, guid](ECheckBoxState checkBoxState) { Context->SetFlags_ClearZeroFrameOffset(guid, EAxis::X, checkBoxState == ECheckBoxState::Checked); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
							.IsChecked(false)
							.OnCheckStateChanged_Lambda([this, guid](ECheckBoxState checkBoxState) { Context->SetFlags_ClearZeroFrameOffset(guid, EAxis::Y, checkBoxState == ECheckBoxState::Checked); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
							.IsChecked(false)
							.OnCheckStateChanged_Lambda([this, guid](ECheckBoxState checkBoxState) { Context->SetFlags_ClearZeroFrameOffset(guid, EAxis::Z, checkBoxState == ECheckBoxState::Checked); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &SToolWidget::OnRemoveEntry_ClearZeroFrameOffset_Clicked, guid)
							.ContentPadding(FMargin(1, 0))
							[
								SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
							]
					]
			];

		return FReply::Handled();
	}

	FReply OnRemoveEntry_ClearZeroFrameOffset_Clicked(FGuid guid)
	{
		FChildren* children = ClearZeroFrameOffsetItemsVerticalBoxPtr->GetAllChildren();

		for (int32 i = 0; i < children->Num(); i++)
		{
			if (children->GetChildAt(i)->GetTag().ToString() == guid.ToString())
			{
				ClearZeroFrameOffsetItemsVerticalBoxPtr->RemoveSlot(children->GetChildAt(i));
				break;
			}
		}

		Context->RemoveEntry_ClearZeroFrameOffset(guid);

		return FReply::Handled();
	}

	typedef SNumericVectorInputBox<double> SNumericVectorInputBoxLocal;

	FReply OnAddEntry_AddOffset_Clicked()
	{
		FGuid guid = FGuid::NewGuid();

		Context->AddEntry_AddOffset(guid);

		const float limitingValue = 1000;

		AddOffsetItemsVerticalBoxPtr->AddSlot()
			[
				SNew(SHorizontalBox).Tag(FName(guid.ToString()))

					+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SBoneSelectionWidget)
							.OnBoneSelectionChanged(FOnBoneSelectionChanged::CreateLambda([this, guid](FName InName) { Context->SetEntry_AddOffset(guid, InName); }))
							.OnGetSelectedBone(FGetSelectedBone::CreateLambda([this, guid](bool& bMultipleValues) { bMultipleValues = false; return Context->GetEntry_AddOffset(guid); }))
							.OnGetReferenceSkeleton(this, &SToolWidget::GetReferenceSkeleton)
					]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SNumericVectorInputBoxLocal)
							.AllowSpin(true)
							.SpinDelta(0.01f)
							.MinSliderVector(FVector(-limitingValue, -limitingValue, -limitingValue))
							.MaxSliderVector(FVector(limitingValue, limitingValue, limitingValue))
							.X_Lambda([this, guid]() { return Context->GetValue_AddOffset(guid).X; })
							.Y_Lambda([this, guid]() { return Context->GetValue_AddOffset(guid).Y; })
							.Z_Lambda([this, guid]() { return Context->GetValue_AddOffset(guid).Z; })
							.OnXChanged_Lambda([this, guid](double value) { Context->SetValue_AddOffset(guid).X = value; })
							.OnYChanged_Lambda([this, guid](double value) { Context->SetValue_AddOffset(guid).Y = value; })
							.OnZChanged_Lambda([this, guid](double value) { Context->SetValue_AddOffset(guid).Z = value; })
							.OnXCommitted_Lambda([this, guid](double value, ETextCommit::Type commitType) { Context->SetValue_AddOffset(guid).X = value; Context->RefreshPreview(); })
							.OnYCommitted_Lambda([this, guid](double value, ETextCommit::Type commitType) { Context->SetValue_AddOffset(guid).Y = value; Context->RefreshPreview(); })
							.OnZCommitted_Lambda([this, guid](double value, ETextCommit::Type commitType) { Context->SetValue_AddOffset(guid).Z = value; Context->RefreshPreview(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding).VAlign(VAlign_Center)
					[
						SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(this, &SToolWidget::OnRemoveEntry_AddOffset_Clicked, guid)
							.ContentPadding(FMargin(1, 0))
							[
								SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
							]
					]
			];

		return FReply::Handled();
	}

	FReply OnRemoveEntry_AddOffset_Clicked(FGuid guid)
	{
		FChildren* children = AddOffsetItemsVerticalBoxPtr->GetAllChildren();

		for (int32 i = 0; i < children->Num(); i++)
		{
			if (children->GetChildAt(i)->GetTag().ToString() == guid.ToString())
			{
				AddOffsetItemsVerticalBoxPtr->RemoveSlot(children->GetChildAt(i));
				break;
			}
		}

		Context->RemoveEntry_AddOffset(guid);

		return FReply::Handled();
	}

	TSharedPtr<SWidget> CreateSnapEntry(const FGuid& guid)
	{
		return SNew(SHorizontalBox).Tag(FName(guid.ToString()))

			+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding).VAlign(VAlign_Center)
			[
				SNew(SBoneSelectionWidget)
					.OnBoneSelectionChanged(FOnBoneSelectionChanged::CreateLambda([this, guid](FName InName) { Context->SetEntry_SnapSource(guid, InName); }))
					.OnGetSelectedBone(FGetSelectedBone::CreateLambda([this, guid](bool& bMultipleValues) { bMultipleValues = false; return Context->GetEntry_SnapSource(guid); }))
					.OnGetReferenceSkeleton(this, &SToolWidget::GetReferenceSkeleton)
			]
			+ SHorizontalBox::Slot().FillWidth(1).Padding(internalPadding).VAlign(VAlign_Center)
			[
				SNew(SBoneSelectionWidget)
					.OnBoneSelectionChanged(FOnBoneSelectionChanged::CreateLambda([this, guid](FName InName) { Context->SetEntry_SnapTarget(guid, InName); }))
					.OnGetSelectedBone(FGetSelectedBone::CreateLambda([this, guid](bool& bMultipleValues) { bMultipleValues = false; return Context->GetEntry_SnapTarget(guid); }))
					.OnGetReferenceSkeleton(this, &SToolWidget::GetReferenceSkeleton)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(internalPadding).VAlign(VAlign_Center)
			[
				SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &SToolWidget::OnRemoveEntry_Snap_Clicked, guid)
					.ContentPadding(FMargin(1, 0))
					[
						SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X")).ColorAndOpacity(FSlateColor::UseForeground())
					]
			];
	}

	FReply OnAddEntry_Snap_Clicked()
	{
		FGuid guid = FGuid::NewGuid();

		Context->AddEntry_Snap(guid);

		SnapItemsVerticalBoxPtr->AddSlot()[CreateSnapEntry(guid).ToSharedRef()];

		return FReply::Handled();
	}

	TMap<FName, FName> SnapIKBones = {
		{ FName("center_of_mass"), FName("root") },
		{ FName("ik_foot_root"), FName("root") },
		{ FName("ik_hand_root"), FName("root") },
		{ FName("interaction"), FName("root") },
		{ FName("ik_foot_l"), FName("foot_l") },
		{ FName("ik_foot_r"), FName("foot_r") },
		{ FName("ik_hand_gun"), FName("hand_r") },
		{ FName("ik_hand_l"), FName("hand_l") },
		{ FName("ik_hand_r"), FName("hand_r") },
	};

	FReply OnAddEntry_Snap_IKBones()
	{
		const FReferenceSkeleton& RefSkeleton = GetReferenceSkeleton();

		for (TPair<FName, FName>& snapIKBonesEntry : SnapIKBones)
		{
			if (snapIKBonesEntry.Key != NAME_None && snapIKBonesEntry.Value != NAME_None && !Context->HasEntry_Snap(snapIKBonesEntry.Key, snapIKBonesEntry.Value))
			{
				if (RefSkeleton.FindBoneIndex(snapIKBonesEntry.Key) != INDEX_NONE && RefSkeleton.FindBoneIndex(snapIKBonesEntry.Value) != INDEX_NONE	)
				{
					FGuid guid = FGuid::NewGuid();

					Context->AddEntry_Snap(guid);
					Context->SetEntry_SnapSource(guid, snapIKBonesEntry.Key);
					Context->SetEntry_SnapTarget(guid, snapIKBonesEntry.Value);

					SnapItemsVerticalBoxPtr->AddSlot()[CreateSnapEntry(guid).ToSharedRef()];
				}
			}
		}

		return FReply::Handled();
	}

	FReply OnRemoveEntry_Snap_Clicked(FGuid guid)
	{
		FChildren* children = SnapItemsVerticalBoxPtr->GetAllChildren();

		for (int32 i = 0; i < children->Num(); i++)
		{
			if (children->GetChildAt(i)->GetTag().ToString() == guid.ToString())
			{
				SnapItemsVerticalBoxPtr->RemoveSlot(children->GetChildAt(i));
				break;
			}
		}

		Context->RemoveEntry_Snap(guid);

		return FReply::Handled();
	}

	const FReferenceSkeleton& GetReferenceSkeleton() const
	{
		return Context->GetReferenceSkeleton();
	}

	void ResetWidgets()
	{
		ClearZeroFrameOffsetItemsVerticalBoxPtr->ClearChildren();

		AddOffsetItemsVerticalBoxPtr->ClearChildren();

		SnapItemsVerticalBoxPtr->ClearChildren();
	}

protected:

	IRMFixToolBase* Context;

	TSharedPtr<SImage> RemoveRootMotionImagePtr;
	TSharedPtr<SVerticalBox> ClearZeroFrameOffsetVerticalBoxPtr;
	TSharedPtr<SVerticalBox> ClearZeroFrameOffsetItemsVerticalBoxPtr;

	TSharedPtr<SVerticalBox> AddOffsetVerticalBoxPtr;
	TSharedPtr<SVerticalBox> AddOffsetItemsVerticalBoxPtr;

	TSharedPtr<SImage> TransferAnimationImagePtr;
	TSharedPtr<SGridPanel> TransferAnimationGridPanelPtr;

	TSharedPtr<SVerticalBox> SnapVerticalBoxPtr;
	TSharedPtr<SVerticalBox> SnapItemsVerticalBoxPtr;

	TSharedPtr<SImage> FixRootMotionDirectionImagePtr;
};

//--------------------------------------------------------------------
// FRMFixEditor
//--------------------------------------------------------------------

const FName RMFixEditorAppIdentifier = FName(TEXT("RMFixEditorAppIdentifier"));

namespace RMFixEditorModes
{
	// Mode identifiers
	const FName SingleAssetEditorMode(TEXT("SingleAssetEditorMode"));
	const FName MultipleAssetEditorMode(TEXT("MultipleAssetEditorMode"));
}

namespace RMFixEditorTabs
{
	// Tab identifiers
	const FName ViewportTab(TEXT("Viewport"));
	const FName ModifiersTab(TEXT("Modifiers"));
	const FName PlayerTab(TEXT("Player"));
}

//--------------------------------------------------------------------
// FApplicationMode_RMFixEditor_SingleAssetEditorMode
//--------------------------------------------------------------------

class FApplicationMode_RMFixEditor_SingleAssetEditorMode : public FApplicationMode
{
public:
	FApplicationMode_RMFixEditor_SingleAssetEditorMode(TSharedRef<class FWorkflowCentricApplication> InHostingApp, const TSharedRef<IPersonaPreviewScene>& previewScene);
	
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;

protected:
	virtual void AddTabFactory(FCreateWorkflowTabFactory FactoryCreator) override;
	virtual void RemoveTabFactory(FName TabFactoryID) override;

protected:
	TWeakPtr<class FWorkflowCentricApplication> HostingAppPtr;
	FWorkflowAllowedTabSet TabFactories;
};

//--------------------------------------------------------------------
// FRMFixEditor
//--------------------------------------------------------------------

class FRMFixEditor : public IAnimationEditor, public FGCObject, public FTickableEditorObject, public IRMFixToolBase
{
public:
	virtual ~FRMFixEditor();
	void InitRMFixEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, TArray<UAnimSequence*>& AnimSequences);

	/** IToolkit interface */
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override { FAssetEditorToolkit::UnregisterTabSpawners(InTabManager); }
	virtual FName GetToolkitFName() const override { return FName("RM Fix Tool"); }
	virtual FText GetBaseToolkitName() const override { return LOCTEXT("FRMFixEditor_BaseToolkitName", "RM Fix Tool"); }
	virtual FString GetWorldCentricTabPrefix() const override { return LOCTEXT("FRMFixEditor_WorldCentricTabPrefix", "RMFixTool ").ToString(); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f); }

	virtual FText GetToolkitName() const override{ return FPersonaAssetEditorToolkit::GetToolkitName(); }

	/** FTickableEditorObject Interface */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FRMFixEditor, STATGROUP_Tickables); }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		for (UAnimSequence* animSequence : AnimSequences)
		{
			TObjectPtr<UAnimSequence> animSequencePtr = animSequence;
			Collector.AddReferencedObject(animSequencePtr);
		}
	}
	virtual FString GetReferencerName() const override { return TEXT("FRMFixEditor"); }

	UObject* HandleGetAsset() { return GetEditingObject(); }

	TSharedRef<SWidget> GenerateSingleObjectPlayer(const FWorkflowTabSpawnInfo& Info)
	{
		FString    DocumentLink;

		FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");

		ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
		EditableSkeleton = SkeletonEditorModule.CreateEditableSkeleton(AnimSequence->GetSkeleton());

		FAnimDocumentArgs Args(PersonaToolkit->GetPreviewScene(), GetPersonaToolkit(), EditableSkeleton.ToSharedRef(), OnSectionsChanged);

		return PersonaModule.CreateEditorWidgetForAnimDocument(SharedThis(this), AnimSequence, Args, DocumentLink);
	}

	virtual void SetAnimationAsset(UAnimationAsset* AnimAsset) override {}
	
	virtual IAnimationSequenceBrowser* GetAssetBrowser() const override { return nullptr; }

	virtual TSharedRef<class IPersonaToolkit> GetPersonaToolkit() const override { return PersonaToolkit.ToSharedRef(); }

	virtual void EditCurves(UAnimSequenceBase* InAnimSequence, const TArray<FCurveEditInfo>& InCurveInfo, const TSharedPtr<ITimeSliderController>& InExternalTimeSliderController) override {}

	virtual void StopEditingCurves(const TArray<FCurveEditInfo>& InCurveInfo) override {}

private:

	void OnReimportAnimation();

	void ConditionalRefreshEditor(UObject* InObject);

	void HandlePostReimport(UObject* InObject, bool bSuccess);

	void HandlePostImport(class UFactory* InFactory, UObject* InObject) { ConditionalRefreshEditor(InObject); }

private:

	void HandleOnPreviewSceneSettingsCustomized(IDetailLayoutBuilder& DetailBuilder) const { /* DetailBuilder.HideCategory("Animation Blueprint"); */ }

private:

	TSharedPtr<SToolWidget> ToolWidgetPtr;
	
	TSharedPtr<IEditableSkeleton> EditableSkeleton;

	FSimpleMulticastDelegate OnSectionsChanged;
};

FRMFixEditor::~FRMFixEditor()
{
	if (PersonaToolkit.IsValid())
	{
		constexpr bool bSetPreviewMeshInAsset = false;
		PersonaToolkit->SetPreviewMesh(nullptr, bSetPreviewMeshInAsset);
	}

	GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
}

void FRMFixEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_AnimationEditor", "Animation Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FRMFixEditor::InitRMFixEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, TArray<UAnimSequence*>& animSequences)
{
	AnimSequences = animSequences;

	AnimSequence = Cast<UAnimSequence>(DuplicateObject(AnimSequences[0], GetTransientPackage()));
	AnimSequence->AddToRoot();

	AnimSequenceSafe = Cast<UAnimSequence>(DuplicateObject(AnimSequences[0], GetTransientPackage()));
	AnimSequenceSafe->AddToRoot();

	// Register post import callback to catch animation imports when we have the asset open (we need to reinit)
	FReimportManager::Instance()->OnPostReimport().AddRaw(this, &FRMFixEditor::HandlePostReimport);
	GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddRaw(this, &FRMFixEditor::HandlePostImport);

	FPersonaToolkitArgs PersonaToolkitArgs;
	PersonaToolkitArgs.OnPreviewSceneSettingsCustomized = FOnPreviewSceneSettingsCustomized::FDelegate::CreateSP(this, &FRMFixEditor::HandleOnPreviewSceneSettingsCustomized);

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(AnimSequence, PersonaToolkitArgs);

	PersonaToolkit->GetPreviewScene()->SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode::Animation);

	const bool bCreateDefaultStandaloneMenu = false;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, RMFixEditorAppIdentifier, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, AnimSequences[0]);

	AddApplicationMode(RMFixEditorModes::SingleAssetEditorMode, MakeShareable(new FApplicationMode_RMFixEditor_SingleAssetEditorMode(SharedThis(this), PersonaToolkit->GetPreviewScene())));

	SetCurrentMode(RMFixEditorModes::SingleAssetEditorMode);

	RegenerateMenusAndToolbars();

	PersonaToolkit->GetPreviewScene()->SetAllowMeshHitProxies(false);
}

void FRMFixEditor::Tick(float DeltaTime)
{
	if (PersonaToolkit.IsValid())
	{
		if (PersonaToolkit->GetMesh() && PersonaToolkit->GetMesh()->IsCompiling())
		{
			return;
		}
		PersonaToolkit->GetPreviewScene()->InvalidateViews();
	}
}

void FRMFixEditor::OnReimportAnimation()
{
	for (UAnimSequence* animSequence : AnimSequences)
	{
		if (animSequence)
		{
			FReimportManager::Instance()->ReimportAsync(animSequence, true);
		}
	}
}

void FRMFixEditor::ConditionalRefreshEditor(UObject* InObject)
{
	if (PersonaToolkit.IsValid())
	{
		bool bInterestingAsset = true;

		if (InObject != PersonaToolkit->GetSkeleton() && (PersonaToolkit->GetSkeleton() && InObject != PersonaToolkit->GetSkeleton()->GetPreviewMesh()) && AnimSequences.Contains(InObject))
		{
			bInterestingAsset = false;
		}

		if (PersonaToolkit->GetSkeleton() == nullptr)
		{
			bInterestingAsset = false;
		}

		if (bInterestingAsset)
		{
			PersonaToolkit->GetPreviewScene()->InvalidateViews();
		}
	}
}

void FRMFixEditor::HandlePostReimport(UObject* InObject, bool bSuccess)
{
	if (bSuccess)
	{
		ConditionalRefreshEditor(InObject);
	}
}

//--------------------------------------------------------------------
// FModifiersTabSummoner
//--------------------------------------------------------------------

struct FModifiersTabSummoner : public FWorkflowTabFactory
{
public:
	FModifiersTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

private:
	TArray<TWeakObjectPtr<UObject>> Objects;
};

FModifiersTabSummoner::FModifiersTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects)
	: FWorkflowTabFactory(RMFixEditorTabs::ModifiersTab, InHostingApp)
	, Objects(InObjects)
{
	TabLabel = LOCTEXT("FModifiersTabSummoner_TabLabel", "Modifiers");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.Tabs.ControlRigMappingWindow");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("FModifiersTabSummoner_MenuDescription", "Modifiers");
	ViewMenuTooltip = LOCTEXT("FModifiersTabSummoner_MenuTooltip", "Setup modifiers");
}

TSharedRef<SWidget> FModifiersTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	check(Objects.Num() == 1);

	return SNew(SToolWidget, StaticCastSharedPtr<FRMFixEditor>(HostingApp.Pin()).Get());
}

//--------------------------------------------------------------------
// FPlayerTabSummoner
//--------------------------------------------------------------------

struct FPlayerTabSummoner : public FWorkflowTabFactory
{
public:
	FPlayerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

private:
	TArray<TWeakObjectPtr<UObject>> Objects;
};

FPlayerTabSummoner::FPlayerTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TArray<UObject*>& InObjects)
	: FWorkflowTabFactory(RMFixEditorTabs::PlayerTab, InHostingApp)
	, Objects(InObjects)
{
	TabLabel = LOCTEXT("FPlayerTabSummoner_TabLabel", "Time manipulation");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.Tabs.ControlRigMappingWindow");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("FPlayerTabSummoner_MenuDescription", "Time manipulation");
	ViewMenuTooltip = LOCTEXT("FPlayerTabSummoner_MenuTooltip", "Setup time manipulation");
}

TSharedRef<SWidget> FPlayerTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	check(Objects.Num() == 1);

	return StaticCastSharedPtr<FRMFixEditor>(HostingApp.Pin())->GenerateSingleObjectPlayer(Info);
}

//--------------------------------------------------------------------
// FApplicationMode_RMFixEditor_SingleAssetEditorMode
//--------------------------------------------------------------------

FApplicationMode_RMFixEditor_SingleAssetEditorMode::FApplicationMode_RMFixEditor_SingleAssetEditorMode(TSharedRef<FWorkflowCentricApplication> InHostingApp, const TSharedRef<IPersonaPreviewScene>& previewScene)
	: FApplicationMode(RMFixEditorModes::SingleAssetEditorMode)
{
	HostingAppPtr = InHostingApp;

	TSharedRef<FRMFixEditor> AnimationEditor = StaticCastSharedRef<FRMFixEditor>(InHostingApp);

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");

	FPersonaViewportArgs ViewportArgs(previewScene);
	ViewportArgs.ContextName = TEXT("AnimationEditor.Viewport");
	ViewportArgs.bAlwaysShowTransformToolbar = false;
	ViewportArgs.bShowFloorOptions = false;
	ViewportArgs.bShowLODMenu = false;
	ViewportArgs.bShowPhysicsMenu = false;
	ViewportArgs.bShowPlaySpeedMenu = false;
	ViewportArgs.bShowShowMenu = false;
	ViewportArgs.bShowStats = false;
	ViewportArgs.bShowTimeline = false;
	ViewportArgs.bShowTurnTable = false;

	PersonaModule.RegisterPersonaViewportTabFactories(TabFactories, InHostingApp, ViewportArgs);

	bool isFirst = true;
	for (auto It = TabFactories.CreateIterator(); It; ++It)
	{
		if (isFirst)
		{
			isFirst = false;
		}
		else
		{
			It.RemoveCurrent();
		}
	}

	TabFactories.RegisterFactory(MakeShareable(new FModifiersTabSummoner(InHostingApp, *AnimationEditor->GetObjectsCurrentlyBeingEdited())));
	TabFactories.RegisterFactory(MakeShareable(new FPlayerTabSummoner(InHostingApp, *AnimationEditor->GetObjectsCurrentlyBeingEdited())));

	TabLayout = FTabManager::NewLayout("FApplicationMode_RMFixEditor_SingleAssetEditorMode_v1.2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.9f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->SetHideTabWell(true)
					->AddTab(RMFixEditorTabs::ModifiersTab, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)
						->SetHideTabWell(true)
						->AddTab(RMFixEditorTabs::ViewportTab, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->SetHideTabWell(true)
						->AddTab(RMFixEditorTabs::PlayerTab, ETabState::OpenedTab)
					)
				)
			)
		);

	PersonaModule.OnRegisterTabs().Broadcast(TabFactories, InHostingApp);
	LayoutExtender = MakeShared<FLayoutExtender>();
	PersonaModule.OnRegisterLayoutExtensions().Broadcast(*LayoutExtender.Get());
	TabLayout->ProcessExtensions(*LayoutExtender.Get());
}

void FApplicationMode_RMFixEditor_SingleAssetEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FWorkflowCentricApplication> HostingApp = HostingAppPtr.Pin();
	HostingApp->RegisterTabSpawners(InTabManager.ToSharedRef());
	HostingApp->PushTabFactories(TabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FApplicationMode_RMFixEditor_SingleAssetEditorMode::AddTabFactory(FCreateWorkflowTabFactory FactoryCreator)
{
	if (FactoryCreator.IsBound())
	{
		TabFactories.RegisterFactory(FactoryCreator.Execute(HostingAppPtr.Pin()));
	}
}

void FApplicationMode_RMFixEditor_SingleAssetEditorMode::RemoveTabFactory(FName TabFactoryID)
{
	TabFactories.UnregisterFactory(TabFactoryID);
}

//--------------------------------------------------------------------
// FContentBrowserSelectionMenuExtender
//--------------------------------------------------------------------

template<class T>
class FContentBrowserSelectionMenuExtender : public IContentBrowserSelectionMenuExtender, public TSharedFromThis<FContentBrowserSelectionMenuExtender<T>>
{
public:
	FContentBrowserSelectionMenuExtender(const FText& label, const FText& toolTip, const FName styleSetName, const FName iconName)
		: Label(label), ToolTip(toolTip), StyleSetName(styleSetName), IconName(iconName)
	{}

	virtual ~FContentBrowserSelectionMenuExtender() = default;

	virtual void Extend() override
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(FContentBrowserMenuExtender_SelectedAssets::CreateSP(this, &FContentBrowserSelectionMenuExtender::CreateExtender));
	}

protected:
	virtual void Execute(TArray<T*> SelectedAssets) const = 0;

private:
	TSharedRef<FExtender> CreateExtender(const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();

		Extender->AddMenuExtension(
			"GetAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateSP(this, &FContentBrowserSelectionMenuExtender::AddMenuExtension, SelectedAssets)
		);

		return Extender;
	}

	void AddMenuExtension(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
	{
		TArray<T*> typedSelectedAssets;

		for (const FAssetData& SelectedAsset : SelectedAssets)
		{
			if (SelectedAsset.GetClass() != T::StaticClass()) return;

			typedSelectedAssets.Add(static_cast<T*>(SelectedAsset.GetAsset()));
		}

		if (typedSelectedAssets.Num() == 0) return;

		MenuBuilder.AddMenuEntry(
			Label,
			ToolTip,
			FSlateIcon(StyleSetName, IconName),
			FUIAction(FExecuteAction::CreateSP(this, &FContentBrowserSelectionMenuExtender::Execute, typedSelectedAssets), FCanExecuteAction())
		);
	}

protected:

	const FText Label;
	const FText ToolTip;
	const FName StyleSetName;
	const FName IconName;
};

//--------------------------------------------------------------------
// FContentBrowserSelectionMenuExtender_AnimSequence
//--------------------------------------------------------------------

class FContentBrowserSelectionMenuExtender_AnimSequence : public FContentBrowserSelectionMenuExtender<UAnimSequence>
{
public:
	FContentBrowserSelectionMenuExtender_AnimSequence(const FText& label, const FText& toolTip, const FName styleSetName, const FName iconName)
		: FContentBrowserSelectionMenuExtender(label, toolTip, styleSetName, iconName)
	{}

protected:
	virtual void Execute(TArray<UAnimSequence*> SelectedAssets) const override
	{
		TSharedRef<FRMFixEditor> RMFixeEditor = MakeShareable(new FRMFixEditor);
		RMFixeEditor->InitRMFixEditor(EToolkitMode::Standalone, nullptr, SelectedAssets);
	}
};

//--------------------------------------------------------------------
// FRMFixToolEditorModule
//--------------------------------------------------------------------

void FRMFixToolEditorModule::StartupModule()
{
	StartupStyle();

	ContentBrowserSelectionMenuExtenders.Add(MakeShareable(new FContentBrowserSelectionMenuExtender_AnimSequence(
		LOCTEXT("FContentBrowserSelectionMenuExtender_AnimSequence_Label", "RM Fix Tool"),
		LOCTEXT("FContentBrowserSelectionMenuExtender_AnimSequence_ToolTip", "Modify selected assets with RM Fix Tool"),
		GetStyleSetName(),
		GetContextMenuIconName()
	)));

	for (const TSharedPtr<IContentBrowserSelectionMenuExtender>& extender : ContentBrowserSelectionMenuExtenders)
	{
		if (extender.IsValid())
		{
			extender->Extend();
		}
	}
}

void FRMFixToolEditorModule::ShutdownModule()
{
	ContentBrowserSelectionMenuExtenders.Empty();

	ShutdownStyle();
}

FName FRMFixToolEditorModule::GetStyleSetName()
{
	static FName styleSetName("FRMFixToolEditorModule_StyleSet_Name");
	return styleSetName;
}

FName FRMFixToolEditorModule::GetContextMenuIconName()
{
	static FName iconName("FRMFixToolEditorModule_ContextMenu_Icon");
	return iconName;
}

FName FRMFixToolEditorModule::GetRemoveRootMotionIconName()
{
	static FName iconName("FRMFixToolEditorModule_RemoveRootMotion_Icon");
	return iconName;
}

FName FRMFixToolEditorModule::GetTransferAnimationIconName()
{
	static FName iconName("FRMFixToolEditorModule_TransferAnimation_Icon");
	return iconName;
}

FName FRMFixToolEditorModule::GetSnapIconName()
{
	static FName iconName("FRMFixToolEditorModule_Snap_Icon");
	return iconName;
}

FName FRMFixToolEditorModule::GetFixRootMotionDirectionIconName()
{
	static FName iconName("FRMFixToolEditorModule_FixRootMotionDirection_Icon");
	return iconName;
}

void FRMFixToolEditorModule::StartupStyle()
{
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon160x137(160.0f, 137.0f);

	StyleSet = MakeShareable(new FSlateStyleSet(GetStyleSetName()));

	StyleSet->SetContentRoot(IPluginManager::Get().FindPlugin("RMFixTool")->GetBaseDir() / TEXT("Resources"));

	StyleSet->Set(GetContextMenuIconName(), new FSlateVectorImageBrush(StyleSet->RootToContentDir(TEXT("PlaceholderButtonIcon"), TEXT(".svg")), Icon20x20));

	StyleSet->Set(GetRemoveRootMotionIconName(), new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("FixStartLocationOffset"), TEXT(".png")), Icon160x137));

	StyleSet->Set(GetTransferAnimationIconName(), new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("TransferAnimationBetweenBones"), TEXT(".png")), Icon160x137));

	StyleSet->Set(GetSnapIconName(), new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("SnapIcon"), TEXT(".png")), Icon160x137));

	StyleSet->Set(GetFixRootMotionDirectionIconName(), new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("FixRootMotionDirection"), TEXT(".png")), Icon160x137));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

void FRMFixToolEditorModule::ShutdownStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	ensure(StyleSet.IsUnique());

	StyleSet.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRMFixToolEditorModule, RMFixToolEditor)