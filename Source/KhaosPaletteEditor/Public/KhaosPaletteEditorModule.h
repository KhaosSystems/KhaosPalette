// Copyright Khaos Games ApS. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;
class SKhaosPaletteTab;
class UKhaosPalette;
class FSpawnTabArgs;

class FKhaosPaletteEditorModule : public IModuleInterface
{
public:
	static const FName TabName;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Opens the palette tab (creating it if needed) and shows the given palette. Null keeps the current one.
	static void OpenPaletteTab(UKhaosPalette* Palette = nullptr);

private:
	void RegisterMenus();
	TSharedRef<SDockTab> SpawnPaletteTab(const FSpawnTabArgs& Args);

	TWeakPtr<SKhaosPaletteTab> PaletteTab;
};
