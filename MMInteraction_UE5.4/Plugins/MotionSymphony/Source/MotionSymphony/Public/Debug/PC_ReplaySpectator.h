// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PC_ReplaySpectator.generated.h"

/**
 * 
 */
UCLASS()
class MOTIONSYMPHONY_API APC_ReplaySpectator : public APlayerController
{
	GENERATED_BODY()
public:
	APC_ReplaySpectator(const FObjectInitializer& ObjectInitializer);

protected:
	int32 PreviousAASetting;
	int32 PreviousMBSetting;

	UFUNCTION(BlueprintCallable, Category = "CurrentReplay")
	bool SetCurrentReplayPausedState(bool bDoPause);

	UFUNCTION(BlueprintCallable, Category = "CurrentReplay")
	float GetCurrentReplayTotalTimeInSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "CurrentReplay")
	float GetCurrentReplayCurrentTimeInSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "CurrentReplay")
	void SetCurrentReplayToTimeInSeconds(int32 Seconds);

	UFUNCTION(BlueprintCallable, Category = "CurrentReplay")
	void SetCurrentReplayPlayRate(float PlayRate = 1.f);
};
