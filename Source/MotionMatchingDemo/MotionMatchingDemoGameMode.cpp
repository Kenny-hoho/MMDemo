// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionMatchingDemoGameMode.h"
#include "MotionMatchingDemoCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMotionMatchingDemoGameMode::AMotionMatchingDemoGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
