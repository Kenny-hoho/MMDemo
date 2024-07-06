// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/PC_ReplaySpectator.h"
#include "Engine/DemoNetDriver.h"

APC_ReplaySpectator::APC_ReplaySpectator(const FObjectInitializer& ObjectInitializer) {
    bShowMouseCursor = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    bShouldPerformFullTickWhenPaused = true;
}


bool APC_ReplaySpectator::SetCurrentReplayPausedState(bool bDoPause)
{
    AWorldSettings* WorldSettings = GetWorldSettings();
    // Set MotionBlur off and Anti Aliasing to FXAA in order to bypass the pause-bug of both
    // static const auto CVarAA = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AntiAliasing"));

    static const auto CVarMB = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.MotionBlur"));

    if (bDoPause)
    {
        // PreviousAASetting = CVarAA->GetInt();
        PreviousMBSetting = CVarMB->GetInt();

        // Set MotionBlur to OFF, Anti-Aliasing to FXAA
        // CVarAA->Set(1);
        CVarMB->Set(0);

        APlayerController* PlayerController = Cast<APlayerController>(GetWorld()->GetDemoNetDriver()->ServerConnection->OwningActor);
        if (PlayerController != nullptr)
        {
            WorldSettings->SetPauserPlayerState(PlayerController->PlayerState);
        }
        return true;
    }
    // Rest MotionBlur and AA
    // CVarAA->Set(PreviousAASetting);
    CVarMB->Set(PreviousMBSetting);

    WorldSettings->SetPauserPlayerState(nullptr);
    return false;
}

float APC_ReplaySpectator::GetCurrentReplayTotalTimeInSeconds() const
{
    if (GetWorld()) {
        if (GetWorld()->GetDemoNetDriver()) {
            return GetWorld()->GetDemoNetDriver()->GetDemoTotalTime();
        }
    }
    return float();
}

float APC_ReplaySpectator::GetCurrentReplayCurrentTimeInSeconds() const
{
    if (GetWorld()) {
        if (GetWorld()->GetDemoNetDriver()) {
            return GetWorld()->GetDemoNetDriver()->GetDemoCurrentTime();
        }
    }
    return float();
}

void APC_ReplaySpectator::SetCurrentReplayToTimeInSeconds(int32 Seconds)
{
    if (GetWorld()) {
        if (GetWorld()->GetDemoNetDriver()) {
            GetWorld()->GetDemoNetDriver()->GotoTimeInSeconds(Seconds);
        }
    }
}

void APC_ReplaySpectator::SetCurrentReplayPlayRate(float PlayRate)
{
    if (GetWorld()) {
        if (GetWorld()->GetDemoNetDriver()) {
            GetWorld()->GetWorldSettings()->DemoPlayTimeDilation = PlayRate;
        }
    }
}
