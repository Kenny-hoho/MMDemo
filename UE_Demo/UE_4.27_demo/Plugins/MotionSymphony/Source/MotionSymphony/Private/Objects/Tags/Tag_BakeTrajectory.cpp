// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/Tags/Tag_BakeTrajectory.h"
#include "Data/TrajectoryPoint.h"

void UTag_BakeTrajectory::PreProcessPose(FPoseMotionData& OutPose, FMotionAnimAsset& OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Super::PreProcessPose(OutPose, OutMotionAnim, OutMotionData, StartTime, EndTime);
}

void UTag_BakeTrajectory::SaveTrajectory(FPoseMotionData& OutPose, TSharedRef<TJsonWriter<>> JsonWriter)
{
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("PoseID"), OutPose.PoseId);
	JsonWriter->WriteArrayStart(TEXT("TrajectoryPoints"));
	const TArray<FTrajectoryPoint>& currentTrajectory = OutPose.Trajectory;
	for (const FTrajectoryPoint& point : currentTrajectory) {
		JsonWriter->WriteObjectStart();

		JsonWriter->WriteObjectStart(TEXT("Position"));
		JsonWriter->WriteValue(TEXT("X"), point.Position.X);
		JsonWriter->WriteValue(TEXT("Y"), point.Position.Y);
		JsonWriter->WriteValue(TEXT("Z"), point.Position.Z);
		JsonWriter->WriteObjectEnd();

		JsonWriter->WriteObjectStart(TEXT("Rotation"));
		JsonWriter->WriteValue(TEXT("RotationZ"), point.RotationZ);
		JsonWriter->WriteObjectEnd();

		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteArrayEnd();
	JsonWriter->WriteObjectEnd();
}
