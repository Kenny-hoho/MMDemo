// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "Data/CalibrationData.h"
#include "Data/MotionTraitField.h"
#include "CustomAssets/MotionDataAsset.h"
#include "CustomAssets/MotionMatchConfig.h"

FCalibrationData::FCalibrationData()
	: Weight_Momentum(1.0f),
	  Weight_AngularMomentum(1.0f)
{
	PoseJointWeights.Empty(5);
	TrajectoryWeights.Empty(5);
}

FCalibrationData::FCalibrationData(UMotionDataAsset* SourceMotionData)
	: Weight_Momentum(1.0f),
	  Weight_AngularMomentum(1.0f)
{
	if(!SourceMotionData)
	{
		PoseJointWeights.Empty(5);
		TrajectoryWeights.Empty(5);
		return;
	}

	UMotionMatchConfig* MMConfig = SourceMotionData->MotionMatchConfig;

	if(!MMConfig)
		return;

	PoseJointWeights.Empty(MMConfig->PoseBones.Num() + 1);
	TrajectoryWeights.Empty(MMConfig->TrajectoryTimes.Num() + 1);

	for (int32 i = 0; i < MMConfig->PoseBones.Num(); ++i)
	{
		PoseJointWeights.Emplace(FJointWeightSet());
	}

	for (int32 i = 0; i < MMConfig->TrajectoryTimes.Num(); ++i)
	{
		TrajectoryWeights.Emplace(FTrajectoryWeightSet());
	}
}

FCalibrationData::FCalibrationData(UMotionMatchConfig* SourceConfig)
{
	Initialize(SourceConfig);
}

FCalibrationData::FCalibrationData(int32 PoseJointCount, int32 TrajectoryCount)
	: Weight_Momentum(1.0f),
	Weight_AngularMomentum(1.0f)
{
	PoseJointCount = FMath::Clamp(PoseJointCount, 0, 100);
	TrajectoryCount = FMath::Clamp(TrajectoryCount, 0, 100);

	PoseJointWeights.Empty(PoseJointCount + 1);
	TrajectoryWeights.Empty(TrajectoryCount + 1);

	for (int32 i = 0; i < PoseJointCount; ++i)
	{
		PoseJointWeights.Emplace(FJointWeightSet());
	}

	for (int32 i = 0; i < TrajectoryCount; ++i)
	{
		TrajectoryWeights.Emplace(FTrajectoryWeightSet());
	}
}

void FCalibrationData::Initialize(UMotionMatchConfig* SourceConfig)
{
	if (SourceConfig == nullptr)
	{
		PoseJointWeights.Empty(5);
		TrajectoryWeights.Empty(5);
		return;
	}

	Weight_Momentum = 1.0f;
	Weight_AngularMomentum = 1.0f;

	PoseJointWeights.Empty(SourceConfig->PoseBones.Num() + 1);
	TrajectoryWeights.Empty(SourceConfig->TrajectoryTimes.Num() + 1);

	for (int32 i = 0; i < SourceConfig->PoseBones.Num(); ++i)
	{
		PoseJointWeights.Add(FJointWeightSet());
	}

	for (int32 i = 0; i < SourceConfig->TrajectoryTimes.Num(); ++i)
	{
		TrajectoryWeights.Add(FTrajectoryWeightSet());
	}
}

bool FCalibrationData::IsValidWithConfig(UMotionMatchConfig* MotionConfig)
{
	if (!MotionConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("FCalibrationData: Trying to call IsValidWithConfig(UMotionMatchConfig*) but the motion config passed is null"));
		return false;
	}

	bool bValid = true;

	if (PoseJointWeights.Num() != MotionConfig->PoseBones.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("Calibration Data is not valid. There is a mis-match between calibration data and motion config in terms of the number of bones being matched. Did you forget to re-preprocess your animation data?"));
		bValid = false;
	}

	if (TrajectoryWeights.Num() != MotionConfig->TrajectoryTimes.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("Calibration Data is not valid. There is a mis-match between calibration data and motion config in terms of the number of trajectory points being matched. Did you forget to re-preprocess your animation data?"));
		bValid = false;
	}

	return bValid;
}

