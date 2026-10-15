// Copyright Khaos Systems. All Rights Reserved.

#include "KhaosPaletteEditorHelpers.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "IAssetTools.h"
#include "KhaosPalette.h"
#include "KhaosPaletteEditorModule.h"
#include "KhaosPaletteFactory.h"
#include "KhaosPaletteStyle.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "KhaosPaletteEditor"

namespace KhaosPaletteEditor
{
	TArray<FAssetData> FindAllPalettes()
	{
		TArray<FAssetData> Palettes;
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetRegistry.GetAssetsByClass(UKhaosPalette::StaticClass()->GetClassPathName(), Palettes, /*bSearchSubClasses*/ true);
		Palettes.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		return Palettes;
	}

	TArray<FSoftObjectPath> GatherPlaceableAssets(const TArray<AActor*>& Actors)
	{
		TArray<FSoftObjectPath> Paths;
		for (const AActor* Actor : Actors)
		{
			TArray<UObject*> Objects;
			if (Actor)
			{
				Actor->GetReferencedContentObjects(Objects);
			}
			for (const UObject* Object : Objects)
			{
				// AActor::GetReferencedContentObjects falls back to the generated class when no UBlueprint exists; a class is not droppable.
				if (Object && Object->IsAsset())
				{
					Paths.AddUnique(FSoftObjectPath(Object));
				}
			}
		}
		return Paths;
	}

	static void AddToPalette(const FAssetData PaletteAsset, const TArray<FSoftObjectPath> Assets)
	{
		UKhaosPalette* Palette = Cast<UKhaosPalette>(PaletteAsset.GetAsset());
		if (!Palette)
		{
			return;
		}
		Palette->AddAssets(Assets);
		FKhaosPaletteEditorModule::OpenPaletteTab(Palette);
	}

	void CreatePalette(TArray<FSoftObjectPath> InitialAssets)
	{
		UKhaosPaletteFactory* Factory = NewObject<UKhaosPaletteFactory>();
		Factory->InitialAssets = MoveTemp(InitialAssets);
		IAssetTools::Get().CreateAssetWithDialog(UKhaosPalette::StaticClass(), Factory);
	}

	void BuildAddToPaletteMenu(UToolMenu* Menu, TArray<FSoftObjectPath> Assets)
	{
		FToolMenuSection& Section = Menu->AddSection("KhaosPalettes", LOCTEXT("PalettesSection", "Palettes"));
		const FSlateIcon PaletteIcon(FKhaosPaletteStyle::Get().GetStyleSetName(), "ClassIcon.KhaosPalette");

		for (const FAssetData& PaletteAsset : FindAllPalettes())
		{
			FText Label;
			if (!PaletteAsset.GetTagValue(GET_MEMBER_NAME_CHECKED(UKhaosPalette, DisplayName), Label) || Label.IsEmpty())
			{
				Label = FText::FromName(PaletteAsset.AssetName);
			}
			Section.AddMenuEntry(
				PaletteAsset.PackageName,
				Label,
				FText::FromString(PaletteAsset.GetObjectPathString()),
				PaletteIcon,
				FUIAction(FExecuteAction::CreateStatic(&AddToPalette, PaletteAsset, Assets)));
		}

		Section.AddSeparator("NewPaletteSeparator");
		Section.AddMenuEntry(
			"NewPalette",
			LOCTEXT("NewPaletteFromSelection", "New Palette from Selection..."),
			LOCTEXT("NewPaletteFromSelectionTooltip", "Create a new palette asset containing the selected assets."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(FExecuteAction::CreateStatic(&CreatePalette, Assets)));
	}
}

#undef LOCTEXT_NAMESPACE
