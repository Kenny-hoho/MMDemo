// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "Components/TrajectoryGenerator_Base.h"
#include "DrawDebugHelpers.h"
#include "Engine.h"
#include "Data/InputProfile.h"
#include "Logging/LogMacros.h"

#define EPSILON 0.0001f
#define THIRTY_HZ 1.0f / 30.0f

// Sets default values for this component's properties
UTrajectoryGenerator_Base::UTrajectoryGenerator_Base()
	: MotionMatchConfig(nullptr) ,
	  RecordingFrequency(0.0f),
	  SampleRate(20.0f), 
	  bFlattenTrajectory(false),
	  bDebugRandomInput(false),
	  DebugTimeIntervalRange(FVector2D(1.5f, 5.0f)),
	  Trajectory(FTrajectory()),
	  InputVector(FVector2D(0.0f)),
	  MaxRecordTime(1.0f),
	  TimeSinceLastRecord(0.0f),
	  CumActiveTime(0.0f),
	  TimeHorizon(0.0f), 
	  TimeStep(0.0f), 
	  TrajectoryIterations(0),
	  CurFacingAngle(0.0f),  
	  TimeSinceLastDebugInputChange(0.0f),
	  TimeToChangeDebugInput(0.0f),
	  DebugInputVector(FVector::ZeroVector),
	  OwningActor(nullptr), 
	  InputProfile(nullptr),
	  bExtractedThisFrame(false),
	  CacheActorTransform(FTransform::Identity)
{
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UTrajectoryGenerator_Base::BeginPlay()
{
	Super::BeginPlay();

	if (!MotionMatchConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("TrajectoryGenerator_Base: Cannot BeginPlay with null MotionMatchConfig, please make sure the motion data is set on the component."));
		return;
	}

	OwningActor = GetOwner();

	if (!OwningActor)
	{
		UE_LOG(LogTemp, Error, TEXT("TrajectoryGenerator_Base: Cannot BeginPlay with null OwningActor."));
		return;
	}
		
	TrajTimes = TArray<float>(MotionMatchConfig->TrajectoryTimes);
		
	int32 TrajCount = TrajTimes.Num();

	if (TrajCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("TrajectoryGenerator_Base: Cannot BeginPlay with zero trajectory. Please add trajectory points to your motion data and re-preprocess it."));
		return;
	}

	//Setup Goal container and prediction parameters
	Trajectory.Initialize(TrajCount);

	TimeHorizon = TrajTimes.Last();
	SampleRate = FMath::Max(SampleRate, 0.001f);
	TimeStep = TimeHorizon / SampleRate;

	//Determine parameters for past trajectory recording
	if (TrajTimes[0] < 0.0f)
	{
		MaxRecordTime = FMath::Abs(TrajTimes[0] + 0.05f);
	}

	int32 PastTrajCount = 0;
	for (int32 i = 0; i < TrajCount; ++i)
	{
		if(TrajTimes[i] < 0.0f)
			++PastTrajCount;
	}

	if (RecordingFrequency <= EPSILON)
	{
		RecordingFrequency = THIRTY_HZ;
	}

	const int32 MaxPastRecordings = FMath::CeilToInt(MaxRecordTime / RecordingFrequency);
	RecordedPastPositions.Empty(MaxPastRecordings + 1);
	RecordedPastRotations.Empty(MaxPastRecordings + 1);
	RecordedPastTimes.Empty(MaxPastRecordings + 1);
	CumActiveTime = 0.0f;
	TimeSinceLastRecord = 0.0f;

	FVector StartPos = OwningActor->GetActorLocation();
	float StartRot = OwningActor->GetActorRotation().Euler().Z;
	for (int32 i = 0; i < MaxPastRecordings; ++i)
	{
		RecordedPastPositions.Emplace(StartPos); 
		RecordedPastRotations.Emplace(StartRot); 
		RecordedPastTimes.Emplace(TimeStep * i);
	}

	//Setup containers for storing future trajectory
	TrajectoryIterations = FMath::FloorToInt(TimeHorizon * SampleRate);

	TrajPositions.Empty(TrajectoryIterations + 1);
	TrajRotations.Empty(TrajectoryIterations + 1);

	for (int32 i = 0; i < TrajectoryIterations; ++i)
	{
		TrajPositions.Emplace(FVector::ZeroVector);
		TrajRotations.Emplace(0.0f);
	}
	
	Setup(TrajTimes);
}

void UTrajectoryGenerator_Base::Setup(TArray<float>& InTrajTimes)
{

}

