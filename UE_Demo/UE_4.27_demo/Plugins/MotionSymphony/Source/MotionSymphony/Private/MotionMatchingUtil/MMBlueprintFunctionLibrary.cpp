// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.


#include "MMBlueprintFunctionLibrary.h"
#include "MotionSymphonySettings.h"
#include "Data/Trajectory.h"
#include "Camera/CameraComponent.h"

FVector UMMBlueprintFunctionLibrary::GetInputVectorRelativeToCamera(FVector InputVector, UCameraComponent* CameraComponent)
{
	if (!CameraComponent)
	{
		return InputVector;
	}

	FRotator CameraProjectedRotation = CameraComponent->GetComponentToWorld().GetRotation().Rotator();
	CameraProjectedRotation.Roll = 0.0f;
	CameraProjectedRotation.Pitch = 0.0f;

	return CameraProjectedRotation.RotateVector(InputVector);
}


FVector UMMBlueprintFunctionLibrary::GetVectorRelativeToCamera(const float InputX, const float InputY, UCameraComponent* CameraComponent)
{
	if (!CameraComponent)
	{
		return FVector(InputX, InputY, 0.0f);
	}

	FRotator CameraProjectedRotation = CameraComponent->GetComponentToWorld().GetRotation().Rotator();
	CameraProjectedRotation.Roll = 0.0f;
	CameraProjectedRotation.Pitch = 0.0f;

	return CameraProjectedRotation.RotateVector(FVector(InputX, InputY, 0.0f));
}

FMotionTraitField UMMBlueprintFunctionLibrary::CreateMotionTraitField(const FString TraitName)
{
	UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();

	if (!Settings)
	{
		return FMotionTraitField();
	}

	int32 TraitIndex = Settings->TraitNames.IndexOfByKey(TraitName);

	if (TraitIndex > 63)
	{
		//Trait index out of range
		return FMotionTraitField();
	}

	return FMotionTraitField(TraitIndex);
}

FMotionTraitField UMMBlueprintFunctionLibrary::CreateMotionTraitFieldFromArray(const TArray<FString>& TraitNames)
{
	UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();

	if (!Settings)
	{
		return FMotionTraitField();
	}

	FMotionTraitField MotionTraits = FMotionTraitField();
	for (const FString& TraitName : TraitNames)
	{
		MotionTraits.SetTraitPosition(Settings->TraitNames.IndexOfByKey(TraitName));
	}

	return MotionTraits;
}

void UMMBlueprintFunctionLibrary::AddTrait(const FString TraitName, FMotionTraitField& OutTraitField)
{
	UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();

	if (!Settings)
	{
		return;
	}

	int32 TraitIndex = Settings->TraitNames.IndexOfByKey(TraitName);

	if (TraitIndex > 63)
	{
		//Trait index out of range
		return;
	}

	OutTraitField.SetTraitPosition(TraitIndex);
}

void UMMBlueprintFunctionLibrary::AddTraits(const TArray<FString>& TraitNames,FMotionTraitField& OutTraitField)
{
	UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();

	if (!Settings)
	{
		return;
	}

	for (const FString& TraitName : TraitNames)
	{
		int32 TraitIndex = Settings->TraitNames.IndexOfByKey(TraitName);

		if (TraitIndex > -1 && TraitIndex < 64)
		{
			OutTraitField.SetTraitPosition(TraitIndex);
		}
	}
}

void UMMBlueprintFunctionLibrary::AddTraitField(const FMotionTraitField NewTraits, FMotionTraitField& OutTraitField)
{
	OutTraitField |= NewTraits;
}

void UMMBlueprintFunctionLibrary::RemoveTrait(const FString TraitName, FMotionTraitField& OutTraitField)
{	
	UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();

	if (!Settings)
	{
		return;
	}

	int32 TraitIndex = Settings->TraitNames.IndexOfByKey(TraitName);

	if (TraitIndex > 63)
	{
		return;
	}

	OutTraitField.UnSetTraitPosition(TraitIndex);
}

void UMMBlueprintFunctionLibrary::RemoveTraits(const TArray<FString>& TraitNames, FMotionTraitField& OutTraitField)
{
	UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();

	if (!Settings)
	{
		return;
	}

	FMotionTraitField TraitsToRemove = FMotionTraitField();
	for (const FString& TraitName : TraitNames)
	{
		int32 TraitIndex = Settings->TraitNames.IndexOfByKey(TraitName);

		if (TraitIndex > -1 && TraitIndex < 64)
		{
			OutTraitField.UnSetTraitPosition(TraitIndex);
		}
	}
}

void UMMBlueprintFunctionLibrary::RemoveTraitField(const FMotionTraitField TraitsToRemove, FMotionTraitField& OutTraitField)
{
	OutTraitField.UnSetTraits(TraitsToRemove);
}

void UMMBlueprintFunctionLibrary::ClearTraitField(FMotionTraitField& OutTraitField)
{
	OutTraitField.A = OutTraitField.B = 0;
}

FMotionTraitField UMMBlueprintFunctionLibrary::GetTraitHandle(const FString TraitName)
{
	UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();

	if (!Settings)
	{
		return FMotionTraitField();
	}

	int32 TraitIndex = Settings->TraitNames.IndexOfByKey(TraitName);

	if (TraitIndex > 63)
	{
		return FMotionTraitField();
	}

	return FMotionTraitField(TraitIndex);
}



FMotionTraitField UMMBlueprintFunctionLibrary::GetTraitHandleFromArray(const TArray<FString>& TraitNames)
{
	UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();

	if (!Settings)
	{
		return FMotionTraitField();
	}

	FMotionTraitField MotionTraits = FMotionTraitField();
	for (const FString& TraitName : TraitNames)
	{
		int32 TraitIndex = Settings->TraitNames.IndexOfByKey(TraitName);

		if (TraitIndex > -1 && TraitIndex < 64)
		{
			MotionTraits.SetTraitPosition(TraitIndex);
		}
	}

	return MotionTraits;
}

void UMMBlueprintFunctionLibrary::InitializeTrajectory(FTrajectory& OutTrajectory, const int32 TrajectoryCount)
{
	OutTrajectory.TrajectoryPoints.Empty(TrajectoryCount + 1);

	for (int32 i = 0; i < TrajectoryCount; ++i)
	{
		OutTrajectory.TrajectoryPoints.Emplace(FTrajectoryPoint(FVector::ZeroVector, 0.0f));
	}
}

void UMMBlueprintFunctionLibrary::SetTrajectoryPoint(FTrajectory& OutTrajectory, const int32 Index, const FVector Position, const float RotationZ)
{
	if(Index < 0 || Index > OutTrajectory.TrajectoryPointCount() - 1)
		return;

	OutTrajectory.TrajectoryPoints[Index] = FTrajectoryPoint(Position, RotationZ);
}
