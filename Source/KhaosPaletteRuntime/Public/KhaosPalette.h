// Copyright Khaos Systems. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "KhaosPalette.generated.h"

class UTexture2D;

USTRUCT()
struct FKhaosPaletteEntry
{
	GENERATED_BODY()

	// Anything the level viewport can place from a drag: blueprint actors, static/skeletal meshes, Niagara systems, sounds, decal materials...
	UPROPERTY(EditAnywhere, Category = Palette)
	TSoftObjectPtr<UObject> Asset;

	// Shown under the tile instead of the asset name when set.
	UPROPERTY(EditAnywhere, Category = Palette)
	FText DisplayName;

	// Optional grouping; the palette tab can show one collapsible section per category.
	UPROPERTY(EditAnywhere, Category = Palette)
	FName Category;
};

USTRUCT()
struct FKhaosPaletteCategory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Palette)
	FName Name;

	// Shown in the section header when grouping by category.
	UPROPERTY(EditAnywhere, Category = Palette)
	TSoftObjectPtr<UTexture2D> Icon;

	// Section starts collapsed in the palette tab; toggling it there overrides this for the session.
	UPROPERTY(EditAnywhere, Category = Palette)
	bool bCollapsedByDefault = false;
};

// A themed set of placeable assets (e.g. one per biome). Lives in the runtime module only so cooked
// content referencing a palette does not break; the editor module owns all UI.
UCLASS()
class KHAOSPALETTERUNTIME_API UKhaosPalette : public UDataAsset
{
	GENERATED_BODY()

public:
	// Shown in the palette tab and menus instead of the asset name when set.
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category = Palette)
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = Palette, meta = (MultiLine = true))
	FText Description;

	// Sidebar icon in the palette tab. Leave empty to use the built-in palette glyph tinted with Color.
	UPROPERTY(EditAnywhere, Category = Palette)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, Category = Palette)
	FLinearColor Color = FLinearColor(1.f, 0.55f, 0.16f);

	UPROPERTY(EditAnywhere, Category = Palette, meta = (TitleProperty = "Asset"))
	TArray<FKhaosPaletteEntry> Entries;

	// Section order and icons. Categories only referenced by entries still show, after these, in name order.
	UPROPERTY(EditAnywhere, Category = Palette, meta = (TitleProperty = "Name"))
	TArray<FKhaosPaletteCategory> Categories;

	FText GetDisplayName() const;

	TArray<FName> GetCategoryOrder() const;
	const FKhaosPaletteCategory* FindCategory(FName Name) const;

	const FKhaosPaletteEntry* FindEntry(const FSoftObjectPath& AssetPath) const;
	FKhaosPaletteEntry* FindEntry(const FSoftObjectPath& AssetPath);
	bool ContainsAsset(const FSoftObjectPath& AssetPath) const { return FindEntry(AssetPath) != nullptr; }

	// All mutators call Modify() and mark the package dirty only when something actually changes.

	// Appends assets not already present under Category. Returns the number added.
	int32 AddAssets(TConstArrayView<FSoftObjectPath> AssetPaths, FName Category = NAME_None);

	// Moves entries already present to Category. Returns the number changed.
	int32 SetCategory(TConstArrayView<FSoftObjectPath> AssetPaths, FName Category);

	// Sets an entry's tile label; empty reverts to the asset name.
	bool SetDisplayName(const FSoftObjectPath& AssetPath, const FText& InDisplayName);

	bool RemoveAsset(const FSoftObjectPath& AssetPath);

	// Category list. Mutators that take a name register it first if it is only referenced by entries.
	bool AddCategory(FName Name);
	bool MoveCategory(FName Name, int32 NewIndex);
	bool RenameCategory(FName From, FName To);
	bool SetCategoryIcon(FName Name, const TSoftObjectPtr<UTexture2D>& InIcon);
	bool SetCategoryCollapsedByDefault(FName Name, bool bCollapsed);
	// Drops the category; its entries stay and become uncategorised.
	bool RemoveCategory(FName Name);
};