void UTrajectoryGenerator_Base::SetTrajectoryInputX(float XAxisValue)
{
	InputVector.X = XAxisValue;
	//ClampInputVector();
}

void UTrajectoryGenerator_Base::SetTrajectoryInputY(float YAxisValue)
{
	InputVector.Y = YAxisValue;
	//ClampInputVector();
}

void UTrajectoryGenerator_Base::SetTrajectoryInput(float XAxisValue, float YAxisValue)
{
	InputVector = FVector2D(XAxisValue, YAxisValue);
	//ClampInputVector();
}

void UTrajectoryGenerator_Base::SetInputProfile(FInputProfile& InInputProfile)
{
	InputProfile = &InInputProfile;
}

void UTrajectoryGenerator_Base::ClearInputProfile()
{
	InputProfile = nullptr;
}

inline void UTrajectoryGenerator_Base::ClampInputVector()
{
	if (InputVector.SizeSquared() > 1.0f)
	{
		InputVector.Normalize();
	}
}

FTrajectory & UTrajectoryGenerator_Base::GetCurrentTrajectory()
{
	if(!bExtractedThisFrame)
	{
		ExtractTrajectory();
	}

	return Trajectory;
}

bool UTrajectoryGenerator_Base::IsIdle()
{
	FVector TotalTrajectory = FVector::ZeroVector;
	for (int32 i = 0; i < TrajTimes.Num(); ++i)
	{
		if (TrajTimes[i] > 0.0f)
		{
			TotalTrajectory += Trajectory.TrajectoryPoints[i].Position;
		}
	}

	return TotalTrajectory.SizeSquared() < 0.5f;
}

bool UTrajectoryGenerator_Base::HasMoveInput()
{
	return InputVector.SizeSquared() > EPSILON;
}

// Called every frame
void UTrajectoryGenerator_Base::TickComponent(float DeltaTime, ELevelTick TickType, 
	FActorComponentTickFunction* ThisTickFunction)
{
	if(!MotionMatchConfig)
	{
		return;
	}

	if(!OwningActor)
	{
		return;
	}

	bExtractedThisFrame = false;

	RecordPastTrajectory(DeltaTime);
	CurFacingAngle = OwningActor->GetActorRotation().Euler().Z;

	CacheActorTransform = OwningActor->GetActorTransform() * FQuat(FVector::UpVector, -PI / 2.0f);

	if (bDebugRandomInput)
	{
		ApplyDebugInput(DeltaTime);
	}

	UpdatePrediction(DeltaTime);
}

void UTrajectoryGenerator_Base::RecordPastTrajectory(float DeltaTime)
{
	if(!OwningActor)
	{
		return;
	}

	TimeSinceLastRecord += DeltaTime;
	CumActiveTime += DeltaTime;

	if (TimeSinceLastRecord > RecordingFrequency)
	{
		if (CumActiveTime > MaxRecordTime + RecordingFrequency)
		{
			RecordedPastPositions.Pop();
			RecordedPastRotations.Pop();
			RecordedPastTimes.Pop();
		}

		RecordedPastPositions.Insert(OwningActor->GetActorLocation(), 0);
		RecordedPastRotations.Insert(OwningActor->GetActorRotation().Euler().Z, 0);
		RecordedPastTimes.Insert(CumActiveTime, 0);

		TimeSinceLastRecord = 0.0f;
	}
}

