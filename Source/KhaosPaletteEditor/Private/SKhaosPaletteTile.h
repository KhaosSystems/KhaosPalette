// Copyright 2026 Khaos Systems. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FKhaosPaletteTileItem;

DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnKhaosPaletteTileEvent, TSharedPtr<FKhaosPaletteTileItem>, const FPointerEvent&);

// One asset in the grid. Reports clicks, drags and context menus to the owning tab, which owns selection.
class SKhaosPaletteTile : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKhaosPaletteTile) : _TileSize(96.f) {}
		SLATE_ARGUMENT(TSharedPtr<FKhaosPaletteTileItem>, Item)
		SLATE_ATTRIBUTE(float, TileSize) // thumbnail width and height; the tab stretches it to fill a row
		SLATE_ATTRIBUTE(bool, IsSelected)
		SLATE_ATTRIBUTE(bool, IsPlacing) // the asset being placed in the level right now
		SLATE_EVENT(FOnKhaosPaletteTileEvent, OnClicked)
		SLATE_EVENT(FOnKhaosPaletteTileEvent, OnReleased) // left button up on the tile without a drag having started
		SLATE_EVENT(FOnKhaosPaletteTileEvent, OnDoubleClicked)
		SLATE_EVENT(FOnKhaosPaletteTileEvent, OnContextMenu)
		SLATE_EVENT(FOnKhaosPaletteTileEvent, OnDragRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	const FSlateBrush* GetBackground() const;

	TSharedPtr<FKhaosPaletteTileItem> Item;
	TAttribute<bool> IsSelected;
	TAttribute<bool> IsPlacing;
	FOnKhaosPaletteTileEvent OnClickedDelegate;
	FOnKhaosPaletteTileEvent OnReleasedDelegate;
	FOnKhaosPaletteTileEvent OnDoubleClickedDelegate;
	FOnKhaosPaletteTileEvent OnContextMenuDelegate;
	FOnKhaosPaletteTileEvent OnDragRequestedDelegate;
};
