// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EMMPreProcessEnums.generated.h"

UENUM(BlueprintType)
enum class ETrajectoryPreProcessMethod : uint8
{
	None,
	IgnoreEdges,
	Extrapolate,
	Animation
};

UENUM(BlueprintType)
enum class EJointVelocityCalculationMethod : uint8
{
	BodyIndependent UMETA(DisplayName = "Body Independent"),
	BodyDependent UMETA(DisplayName = "Body Dependent")
};