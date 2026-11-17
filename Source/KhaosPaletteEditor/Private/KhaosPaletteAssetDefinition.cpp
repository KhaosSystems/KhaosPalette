// Copyright 2026 Khaos Systems. All Rights Reserved.

#include "KhaosPaletteAssetDefinition.h"

#include "KhaosPalette.h"
#include "KhaosPaletteEditorModule.h"

#define LOCTEXT_NAMESPACE "KhaosPaletteEditor"

FText UAssetDefinition_KhaosPalette::GetAssetDisplayName() const
{
	return LOCTEXT("AssetDisplayName", "Khaos Palette");
}

FLinearColor UAssetDefinition_KhaosPalette::GetAssetColor() const
{
	return FLinearColor(FColor(255, 140, 40));
}

TSoftClassPtr<UObject> UAssetDefinition_KhaosPalette::GetAssetClass() const
{
	return UKhaosPalette::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_KhaosPalette::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(LOCTEXT("KhaosCategory", "Khaos")) };
	return Categories;
}

EAssetCommandResult UAssetDefinition_KhaosPalette::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	// The palette tab shows one palette, so open the first one selected.
	const TArray<UKhaosPalette*> Palettes = OpenArgs.LoadObjects<UKhaosPalette>();
	if (Palettes.IsEmpty())
	{
		return EAssetCommandResult::Unhandled;
	}
	FKhaosPaletteEditorModule::OpenPaletteTab(Palettes[0]);
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
