// Copyright Khaos Systems. All Rights Reserved.

#include "KhaosPalette.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FKhaosPaletteSpec, "KhaosPalette.Palette",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	UKhaosPalette* Palette = nullptr;

	static FSoftObjectPath Rock() { return FSoftObjectPath(TEXT("/Game/Props/Rock.Rock")); }
	static FSoftObjectPath Tree() { return FSoftObjectPath(TEXT("/Game/Props/Tree.Tree")); }
	static FSoftObjectPath Bush() { return FSoftObjectPath(TEXT("/Game/Props/Bush.Bush")); }

	static FString Join(const TArray<FName>& InNames)
	{
		TArray<FString> Strings;
		for (const FName& Name : InNames)
		{
			Strings.Add(Name.ToString());
		}
		return FString::Join(Strings, TEXT(","));
	}

	/** The category list in order, as one string to compare. */
	FString Categories() const
	{
		TArray<FName> Names;
		for (const FKhaosPaletteCategory& Category : Palette->Categories)
		{
			Names.Add(Category.Name);
		}
		return Join(Names);
	}

	FString CategoryOf(const FSoftObjectPath& InAsset) const
	{
		const FKhaosPaletteEntry* Entry = Palette->FindEntry(InAsset);
		return Entry ? Entry->Category.ToString() : FString(TEXT("<missing>"));
	}

	bool IsDirty() const { return Palette->GetPackage()->IsDirty(); }
	void ClearDirty() { Palette->GetPackage()->SetDirtyFlag(false); }

END_DEFINE_SPEC(FKhaosPaletteSpec)

