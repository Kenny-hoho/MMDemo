// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "AnimMod_RotationMatching.generated.h"

class UAnimSequence;

/**
 * 
 */
UCLASS()
class MOTIONSYMPHONYEDITOR_API UAnimMod_RotationMatching : public UAnimationModifier
{
	GENERATED_BODY()
	
	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;
};