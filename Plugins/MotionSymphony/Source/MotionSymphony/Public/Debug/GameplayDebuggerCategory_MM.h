// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#if WITH_GAMEPLAY_DEBUGGER_MENU

#include "GameplayDebuggerCategory.h"

class AActor;
class APlayerController;

class FGameplayDebuggerCategory_MM : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_MM();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	MOTIONSYMPHONY_API static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	struct FRepData
	{
		FString CompName;
		FString TreeDesc;
		float StickX;
		float StickY;
		float KeyX;
		float KeyY;

		void Serialize(FArchive& Ar);
	};
	FRepData DataPack;
};

//#endif // WITH_GAMEPLAY_DEBUGGER_MENU