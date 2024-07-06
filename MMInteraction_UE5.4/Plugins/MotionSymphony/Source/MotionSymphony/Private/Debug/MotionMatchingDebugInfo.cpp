// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/MotionMatchingDebugInfo.h"

AMotionMatchingDebugInfo::AMotionMatchingDebugInfo()
{
	timepassed = 0.0f;

	PoseDebugLevel = 0;
	TrajDebugLevel = 0;
	SearchDebugLevel = 0;

	PoseSearchCountsForDebug.Empty(11);
	for (int32 i = 0; i < 30; ++i)
	{
		PoseSearchCountsForDebug.Add(0);
	}
	LowestCostCandidates.Empty(1);
	LowestCostCandidates.Add(FPoseCostInfo());

	bReplicates = true;
}

FPoseCostInfo::FPoseCostInfo()
{
	PoseId = 0;
	AnimName = FString("Invalid");
	AnimId = 0;
	Favour = 1;
	CurrentPoseFavour = 1;
	TotalCost = 1000000;
}