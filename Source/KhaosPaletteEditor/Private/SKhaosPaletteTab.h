// Copyright Khaos Systems. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Framework/SlateDelegates.h"
#include "Styling/SlateColor.h"
#include "Widgets/SCompoundWidget.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
class FActiveTimerHandle;
class FMenuBuilder;
class SBox;
class SWindow;
class UKhaosPalette;
struct FPropertyChangedEvent;
class FTransactionObjectEvent;

struct FKhaosPaletteTileItem
{
	FAssetData AssetData;
	FText Label;
	FName Category;
	TSharedPtr<FAssetThumbnail> Thumbnail;
};

// Place Actors-style panel: a vertical sidebar with one button per palette asset (plus "+" to create one)
// and a grid of the selected palette's assets, optionally grouped by category. Tiles start a standard
// FAssetDragDropOp, so the level viewport places them exactly as a Content Browser drag would; with click
// to place, a plain click starts that drag on release so it follows the cursor until the next click drops it.
// Accepts asset drops to grow the palette.
class SKhaosPaletteTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKhaosPaletteTab) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SKhaosPaletteTab() override;

	void SetPalette(UKhaosPalette* InPalette);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	using FItemPtr = TSharedPtr<FKhaosPaletteTileItem>;

	// Sidebar
	TSharedRef<SWidget> MakeSidebar();
	TSharedRef<SWidget> MakePaletteToolbar();
	void RequestSidebarRefresh();
	void RebuildSidebar();
	void OnAssetsAddedOrRemoved(TConstArrayView<FAssetData> Assets);
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldPath);
	void CreateNewPalette();

	// Header
	TSharedRef<SWidget> MakeHeader();
	TSharedRef<SWidget> MakeIconButton(const FName BrushName, const FText& ToolTip, FOnClicked OnClicked);
	FText GetPaletteName() const;
	FText GetPaletteDescription() const;
	const FSlateBrush* GetPaletteIcon() const;
	void OnSearchTextChanged(const FText& InText);
	void SetGroupByCategory(bool bEnabled);
	TSharedRef<SWidget> MakeSettingsMenu();

	// Fade when not hovered: applies to the sidebar drawer the tab is pinned open in (render opacity, the viewport
	// shows through) or to the floating window it lives in (window alpha). Never the main editor window.
	void SetFadeWhenNotHovered(bool bEnabled);
	EActiveTimerReturnType UpdateFade(double InCurrentTime, float InDeltaTime);
	void RestoreFaded();
	void SetDrawerBrushAlpha(float Alpha);
	void RestoreDrawerBrushes();

	// Grid
	void RebuildItems();
	void RefreshFilteredItems();
	void RebuildGrid();
	TSharedRef<SWidget> MakeCategorySection(FName Category, const TArray<FItemPtr>& Items);
	FString SectionKey(FName Category) const;
	TSharedRef<SWidget> MakeTileWrap(const TArray<FItemPtr>& Items);
	TSharedRef<SWidget> MakeTile(FItemPtr Item);
	TSharedRef<SWidget> MakeTileContextMenu();
	FReply OnTileClicked(FItemPtr Item, const FPointerEvent& MouseEvent);
	FReply OnTileReleased(FItemPtr Item, const FPointerEvent& MouseEvent);
	FReply OnTileDoubleClicked(FItemPtr Item, const FPointerEvent& MouseEvent);
	FReply OnTileContextMenu(FItemPtr Item, const FPointerEvent& MouseEvent);
	FReply OnTileDragRequested(FItemPtr Item, const FPointerEvent& MouseEvent);
	EVisibility GetEmptyHintVisibility() const;
	FText GetEmptyHintText() const;

	// Whole grid: adds new assets. Category sections: also re-categorise entries (including the tab's own tiles).
	bool AreAssetsAcceptableForDrop(TArrayView<FAssetData> Assets) const;
	void OnAssetsDropped(const FDragDropEvent& DragDropEvent, TArrayView<FAssetData> Assets);
	bool AreAssetsAcceptableForCategoryDrop(TArrayView<FAssetData> Assets, FName Category) const;
	void OnAssetsDroppedOnCategory(const FDragDropEvent& DragDropEvent, TArrayView<FAssetData> Assets, FName Category);

	// Categories and renaming, all without opening the details view. Names are typed into a small popup.
	void PromptForName(const FText& Label, const FText& Default, TFunction<void(const FString&)> OnCommitted);
	void FillCategoryMenu(FMenuBuilder& MenuBuilder);
	void SetSelectedCategory(FName Category);
	void RenameSelected();
	void AddCategory();
	FReply OnCategorySectionMouseUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FName Category);
	void RenameCategory(FName Category);
	void RemoveCategory(FName Category);
	void SetCategoryIcon(const FAssetData& Asset, FName Category);
	void ToggleCategoryOpenByDefault(FName Category);
	bool IsCategoryOpenByDefault(FName Category) const;
	void OnCategoryDropped(FName Dragged, FName Target, bool bAfter);

	void RemoveSelected();
	void BrowseToSelected();
	void OpenSelected();
	void EditPaletteProperties();
	void BrowseToPalette();
	bool IsPaletteDirty() const;
	void SavePalette();
	void OnPackageDirtyStateChanged(UPackage* Package);

	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);
	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

	TWeakObjectPtr<UKhaosPalette> Palette;
	FName PaletteIconName;
	TArray<FItemPtr> AllItems;
	TArray<FItemPtr> FilteredItems;
	TSet<FItemPtr> SelectedItems;
	TArray<FName> Categories; // display order, from the palette
	// "<palette path>|<category>" -> collapsed, for sections the user toggled this session; grid rebuilds keep
	// that state. Sections without an entry follow the category's bCollapsedByDefault.
	TMap<FString, bool> SectionCollapsed;
	FText SearchText;
	float TileSize = 96.f;
	bool bGroupByCategory = true;
	bool bClickToPlace = true;
	bool bFadeWhenNotHovered = false;
	float FadedOpacity = 0.4f;
	bool bSidebarRefreshPending = false;
	TSharedPtr<FActiveTimerHandle> FadeTimer;
	// What we last faded, restored when fading stops or the tab moves elsewhere.
	TWeakPtr<SWidget> FadedDrawer;
	TWeakPtr<SWindow> FadedWindow;
	TMap<FName, FSlateColor> DrawerBrushTints; // original tints of the drawer style brushes while their alpha is lowered

	TSharedPtr<SBox> SidebarBox;
	TSharedPtr<SBox> GridBox;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	FDelegateHandle PropertyChangedHandle;
	FDelegateHandle TransactedHandle;
	FDelegateHandle DirtyStateHandle;
	FDelegateHandle AssetsAddedHandle;
	FDelegateHandle AssetsRemovedHandle;
	FDelegateHandle AssetRenamedHandle;
	FDelegateHandle FilesLoadedHandle;
};
