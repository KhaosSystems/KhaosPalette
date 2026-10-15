// Copyright Khaos Games ApS. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "KhaosPaletteAssetDefinition.generated.h"

UCLASS()
class UAssetDefinition_KhaosPalette : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	// Opening a palette shows it in the palette tab; the property editor is reachable from the tab.
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
