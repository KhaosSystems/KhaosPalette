// Copyright 2026 Khaos Systems. All Rights Reserved.

#include "KhaosPaletteEditorHelpers.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "IAssetTools.h"
#include "KhaosPalette.h"
#include "KhaosPaletteEditorModule.h"
#include "KhaosPaletteFactory.h"
#include "KhaosPaletteStyle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"

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

	// With no category, adds the new assets uncategorised and leaves ones already in the palette alone. With one
	// (NAME_None for Uncategorised), new assets go into it and ones already there move to it, so what was picked
	// is where they all end up.
	static void AddToPalette(const FAssetData PaletteAsset, const TArray<FSoftObjectPath> Assets, const TOptional<FName> Category)
	{
		UKhaosPalette* Palette = Cast<UKhaosPalette>(PaletteAsset.GetAsset());
		if (!Palette)
		{
			return;
		}
		const FScopedTransaction Transaction(LOCTEXT("AddToPaletteTransaction", "Add to Palette"));
		if (Category.IsSet())
		{
			Palette->AddCategory(Category.GetValue());
			Palette->AddAssets(Assets, Category.GetValue());
			Palette->SetCategory(Assets, Category.GetValue());
		}
		else
		{
			Palette->AddAssets(Assets);
		}
		FKhaosPaletteEditorModule::OpenPaletteTab(Palette);
	}

	// The categories of one palette, loaded only when its submenu opens.
	static void BuildPaletteCategoryMenu(UToolMenu* Menu, const FAssetData PaletteAsset, const TArray<FSoftObjectPath> Assets)
	{
		const UKhaosPalette* Palette = Cast<UKhaosPalette>(PaletteAsset.GetAsset());
		if (!Palette)
		{
			return;
		}
		FToolMenuSection& Section = Menu->AddSection("KhaosPaletteCategories", LOCTEXT("CategoriesSection", "Category"));
		for (const FName& Category : Palette->GetCategoryOrder())
		{
			Section.AddMenuEntry(
				Category,
				FText::FromName(Category),
				FText::Format(LOCTEXT("AddToCategoryTooltip", "Add to {0}. Assets already in the palette move there."), FText::FromName(Category)),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&AddToPalette, PaletteAsset, Assets, TOptional<FName>(Category))));
		}
		Section.AddMenuEntry(
			"Uncategorised",
			LOCTEXT("Uncategorised", "Uncategorised"),
			LOCTEXT("AddUncategorisedTooltip", "Add without a category. Assets already in the palette move out of theirs."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&AddToPalette, PaletteAsset, Assets, TOptional<FName>(NAME_None))));

		// Typed in place: Enter creates the category and adds the assets to it.
		Section.AddSeparator("NewCategorySeparator");
		Section.AddEntry(FToolMenuEntry::InitWidget(
			"NewCategory",
			SNew(SBox)
			.WidthOverride(180.f)
			.Padding(FMargin(2.f, 2.f))
			[
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("NewCategoryHint", "New category..."))
				.ToolTipText(LOCTEXT("NewCategoryTooltip", "Type a name and press Enter to add the assets to a new category."))
				.OnTextCommitted_Lambda([PaletteAsset, Assets](const FText& Text, ETextCommit::Type CommitType)
				{
					const FString Name = Text.ToString().TrimStartAndEnd();
					if (CommitType != ETextCommit::OnEnter || Name.IsEmpty())
					{
						return;
					}
					AddToPalette(PaletteAsset, Assets, TOptional<FName>(FName(*Name)));
					FSlateApplication::Get().DismissAllMenus();
				})
			],
			FText::GetEmpty(),
			/*bNoIndent*/ true));
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
			// Click adds without a category; the submenu picks one.
			Section.AddSubMenu(
				PaletteAsset.PackageName,
				Label,
				FText::Format(LOCTEXT("AddToPaletteEntryTooltip", "{0}\nClick to add here, or pick a category."), FText::FromString(PaletteAsset.GetObjectPathString())),
				FNewToolMenuDelegate::CreateStatic(&BuildPaletteCategoryMenu, PaletteAsset, Assets),
				FToolUIActionChoice(FUIAction(FExecuteAction::CreateStatic(&AddToPalette, PaletteAsset, Assets, TOptional<FName>()))),
				EUserInterfaceActionType::Button,
				/*bInOpenSubMenuOnClick*/ false,
				PaletteIcon);
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
