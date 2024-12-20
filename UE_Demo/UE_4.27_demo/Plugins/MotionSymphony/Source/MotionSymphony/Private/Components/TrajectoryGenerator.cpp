// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "TrajectoryGenerator.h"
#include "Camera/CameraComponent.h"
#include "Data/InputProfile.h"
#include "MotionMatchingUtil/MotionMatchingUtils.h"

#define EPSILON 0.0001f

UTrajectoryGenerator::UTrajectoryGenerator()
	: MaxSpeed(400.0f),
	MoveResponse(15.0f),
	TurnResponse(15.0f),
	StrafeDirection(FVector(0.0f)),
	bResetDirectionOnIdle(true),
	LastDesiredOrientation(0.0f),
	MoveResponse_Remapped(15.0f),
	TurnResponse_Remapped(15.0f),
	TotalRecordTime(5.0f),
	RecordNum(20),
	TimeSinceLastPredictRecord(0.0f),
	TimeSinceFirstRecord(0.0f),
	bShouldRecord(false)
{
}


void UTrajectoryGenerator::UpdatePrediction(float DeltaTime)
{
	FVector DesiredLinearVelocity;
	CalculateDesiredLinearVelocity(DesiredLinearVelocity);

	const FVector DesiredLinearDisplacement = DesiredLinearVelocity / FMath::Max(EPSILON, SampleRate);

	float DesiredOrientation = 0.0f;
	if (TrajectoryBehaviour != ETrajectoryMoveMode::Standard)
	{
		DesiredOrientation = FMath::RadiansToDegrees(FMath::Atan2(StrafeDirection.Y, StrafeDirection.X));
	}
	else if (DesiredLinearDisplacement.SizeSquared() > EPSILON)
	{
		DesiredOrientation = FMath::RadiansToDegrees(FMath::Atan2(
			DesiredLinearDisplacement.Y, DesiredLinearDisplacement.X));
	}
	else
	{
		if(bResetDirectionOnIdle)
		{
			DesiredOrientation = OwningActor->GetActorRotation().Euler().Z;
		}
		else
		{
			DesiredOrientation = LastDesiredOrientation;
		}
	}

	LastDesiredOrientation = DesiredOrientation;

	NewTrajPosition[0] = FVector::ZeroVector;
	TrajRotations[0] = 0.0f;

	const int32 Iterations = TrajPositions.Num();
	float CumRotation = 0.0f;

	for (int32 i = 1; i < Iterations; ++i)
	{
		const float Percentage = (float)i / FMath::Max(1.0f, (float)(Iterations - 1));
		FVector TrajDisplacement = TrajPositions[i] - TrajPositions[i-1];

		FVector AdjustedTrajDisplacement = FMath::Lerp(TrajDisplacement, DesiredLinearDisplacement,
			1.0f - FMath::Exp((-MoveResponse_Remapped * DeltaTime) * Percentage));

		NewTrajPosition[i] = NewTrajPosition[i - 1] + AdjustedTrajDisplacement;

		//TrajRotations[i] = DesiredOrientation;

		TrajRotations[i] = FMath::RadiansToDegrees(FMotionMatchingUtils::LerpAngle(
			FMath::DegreesToRadians(TrajRotations[i]),
			FMath::DegreesToRadians(DesiredOrientation) ,
			1.0f - FMath::Exp((-TurnResponse_Remapped * DeltaTime) * Percentage)));
	}

	for (int32 i = 0; i < Iterations; ++i)
	{
		TrajPositions[i] = NewTrajPosition[i];
	}


	// Recording Predict Trajectory
	if (bShouldRecord) {
		TimeSinceLastPredictRecord += DeltaTime;
		TimeSinceFirstRecord += DeltaTime;

		float recordInterval = TotalRecordTime / RecordNum;

		if (TimeSinceLastPredictRecord > recordInterval) {
			// Add current Trajectory
			RecordedTrajectories.Add(GetCurrentTrajectory());
			TimeSinceLastPredictRecord = 0.0f;
		}
		if (TimeSinceFirstRecord > TotalRecordTime) {
			// Save to Json
			FString JsonStr = "";
			FString FilePath = FPaths::ProjectPluginsDir() + TEXT("JsonData/PredictTrajectoryFromComponent.json");
			TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonStr);
			JsonWriter->WriteObjectStart();
			JsonWriter->WriteValue(TEXT("PoseCount"), RecordNum - 1);
			JsonWriter->WriteArrayStart(TEXT("Trajectory"));
			for (FTrajectory& traj : RecordedTrajectories) {
				JsonWriter->WriteObjectStart();
				JsonWriter->WriteArrayStart(TEXT("TrajectoryPoints"));
				for (FTrajectoryPoint& trajPoint : traj.TrajectoryPoints) {
					JsonWriter->WriteObjectStart();

					JsonWriter->WriteObjectStart(TEXT("Position"));
					JsonWriter->WriteValue(TEXT("X"), trajPoint.Position.X);
					JsonWriter->WriteValue(TEXT("Y"), trajPoint.Position.Y);
					JsonWriter->WriteValue(TEXT("Z"), trajPoint.Position.Z);
					JsonWriter->WriteObjectEnd();

					JsonWriter->WriteObjectStart(TEXT("Rotation"));
					JsonWriter->WriteValue(TEXT("RotationZ"), trajPoint.RotationZ);
					JsonWriter->WriteObjectEnd();

					JsonWriter->WriteObjectEnd();
				}
				JsonWriter->WriteArrayEnd();
				JsonWriter->WriteObjectEnd();
			}
			JsonWriter->WriteArrayEnd();
			JsonWriter->WriteObjectEnd();
			JsonWriter->Close();
			FFileHelper::SaveStringToFile(JsonStr, *FilePath);

			GEngine->AddOnScreenDebugMessage(-1, 3, FColor::Red, TEXT("Recored From Component Finished!"));

			TimeSinceFirstRecord = 0.0f;
			bShouldRecord = false;
		}
	}
}

void UTrajectoryGenerator::Setup(TArray<float>& InTrajTimes)
{
	NewTrajPosition.Empty(TrajectoryIterations);

	FVector ActorPosition = OwningActor->GetActorLocation();
	for (int32 i = 0; i < TrajectoryIterations; ++i)
	{
		NewTrajPosition.Emplace(ActorPosition);
	}
}

void UTrajectoryGenerator::CalculateDesiredLinearVelocity(FVector & OutVelocity)
{
	MoveResponse_Remapped = MoveResponse;
	TurnResponse_Remapped = TurnResponse;

	if (InputProfile != nullptr)
	{
		const FInputSet* InputSet = InputProfile->GetInputSet(InputVector);

		if (InputSet != nullptr)
		{
			InputVector.Normalize();
			InputVector *= InputSet->SpeedMultiplier;

			MoveResponse_Remapped = MoveResponse * InputSet->MoveResponseMultiplier;
			TurnResponse_Remapped = TurnResponse * InputSet->TurnResponseMultiplier;
		}
	}

	if(InputVector.SizeSquared() > 1.0f)
	{
		InputVector.Normalize();
	}

	OutVelocity = FVector(InputVector.X, InputVector.Y, 0.0f) * MaxSpeed;
}

void UTrajectoryGenerator::SetStrafeDirectionFromCamera(UCameraComponent* Camera)
{
	if (!Camera)
	{
		return;
	}

	StrafeDirection = FMath::Lerp(StrafeDirection, Camera->GetForwardVector(), 0.5f);
	StrafeDirection.Z = 0.0f;

	StrafeDirection.Normalize();
}