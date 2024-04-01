// Fill out your copyright notice in the Description page of Project Settings.


#include "DebugGameInstance.h"
#include "Misc/NetworkVersion.h" 
#include "Components/ModelComponent.h"

void UDebugGameInstance::Init()
{
	Super::Init();

	if (GIsEditor) {
		UE_LOG(LogTemp, Warning, TEXT("GIsEditor=True"));
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("GIsEditor=False"));
	}
	
}

void UDebugGameInstance::StartRecordingReplayFromBP(FString ReplayName, FString FriendlyName)
{
	TArray<FString> Options;
	Options.Add("ReplayStreamerOverride=InMemoryNetworkReplayStreaming");
	StartRecordingReplay(ReplayName, FriendlyName, Options);
}

void UDebugGameInstance::StopRecordingReplayFromBP()
{
	StopRecordingReplay();
}

void UDebugGameInstance::PlayReplayFromBP(FString ReplayName)
{
	TArray<FString> Options;
	Options.Add("ReplayStreamerOverride=InMemoryNetworkReplayStreaming");
	PlayReplay(ReplayName, nullptr, Options);
}