void FKhaosPaletteSpec::Define()
{
	BeforeEach([this]
	{
		// A package of its own, not the transient one, so marking it dirty is observable.
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/KhaosPaletteSpec/%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		Palette = NewObject<UKhaosPalette>(Package, TEXT("Palette"));
		ClearDirty();
	});

	AfterEach([this]
	{
		Palette = nullptr;
	});

	Describe("Entries", [this]
	{
		It("adds each asset once, skipping empty paths", [this]
		{
			TestEqual(TEXT("Added"), Palette->AddAssets({ Rock(), Rock(), FSoftObjectPath(), Tree() }, TEXT("Props")), 2);
			TestEqual(TEXT("Entries"), Palette->Entries.Num(), 2);
			TestEqual(TEXT("Category"), CategoryOf(Tree()), FString(TEXT("Props")));
			TestEqual(TEXT("Added again"), Palette->AddAssets({ Rock() }), 0);
		});

		It("moves entries to a category, counting only real changes", [this]
		{
			Palette->AddAssets({ Rock(), Tree() });
			TestEqual(TEXT("Changed"), Palette->SetCategory({ Rock(), Bush() }, TEXT("Big")), 1);
			TestEqual(TEXT("Category"), CategoryOf(Rock()), FString(TEXT("Big")));
			TestEqual(TEXT("Changed again"), Palette->SetCategory({ Rock() }, TEXT("Big")), 0);
		});

		It("labels a tile, and an empty label reverts to the asset name", [this]
		{
			Palette->AddAssets({ Rock() });
			TestTrue(TEXT("Labelled"), Palette->SetDisplayName(Rock(), FText::FromString(TEXT("Big rock"))));
			TestFalse(TEXT("Same label"), Palette->SetDisplayName(Rock(), FText::FromString(TEXT("Big rock"))));
			TestTrue(TEXT("Cleared"), Palette->SetDisplayName(Rock(), FText::GetEmpty()));
			TestTrue(TEXT("Label empty"), Palette->FindEntry(Rock())->DisplayName.IsEmpty());
		});

		It("removes an asset", [this]
		{
			Palette->AddAssets({ Rock(), Tree() });
			TestTrue(TEXT("Removed"), Palette->RemoveAsset(Rock()));
			TestFalse(TEXT("Removed again"), Palette->RemoveAsset(Rock()));
			TestFalse(TEXT("Contains"), Palette->ContainsAsset(Rock()));
			TestTrue(TEXT("Other kept"), Palette->ContainsAsset(Tree()));
		});
	});

	Describe("Categories", [this]
	{
		It("orders listed categories first, then the rest by name", [this]
		{
			Palette->AddAssets({ Rock() }, TEXT("Zed"));
			Palette->AddAssets({ Tree() }, TEXT("Alpha"));
			Palette->AddAssets({ Bush() }, TEXT("Mid"));
			Palette->AddCategory(TEXT("Mid"));
			TestEqual(TEXT("Order"), Join(Palette->GetCategoryOrder()), FString(TEXT("Mid,Alpha,Zed")));
		});

		It("adds a category once, and never a nameless one", [this]
		{
			TestTrue(TEXT("Added"), Palette->AddCategory(TEXT("Trees")));
			TestFalse(TEXT("Added again"), Palette->AddCategory(TEXT("Trees")));
			TestFalse(TEXT("Nameless"), Palette->AddCategory(NAME_None));
			TestEqual(TEXT("Categories"), Categories(), FString(TEXT("Trees")));
		});

		It("moves a category, clamping the index", [this]
		{
			Palette->AddCategory(TEXT("A"));
			Palette->AddCategory(TEXT("B"));
			Palette->AddCategory(TEXT("C"));
			TestTrue(TEXT("Moved to front"), Palette->MoveCategory(TEXT("C"), 0));
			TestEqual(TEXT("Order"), Categories(), FString(TEXT("C,A,B")));
			TestTrue(TEXT("Moved past end"), Palette->MoveCategory(TEXT("C"), 99));
			TestEqual(TEXT("Order"), Categories(), FString(TEXT("A,B,C")));
		});

		It("renames a category along with its entries", [this]
		{
			Palette->AddAssets({ Rock() }, TEXT("Stones"));
			Palette->AddCategory(TEXT("Stones"));
			TestTrue(TEXT("Renamed"), Palette->RenameCategory(TEXT("Stones"), TEXT("Rocks")));
			TestEqual(TEXT("Categories"), Categories(), FString(TEXT("Rocks")));
			TestEqual(TEXT("Entry"), CategoryOf(Rock()), FString(TEXT("Rocks")));
		});

		It("merges into a category that already exists when renaming", [this]
		{
			Palette->AddAssets({ Rock() }, TEXT("Stones"));
			Palette->AddAssets({ Tree() }, TEXT("Rocks"));
			Palette->AddCategory(TEXT("Stones"));
			Palette->AddCategory(TEXT("Rocks"));
			TestTrue(TEXT("Renamed"), Palette->RenameCategory(TEXT("Stones"), TEXT("Rocks")));
			TestEqual(TEXT("Categories"), Categories(), FString(TEXT("Rocks")));
			TestEqual(TEXT("Entry"), CategoryOf(Rock()), FString(TEXT("Rocks")));
		});

		It("leaves a removed category's entries uncategorised", [this]
		{
			Palette->AddAssets({ Rock() }, TEXT("Stones"));
			Palette->AddCategory(TEXT("Stones"));
			TestTrue(TEXT("Removed"), Palette->RemoveCategory(TEXT("Stones")));
			TestEqual(TEXT("Categories"), Categories(), FString());
			TestTrue(TEXT("Kept"), Palette->ContainsAsset(Rock()));
			TestEqual(TEXT("Entry"), CategoryOf(Rock()), FString(TEXT("None")));
		});

		It("registers a category referenced only by entries when its settings change", [this]
		{
			Palette->AddAssets({ Rock() }, TEXT("Stones"));
			TestTrue(TEXT("Collapsed"), Palette->SetCategoryCollapsedByDefault(TEXT("Stones"), true));
			TestEqual(TEXT("Categories"), Categories(), FString(TEXT("Stones")));
			TestTrue(TEXT("Stored"), Palette->FindCategory(TEXT("Stones"))->bCollapsedByDefault);
		});
	});

	Describe("Saving", [this]
	{
		It("marks the asset modified only when something changes", [this]
		{
			Palette->AddAssets({ Rock() }, TEXT("Stones"));
			TestTrue(TEXT("Dirty after adding"), IsDirty());

			ClearDirty();
			Palette->AddAssets({ Rock() });
			Palette->SetCategory({ Rock() }, TEXT("Stones"));
			Palette->RemoveAsset(Bush());
			Palette->AddCategory(NAME_None);
			Palette->RemoveCategory(TEXT("Nothing"));
			Palette->RenameCategory(TEXT("Nothing"), TEXT("Else"));
			TestFalse(TEXT("Dirty after changing nothing"), IsDirty());
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
