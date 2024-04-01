// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/GameplayDebuggerCategory_MM.h"

//#if WITH_GAMEPLAY_DEBUGGER_MENU



FGameplayDebuggerCategory_MM::FGameplayDebuggerCategory_MM()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_MM::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_MM());
}

void FGameplayDebuggerCategory_MM::FRepData::Serialize(FArchive& Ar)
{
	Ar << CompName;
	Ar << TreeDesc;
	Ar << StickX;
	Ar << StickY;
	Ar << KeyX;
	Ar << KeyY;
}

void FGameplayDebuggerCategory_MM::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{

}

void FGameplayDebuggerCategory_MM::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	if (!DataPack.CompName.IsEmpty())
	{
		CanvasContext.Printf(TEXT("MM: {yellow}%s"), *DataPack.CompName);
		CanvasContext.Print(DataPack.TreeDesc);

	}
}

//#endif // WITH_GAMEPLAY_DEBUGGER_MENU
