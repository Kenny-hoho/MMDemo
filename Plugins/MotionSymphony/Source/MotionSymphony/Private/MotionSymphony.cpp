// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "MotionSymphony.h"
#include "GameplayDebugger.h"
#include "Debug/GameplayDebuggerCategory_MM.h"

#define LOCTEXT_NAMESPACE "FMotionSymphonyModule"

void FMotionSymphonyModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("MotionMatching", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_MM::MakeInstance), EGameplayDebuggerCategoryState::EnabledInGameAndSimulate, 5);
	GameplayDebuggerModule.NotifyCategoriesChanged();
}

void FMotionSymphonyModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("MotionMatching");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}

}
	
IMPLEMENT_MODULE(FMotionSymphonyModule, MotionSymphony)

#undef LOCTEXT_NAMESPACE