void FCalibrationData::GenerateStandardDeviationWeights(const UMotionDataAsset* SourceMotionData, const FMotionTraitField& MotionTrait)
{
	if (!SourceMotionData || !SourceMotionData->MotionMatchConfig)
	{
		return;
	}

	UMotionMatchConfig* MMConfig = SourceMotionData->MotionMatchConfig;

	Initialize(MMConfig);

	int32 PoseCount = 0;

	//For momentum and angular momentum
	FVector TotalMomentum = FVector::ZeroVector;
	float TotalAngularMomentum = 0.0f;

	for (const FPoseMotionData& Pose : SourceMotionData->Poses)
	{
		if (Pose.bDoNotUse || Pose.Traits != MotionTrait)
			continue;

		++PoseCount;

		TotalMomentum += Pose.LocalVelocity;
		TotalAngularMomentum += Pose.RotationalVelocity;
	}

	FVector MeanMomentum = TotalMomentum / PoseCount;
	float MeanAngularMomentum = TotalAngularMomentum / PoseCount;

	//Sum the total of all atom distances squared to the mean
	float TotalDistanceToMeanSquared_Momentum = 0.0f;
	float TotalDistanceToMeanSquared_AngularMomentum = 0.0f;

	for (const FPoseMotionData& Pose : SourceMotionData->Poses)
	{
		if (Pose.bDoNotUse)
			continue;

		float DistanceToMean_Momentum = FVector::DistSquared(Pose.LocalVelocity, MeanMomentum);
		float DistanceToMean_AngularMomentum = FMath::Abs(Pose.RotationalVelocity - MeanAngularMomentum);

		TotalDistanceToMeanSquared_Momentum += DistanceToMean_Momentum * DistanceToMean_Momentum;
		TotalDistanceToMeanSquared_AngularMomentum += DistanceToMean_AngularMomentum * DistanceToMean_AngularMomentum;
	}

	Weight_Momentum = 1.0f / FMath::Sqrt(TotalDistanceToMeanSquared_Momentum / PoseCount);
	Weight_AngularMomentum = 1.0f / FMath::Sqrt(TotalDistanceToMeanSquared_AngularMomentum / PoseCount);

	//For Each Bone
	for (int32 i = 0; i < MMConfig->PoseBones.Num(); ++i)
	{
		//Sum up magnitude of all pose atoms
		FVector TotalPosition =  FVector::ZeroVector;
		FVector TotalVelocity = FVector::ZeroVector;

		for (const FPoseMotionData& Pose : SourceMotionData->Poses)
		{
			if(Pose.bDoNotUse)
				continue;

			const FJointData& PoseJointData = Pose.JointData[i];

			TotalPosition += PoseJointData.Position;
			TotalVelocity += PoseJointData.Velocity;
		}

		//Find Mean
		FVector MeanPosition = TotalPosition / PoseCount;
		FVector MeanVelocity = TotalPosition / PoseCount;

		//Sum the total of all atom distances squared to the mean
		float TotalDistanceToMeanSquared_Position = 0.0f;
		float TotalDistanceToMeanSquared_Velocity = 0.0f;

		for (const FPoseMotionData& Pose : SourceMotionData->Poses)
		{
			if (Pose.bDoNotUse)
				continue;

			const FJointData& PoseJointData = Pose.JointData[i];

			float DistanceToMean_Position = FVector::DistSquared(PoseJointData.Position, MeanPosition);
			float DistanceToMean_Velocity = FVector::DistSquared(PoseJointData.Velocity, MeanVelocity);

			TotalDistanceToMeanSquared_Position += DistanceToMean_Position * DistanceToMean_Position;
			TotalDistanceToMeanSquared_Velocity += DistanceToMean_Velocity * DistanceToMean_Velocity;
		}

		FJointWeightSet& StdDevWeightSet = PoseJointWeights[i];

		//Set the standard deviation of these features
		StdDevWeightSet.Weight_Pos = 1.0f / FMath::Sqrt(TotalDistanceToMeanSquared_Position / PoseCount);
		StdDevWeightSet.Weight_Vel = 1.0f / FMath::Sqrt(TotalDistanceToMeanSquared_Velocity / PoseCount);
	}

	//For Each Trajectory Point
	for (int32 i = 0; i < MMConfig->TrajectoryTimes.Num(); ++i)
	{
		FVector TotalPosition = FVector::ZeroVector;
		float TotalFacingAngle = 0.0f;

		for (const FPoseMotionData& Pose : SourceMotionData->Poses)
		{
			if (Pose.bDoNotUse)
				continue;

			const FTrajectoryPoint& TrajPoint = Pose.Trajectory[i];

			TotalPosition += TrajPoint.Position;
			TotalFacingAngle += TrajPoint.RotationZ;
		}

		//Find Mean
		FVector MeanPosition = TotalPosition / PoseCount;
		float MeanFacing = TotalFacingAngle / PoseCount;

		//Sum the total of all atom distances squared to the mean
		float TotalDistanceToMeanSquared_Position = 0.0f;
		float TotalDistanceToMeanSquared_Facing = 0.0f;

		for (const FPoseMotionData& Pose : SourceMotionData->Poses)
		{
			if (Pose.bDoNotUse)
				continue;

			const FTrajectoryPoint& TrajPoint = Pose.Trajectory[i];

			float DistanceToMean_Position = FVector::DistSquared(TrajPoint.Position, MeanPosition);
			float DistanceToMean_Facing = FMath::Abs(TrajPoint.RotationZ - MeanFacing);

			TotalDistanceToMeanSquared_Position += DistanceToMean_Position * DistanceToMean_Position;
			TotalDistanceToMeanSquared_Facing += DistanceToMean_Facing * DistanceToMean_Facing;
		}

		FTrajectoryWeightSet& StdDevWeightSet = TrajectoryWeights[i];

		//Set the standard deviation of these features
		StdDevWeightSet.Weight_Pos = 1.0f / FMath::Sqrt(TotalDistanceToMeanSquared_Position / PoseCount);
		StdDevWeightSet.Weight_Facing = 1.0f / FMath::Sqrt(TotalDistanceToMeanSquared_Facing / PoseCount);
	}
}

