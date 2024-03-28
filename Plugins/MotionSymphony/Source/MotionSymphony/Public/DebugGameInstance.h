// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "NetworkReplayStreaming.h"
#include "DebugGameInstance.generated.h"

UCLASS()
class MOTIONSYMPHONY_API UDebugGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
    /** Start recording a replay from blueprint. ReplayName = Name of file on disk, FriendlyName = Name of replay in UI */
	UFUNCTION(BlueprintCallable, Category = "Replays")
	void StartRecordingReplayFromBP(FString ReplayName, FString FriendlyName);

    /** Start recording a running replay and save it, from blueprint. */
    UFUNCTION(BlueprintCallable, Category = "Replays")
    void StopRecordingReplayFromBP();

    /** Start playback for a previously recorded Replay, from blueprint */
    UFUNCTION(BlueprintCallable, Category = "Replays")
    void PlayReplayFromBP(FString ReplayName);

    virtual void Init() override;

    // virtual void StartGameInstance() override;


private:
    TArray<FLevelCollection>    LevelCollections;
};
