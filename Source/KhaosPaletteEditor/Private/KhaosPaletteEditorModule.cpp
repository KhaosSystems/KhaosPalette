// Copyright Khaos Games ApS. All Rights Reserved.

#include "KhaosPaletteEditorModule.h"

#include "ContentBrowserMenuContexts.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "KhaosPalette.h"
#include "KhaosPaletteEditorHelpers.h"
#include "KhaosPaletteStyle.h"
#include "SKhaosPaletteTab.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "KhaosPaletteEditor"

const FName FKhaosPaletteEditorModule::TabName(TEXT("KhaosPalette"));

void FKhaosPaletteEditorModule::StartupModule()
{
	FKhaosPaletteStyle::Get();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName, FOnSpawnTab::CreateRaw(this, &FKhaosPaletteEditorModule::SpawnPaletteTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Khaos Palette"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Drag placeable assets from an artist-authored palette into the level."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FKhaosPaletteStyle::Get().GetStyleSetName(), "KhaosPalette.TabIcon"));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FKhaosPaletteEditorModule::RegisterMenus));
}

void FKhaosPaletteEditorModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
	}
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}
	FKhaosPaletteStyle::Shutdown();
}

void FKhaosPaletteEditorModule::OpenPaletteTab(UKhaosPalette* Palette)
{
	const TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->TryInvokeTab(TabName);
	if (!Tab.IsValid() || !Palette)
	{
		return;
	}
	FKhaosPaletteEditorModule& Module = FModuleManager::GetModuleChecked<FKhaosPaletteEditorModule>("KhaosPaletteEditor");
	if (const TSharedPtr<SKhaosPaletteTab> TabWidget = Module.PaletteTab.Pin())
	{
		TabWidget->SetPalette(Palette);
	}
}

void FKhaosPaletteEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	const FSlateIcon SubMenuIcon(FKhaosPaletteStyle::Get().GetStyleSetName(), "ClassIcon.KhaosPalette");

	// Level editor toolbar: the User section is the slot the engine leaves for plugins.
	if (UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User"))
	{
		Toolbar->FindOrAddSection("Khaos").AddEntry(FToolMenuEntry::InitToolBarButton(
			"KhaosPalette",
			FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(TabName); })),
			LOCTEXT("ToolbarLabel", "Palette"),
			LOCTEXT("ToolbarTooltip", "Drag placeable assets from an artist-authored palette into the level."),
			FSlateIcon(FKhaosPaletteStyle::Get().GetStyleSetName(), "KhaosPalette.Sidebar.Palette")));
	}

	// Content Browser: right-click assets -> Add to Palette.
	UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
	AssetMenu->FindOrAddSection("GetAssetActions").AddDynamicEntry("KhaosPalette", FNewToolMenuSectionDelegate::CreateLambda([SubMenuIcon](FToolMenuSection& InSection)
	{
		const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
		if (!Context)
		{
			return;
		}
		TArray<FSoftObjectPath> Assets;
		for (const FAssetData& AssetData : Context->SelectedAssets)
		{
			if (!AssetData.IsInstanceOf(UKhaosPalette::StaticClass()))
			{
				Assets.Add(AssetData.GetSoftObjectPath());
			}
		}
		if (Assets.IsEmpty())
		{
			return;
		}
		InSection.AddSubMenu(
			"KhaosPaletteAddTo",
			LOCTEXT("AddToPalette", "Add to Palette"),
			LOCTEXT("AddToPaletteTooltip", "Add the selected assets to a Khaos Palette."),
			FNewToolMenuDelegate::CreateStatic(&KhaosPaletteEditor::BuildAddToPaletteMenu, Assets),
			/*bInOpenSubMenuOnClick*/ false,
			SubMenuIcon);
	}));

	// Level editor: right-click actors -> Add to Palette (resolves actors to their source blueprint/mesh).
	// Sits above the engine's Preview section (Play From Here) rather than at the very end of the menu.
	const FToolMenuInsert AbovePreview("ActorPreview", EToolMenuInsertType::Before);
	UToolMenu* ActorMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
	ActorMenu->AddDynamicSection("KhaosPalette", FNewToolMenuDelegate::CreateLambda([SubMenuIcon, AbovePreview](UToolMenu* InMenu)
	{
		TArray<AActor*> SelectedActors;
		if (GEditor)
		{
			GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);
		}
		const TArray<FSoftObjectPath> Assets = KhaosPaletteEditor::GatherPlaceableAssets(SelectedActors);
		if (Assets.IsEmpty())
		{
			return;
		}
		FToolMenuSection& Section = InMenu->AddSection("KhaosPalette", LOCTEXT("PaletteSection", "Khaos Palette"), AbovePreview);
		Section.AddSubMenu(
			"KhaosPaletteAddTo",
			LOCTEXT("AddActorsToPalette", "Add to Palette"),
			LOCTEXT("AddActorsToPaletteTooltip", "Add the selected actors' source assets (blueprints, meshes) to a Khaos Palette."),
			FNewToolMenuDelegate::CreateStatic(&KhaosPaletteEditor::BuildAddToPaletteMenu, Assets),
			/*bInOpenSubMenuOnClick*/ false,
			SubMenuIcon);
	}), AbovePreview);
}

TSharedRef<SDockTab> FKhaosPaletteEditorModule::SpawnPaletteTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SKhaosPaletteTab> Widget = SNew(SKhaosPaletteTab);
	PaletteTab = Widget;
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			Widget
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FKhaosPaletteEditorModule, KhaosPaletteEditor)