void FCalibrationData::GenerateFinalWeights(const UMotionCalibration* SourceCalibration, const FCalibrationData& StdDeviationNormalizers)
{
	if (!SourceCalibration)
	{
		return;
	}

	Initialize(SourceCalibration->MotionMatchConfig);

	float TrajMultiplier = SourceCalibration->QualityVsResponsivenessRatio * 2.0f;
	float PoseMultiplier = (1.0f - SourceCalibration->QualityVsResponsivenessRatio) * 2.0f;

	Weight_Momentum = SourceCalibration->Weight_Momentum * PoseMultiplier * StdDeviationNormalizers.Weight_Momentum;
	Weight_AngularMomentum = SourceCalibration->Weight_AngularMomentum * PoseMultiplier * StdDeviationNormalizers.Weight_Momentum;

	PoseJointWeights.Empty(SourceCalibration->PoseJointWeights.Num() + 1);
	TrajectoryWeights.Empty(SourceCalibration->TrajectoryWeights.Num() + 1);

	for (int32 i = 0; i < SourceCalibration->PoseJointWeights.Num(); ++i)
	{
		FJointWeightSet CalibWeightSet = SourceCalibration->PoseJointWeights[i];
		CalibWeightSet.Weight_Pos  *= PoseMultiplier;
		CalibWeightSet.Weight_Vel *= PoseMultiplier;
		FJointWeightSet StdDevWeightSet = StdDeviationNormalizers.PoseJointWeights[i];

		PoseJointWeights.Add(CalibWeightSet * StdDevWeightSet);
	}

	for (int32 i = 0; i < SourceCalibration->TrajectoryWeights.Num(); ++i)
	{
		FTrajectoryWeightSet CalibWeightSet = SourceCalibration->TrajectoryWeights[i];
		CalibWeightSet.Weight_Pos *= PoseMultiplier;
		CalibWeightSet.Weight_Facing *= TrajMultiplier;
		FTrajectoryWeightSet StdDevWeightSet = StdDeviationNormalizers.TrajectoryWeights[i];

		TrajectoryWeights.Add(CalibWeightSet * StdDevWeightSet);
	}
}