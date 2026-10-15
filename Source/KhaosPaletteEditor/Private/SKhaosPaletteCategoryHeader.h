// Copyright Khaos Games ApS. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class SExpandableArea;

class FKhaosPaletteCategoryDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FKhaosPaletteCategoryDragDropOp, FDecoratedDragDropOp)

	FName Category;

	static TSharedRef<FKhaosPaletteCategoryDragDropOp> New(FName Category, const FText& Label, const FSlateBrush* Icon);
};

// Dragged category, target category (NAME_None = the Uncategorised section, i.e. the end), drop below the target.
DECLARE_DELEGATE_ThreeParams(FOnKhaosPaletteCategoryDropped, FName, FName, bool);

// Header row of a category section: drag grip, optional icon, "Name (N)", expander arrow. Clicking toggles the
// owning section; dragging the row starts a reorder, which the enclosing SKhaosPaletteCategorySection receives.
class SKhaosPaletteCategoryHeader : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKhaosPaletteCategoryHeader) : _Count(0), _Icon(nullptr), _bReorderable(true) {}
		SLATE_ARGUMENT(FName, Category)
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(int32, Count)
		SLATE_ARGUMENT(UTexture2D*, Icon)
		SLATE_ARGUMENT(bool, bReorderable)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// The section this header toggles; set after construction because the area is built around the header.
	void SetOwnerArea(TSharedPtr<SExpandableArea> InArea) { OwnerArea = InArea; }

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	const FSlateBrush* GetExpanderImage() const;

	FName Category;
	FText Title;
	bool bReorderable = true;
	TWeakPtr<SExpandableArea> OwnerArea;

	TStrongObjectPtr<UTexture2D> IconTexture;
	FSlateBrush IconBrush;
};

// Wraps one whole category section. Takes dragged headers, dropping them above or below this section, and draws
// the insertion line in the gap between sections rather than inside the header.
class SKhaosPaletteCategorySection : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SKhaosPaletteCategorySection) : _GapBelow(4.f) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(FName, Category)
		// Padding under each section; the insertion line is centred in it.
		SLATE_ARGUMENT(float, GapBelow)
		SLATE_EVENT(FOnKhaosPaletteCategoryDropped, OnCategoryDropped)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	enum class EDropZone : uint8 { None, Above, Below };

	// Zone for a category drag over this section, or None if the operation is not one we take.
	EDropZone GetDropZone(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) const;

	FName Category;
	float GapBelow = 4.f;
	FOnKhaosPaletteCategoryDropped OnCategoryDropped;
	EDropZone DropZone = EDropZone::None;
};
