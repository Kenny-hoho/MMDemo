// Copyright 2023 Attaku under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"

class FSlateStyleSet;

class IContentBrowserSelectionMenuExtender
{
public:
	virtual void Extend() = 0;
};

class FRMFixToolEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FName GetStyleSetName();
	static FName GetContextMenuIconName();
	static FName GetRemoveRootMotionIconName();
	static FName GetTransferAnimationIconName();
	static FName GetSnapIconName();
	static FName GetFixRootMotionDirectionIconName();

	const ISlateStyle& GetStyleSet() const { return *StyleSet; }

protected:
	void StartupStyle();
	void ShutdownStyle();

protected:
	TSharedPtr<FSlateStyleSet> StyleSet;
	TArray<TSharedPtr<IContentBrowserSelectionMenuExtender>> ContentBrowserSelectionMenuExtenders;
};