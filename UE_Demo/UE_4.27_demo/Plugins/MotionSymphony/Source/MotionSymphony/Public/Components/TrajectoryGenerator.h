// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TrajectoryGenerator_Base.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "TrajectoryGenerator.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Category = "Motion Matching", meta = (BlueprintSpawnableComponent))
class MOTIONSYMPHONY_API UTrajectoryGenerator : public UTrajectoryGenerator_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotonSettings")
	float MaxSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotonSettings")
	float MoveResponse;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotonSettings")
	float TurnResponse;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionSettings")
	FVector StrafeDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	bool bResetDirectionOnIdle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviour")
	ETrajectoryMoveMode TrajectoryBehaviour;

	// recording predict trajectory
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording")
	float TotalRecordTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording")
	bool bShouldRecord;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recording")
	int RecordNum;
	
private:
	TArray<FVector> NewTrajPosition;
	float LastDesiredOrientation;

	float MoveResponse_Remapped;
	float TurnResponse_Remapped;

	// recording predict trajectory
	float TimeSinceLastPredictRecord;
	float TimeSinceFirstRecord;
	TArray<FTrajectory> RecordedTrajectories;

public:
	UTrajectoryGenerator();

protected:
	virtual void UpdatePrediction(float DeltaTime) override;
	virtual void Setup(TArray<float>& TrajTimes);

private:
	void CalculateDesiredLinearVelocity(FVector& OutVelocity);
	
	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|TrajectoryGenerator")
	void SetStrafeDirectionFromCamera(UCameraComponent* Camera);
};