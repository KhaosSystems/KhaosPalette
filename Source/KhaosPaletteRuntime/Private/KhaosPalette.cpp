// Copyright Khaos Games ApS. All Rights Reserved.

#include "KhaosPalette.h"

FText UKhaosPalette::GetDisplayName() const
{
	return DisplayName.IsEmpty() ? FText::FromString(GetName()) : DisplayName;
}

const FKhaosPaletteEntry* UKhaosPalette::FindEntry(const FSoftObjectPath& AssetPath) const
{
	return Entries.FindByPredicate([&AssetPath](const FKhaosPaletteEntry& Entry)
	{
		return Entry.Asset.ToSoftObjectPath() == AssetPath;
	});
}

int32 UKhaosPalette::AddAssets(TConstArrayView<FSoftObjectPath> AssetPaths, FName Category)
{
	int32 NumAdded = 0;
	for (const FSoftObjectPath& AssetPath : AssetPaths)
	{
		if (AssetPath.IsNull() || ContainsAsset(AssetPath))
		{
			continue;
		}
		if (NumAdded == 0)
		{
			Modify();
		}
		FKhaosPaletteEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Asset = AssetPath;
		Entry.Category = Category;
		++NumAdded;
	}
	if (NumAdded > 0)
	{
		MarkPackageDirty();
	}
	return NumAdded;
}

FKhaosPaletteEntry* UKhaosPalette::FindEntry(const FSoftObjectPath& AssetPath)
{
	return const_cast<FKhaosPaletteEntry*>(static_cast<const UKhaosPalette*>(this)->FindEntry(AssetPath));
}

int32 UKhaosPalette::SetCategory(TConstArrayView<FSoftObjectPath> AssetPaths, FName Category)
{
	int32 NumChanged = 0;
	for (const FSoftObjectPath& AssetPath : AssetPaths)
	{
		FKhaosPaletteEntry* Entry = FindEntry(AssetPath);
		if (!Entry || Entry->Category == Category)
		{
			continue;
		}
		if (NumChanged == 0)
		{
			Modify();
		}
		Entry->Category = Category;
		++NumChanged;
	}
	if (NumChanged > 0)
	{
		MarkPackageDirty();
	}
	return NumChanged;
}

bool UKhaosPalette::SetDisplayName(const FSoftObjectPath& AssetPath, const FText& InDisplayName)
{
	FKhaosPaletteEntry* Entry = FindEntry(AssetPath);
	if (!Entry || Entry->DisplayName.EqualTo(InDisplayName))
	{
		return false;
	}
	Modify();
	Entry->DisplayName = InDisplayName;
	MarkPackageDirty();
	return true;
}

bool UKhaosPalette::RemoveAsset(const FSoftObjectPath& AssetPath)
{
	const int32 Index = Entries.IndexOfByPredicate([&AssetPath](const FKhaosPaletteEntry& Entry)
	{
		return Entry.Asset.ToSoftObjectPath() == AssetPath;
	});
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Modify();
	Entries.RemoveAt(Index);
	MarkPackageDirty();
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Categories

TArray<FName> UKhaosPalette::GetCategoryOrder() const
{
	TArray<FName> Order;
	for (const FKhaosPaletteCategory& Category : Categories)
	{
		if (!Category.Name.IsNone())
		{
			Order.AddUnique(Category.Name);
		}
	}
	TArray<FName> Unlisted;
	for (const FKhaosPaletteEntry& Entry : Entries)
	{
		if (!Entry.Category.IsNone() && !Order.Contains(Entry.Category))
		{
			Unlisted.AddUnique(Entry.Category);
		}
	}
	Unlisted.Sort(FNameLexicalLess());
	Order.Append(Unlisted);
	return Order;
}

const FKhaosPaletteCategory* UKhaosPalette::FindCategory(FName Name) const
{
	return Categories.FindByPredicate([Name](const FKhaosPaletteCategory& Category) { return Category.Name == Name; });
}

bool UKhaosPalette::AddCategory(FName Name)
{
	if (Name.IsNone() || FindCategory(Name))
	{
		return false;
	}
	Modify();
	Categories.AddDefaulted_GetRef().Name = Name;
	MarkPackageDirty();
	return true;
}

bool UKhaosPalette::MoveCategory(FName Name, int32 NewIndex)
{
	AddCategory(Name);
	const int32 OldIndex = Categories.IndexOfByPredicate([Name](const FKhaosPaletteCategory& Category) { return Category.Name == Name; });
	NewIndex = FMath::Clamp(NewIndex, 0, Categories.Num() - 1);
	if (OldIndex == INDEX_NONE || OldIndex == NewIndex)
	{
		return false;
	}
	Modify();
	FKhaosPaletteCategory Moved = Categories[OldIndex];
	Categories.RemoveAt(OldIndex);
	Categories.Insert(MoveTemp(Moved), NewIndex);
	MarkPackageDirty();
	return true;
}

bool UKhaosPalette::RenameCategory(FName From, FName To)
{
	if (From.IsNone() || To.IsNone() || From == To)
	{
		return false;
	}
	Modify();
	// Merge into an existing target rather than listing the same name twice.
	const int32 FromIndex = Categories.IndexOfByPredicate([From](const FKhaosPaletteCategory& Category) { return Category.Name == From; });
	if (FromIndex != INDEX_NONE)
	{
		if (FindCategory(To))
		{
			Categories.RemoveAt(FromIndex);
		}
		else
		{
			Categories[FromIndex].Name = To;
		}
	}
	for (FKhaosPaletteEntry& Entry : Entries)
	{
		if (Entry.Category == From)
		{
			Entry.Category = To;
		}
	}
	MarkPackageDirty();
	return true;
}

bool UKhaosPalette::SetCategoryIcon(FName Name, const TSoftObjectPtr<UTexture2D>& InIcon)
{
	AddCategory(Name);
	FKhaosPaletteCategory* Category = const_cast<FKhaosPaletteCategory*>(FindCategory(Name));
	if (!Category || Category->Icon == InIcon)
	{
		return false;
	}
	Modify();
	Category->Icon = InIcon;
	MarkPackageDirty();
	return true;
}

bool UKhaosPalette::SetCategoryCollapsedByDefault(FName Name, bool bCollapsed)
{
	AddCategory(Name);
	FKhaosPaletteCategory* Category = const_cast<FKhaosPaletteCategory*>(FindCategory(Name));
	if (!Category || Category->bCollapsedByDefault == bCollapsed)
	{
		return false;
	}
	Modify();
	Category->bCollapsedByDefault = bCollapsed;
	MarkPackageDirty();
	return true;
}

bool UKhaosPalette::RemoveCategory(FName Name)
{
	if (Name.IsNone())
	{
		return false;
	}
	Modify();
	Categories.RemoveAll([Name](const FKhaosPaletteCategory& Category) { return Category.Name == Name; });
	for (FKhaosPaletteEntry& Entry : Entries)
	{
		if (Entry.Category == Name)
		{
			Entry.Category = NAME_None;
		}
	}
	MarkPackageDirty();
	return true;
}