void UTrajectoryGenerator_Base::ExtractTrajectory()
{
	if(!OwningActor)
	{
		return;
	}

	const FVector ActorPosition = OwningActor->GetActorTransform().GetLocation();

	for (int32 i = 0; i < Trajectory.TrajectoryPoints.Num(); ++i)
	{
		const float TimeDelay = TrajTimes[i];

		if (TimeDelay < 0.0f)
		{
			//Past trajectory extraction
			int32 CurrentIndex = 1;
			float Lerp = 0.0f;
			const float CurrentTime = CumActiveTime;

			for (int32 k = 1; k < RecordedPastTimes.Num(); ++k)
			{
				const float Time = RecordedPastTimes[k];

				if (Time < CurrentTime + TimeDelay)
				{
					CurrentIndex = k;

					const float TimeError = (CurrentTime + TimeDelay) - Time;
					const float DeltaTime = FMath::Max( 0.00001f, RecordedPastTimes[k - 1] - Time);

					Lerp = TimeError / DeltaTime;
					break;
				}
			}

			if (CurrentIndex < RecordedPastTimes.Num())
			{
				FVector Position = FMath::Lerp(RecordedPastPositions[CurrentIndex], RecordedPastPositions[CurrentIndex - 1], Lerp);

				FQuat QuatA = FQuat(FVector::UpVector, FMath::DegreesToRadians(RecordedPastRotations[CurrentIndex]));
				FQuat QuatB = FQuat(FVector::UpVector, FMath::DegreesToRadians(RecordedPastRotations[CurrentIndex - 1.0f]));

				const float FacingAngle = FQuat::FastLerp(QuatA, QuatB, Lerp).Euler().Z;

				if(bFlattenTrajectory)
				{
					Position.Z = ActorPosition.Z;
				}

				Trajectory.TrajectoryPoints[i] = FTrajectoryPoint(Position - ActorPosition, FacingAngle);
			}
			else
			{
				Trajectory.TrajectoryPoints[i] = FTrajectoryPoint(RecordedPastPositions.Last(), RecordedPastRotations.Last());
			}
		}
		else
		{
			//Future trajectory extraction
			int32 Index = FMath::RoundToInt(TimeDelay / TimeHorizon * TrajPositions.Num() - 1);

			Index = FMath::Clamp(Index, 0, TrajPositions.Num() - 1);

			FVector Position = TrajPositions[Index];

			if(bFlattenTrajectory)
			{
				Position.Z = 0.0f;
			}

			Trajectory.TrajectoryPoints[i] = FTrajectoryPoint(Position, TrajRotations[Index]);
		}
	}

	Trajectory.MakeRelativeTo(CacheActorTransform);
	
	bExtractedThisFrame = true;
}

void UTrajectoryGenerator_Base::UpdatePrediction(float DeltaTime) {}

void UTrajectoryGenerator_Base::ApplyDebugInput(float DeltaTime)
{
	TimeSinceLastDebugInputChange += DeltaTime;
	if (TimeSinceLastDebugInputChange > TimeToChangeDebugInput)
	{
		const float RandomDirectionAngle = FMath::RandRange(-180.0f, 180.0f);
		const float RandomMagnitude = FMath::RandRange(0.0f, 1.0f);

		const FRotator RandomRotation(0.0f, RandomDirectionAngle, 0.0f);
		const FVector DebugInputVector3 = RandomRotation.Vector() * RandomMagnitude;

		DebugInputVector = FVector2D(DebugInputVector3.X, DebugInputVector3.Y);

		TimeSinceLastDebugInputChange = 0.0f;
		TimeToChangeDebugInput = FMath::RandRange(DebugTimeIntervalRange.X, DebugTimeIntervalRange.Y);
	}

	InputVector = DebugInputVector;
}

void UTrajectoryGenerator_Base::DrawTrajectoryDebug(const FVector DrawOffset)
{
	if(Trajectory.TrajectoryPoints.Num() < 2
	|| !OwningActor)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector LastPoint;
	const FVector ActorLocation = OwningActor->GetActorLocation() + DrawOffset;
	const FTransform ActorTransform = Cast<ACharacter>(OwningActor)->GetMesh()->GetComponentTransform();

	//ActorTransform.SetLocation(ActorTransform.GetLocation() + DrawOffset);
	for (int32 i = 0; i < Trajectory.TrajectoryPoints.Num(); ++i)
	{
		FColor Color = FColor::Green;
		if (TrajTimes[i] < 0.0f)
		{
			Color = FColor(0, 128, 0);
		}

		FTrajectoryPoint& TrajPoint = Trajectory.TrajectoryPoints[i];

		FVector PointPosition = ActorTransform.TransformPosition(TrajPoint.Position);
		DrawDebugSphere(World, PointPosition, 5.0f, 32, Color, false, -1.0f);

		FQuat ArrowRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(TrajPoint.RotationZ));
		FVector DrawTo = PointPosition + (ArrowRotation * ActorTransform.TransformVector(FVector::ForwardVector) * 30.0f);

		DrawDebugDirectionalArrow(World, PointPosition, DrawTo,
			40.0f, Color, false, -1.0f, 0, 2.0f);

		if(i > 0)
		{
			if (TrajTimes[i - 1] < 0.0f && TrajTimes[i] > 0.0f)
			{
				DrawDebugLine(World, LastPoint, ActorLocation, FColor::Blue, false, -1.0f, 0, 2.0f);
				DrawDebugLine(World, ActorLocation, PointPosition, FColor::Blue, false, -1.0f, 0, 2.0f);
				DrawDebugSphere(World, ActorLocation, 5.0f, 32, FColor::Blue, false, -1.0f);
			}
			else
			{
				DrawDebugLine(World, LastPoint, PointPosition,
					Color, false, -1.0f, 0, 2.0f);
			}
		}

		LastPoint = PointPosition;
	}
}
