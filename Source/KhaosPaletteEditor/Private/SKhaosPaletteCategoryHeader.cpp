// Copyright Khaos Systems. All Rights Reserved.

#include "SKhaosPaletteCategoryHeader.h"

#include "Brushes/SlateImageBrush.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "KhaosPaletteEditor"

namespace
{
	constexpr float IconSize = 20.f;
	constexpr float DropLineThickness = 3.f;
}

TSharedRef<FKhaosPaletteCategoryDragDropOp> FKhaosPaletteCategoryDragDropOp::New(FName Category, const FText& Label, const FSlateBrush* Icon)
{
	TSharedRef<FKhaosPaletteCategoryDragDropOp> Op = MakeShared<FKhaosPaletteCategoryDragDropOp>();
	Op->Category = Category;
	Op->DefaultHoverText = FText::Format(LOCTEXT("MoveCategoryDrag", "Move {0}"), Label);
	Op->DefaultHoverIcon = Icon;
	Op->ResetToDefaultToolTip();
	Op->Construct();
	return Op;
}

// ---------------------------------------------------------------------------------------------------------------------
// Header

void SKhaosPaletteCategoryHeader::Construct(const FArguments& InArgs)
{
	Category = InArgs._Category;
	Title = InArgs._Title;
	bReorderable = InArgs._bReorderable;

	if (InArgs._Icon)
	{
		IconTexture.Reset(InArgs._Icon);
		IconBrush = FSlateImageBrush(InArgs._Icon, FVector2f(IconSize, IconSize));
	}

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Icons.DragHandle"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Visibility(bReorderable ? EVisibility::Visible : EVisibility::Collapsed)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(IconSize)
			.HeightOverride(IconSize)
			.Visibility(InArgs._Icon ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SImage).Image(&IconBrush)
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("CategoryHeader", "{0} ({1})"), Title, InArgs._Count))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]

		// Expander arrow last, so the grip and icon lead the row.
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(6.f, 0.f, 0.f, 0.f)
		[
			SNew(SImage)
			.Image(this, &SKhaosPaletteCategoryHeader::GetExpanderImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];
}

const FSlateBrush* SKhaosPaletteCategoryHeader::GetExpanderImage() const
{
	const FExpandableAreaStyle& Style = FAppStyle::GetWidgetStyle<FExpandableAreaStyle>("ExpandableArea");
	const TSharedPtr<SExpandableArea> Area = OwnerArea.Pin();
	return Area.IsValid() && !Area->IsExpanded() ? &Style.CollapsedImage : &Style.ExpandedImage;
}

FReply SKhaosPaletteCategoryHeader::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Reorderable headers take the click so a drag can start; the toggle happens on release instead of in the
	// area's own button, which never sees this press.
	if (bReorderable && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}
	return FReply::Unhandled();
}

FReply SKhaosPaletteCategoryHeader::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bReorderable && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (const TSharedPtr<SExpandableArea> Area = OwnerArea.Pin())
		{
			Area->SetExpanded_Animated(!Area->IsExpanded());
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SKhaosPaletteCategoryHeader::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled().BeginDragDrop(FKhaosPaletteCategoryDragDropOp::New(Category, Title, IconTexture ? &IconBrush : FAppStyle::GetBrush("Icons.DragHandle")));
}

// ---------------------------------------------------------------------------------------------------------------------
// Section

void SKhaosPaletteCategorySection::Construct(const FArguments& InArgs)
{
	Category = InArgs._Category;
	GapBelow = InArgs._GapBelow;
	OnCategoryDropped = InArgs._OnCategoryDropped;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

SKhaosPaletteCategorySection::EDropZone SKhaosPaletteCategorySection::GetDropZone(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) const
{
	const TSharedPtr<FKhaosPaletteCategoryDragDropOp> Op = DragDropEvent.GetOperationAs<FKhaosPaletteCategoryDragDropOp>();
	if (!Op.IsValid() || Op->Category == Category)
	{
		return EDropZone::None;
	}
	// Uncategorised is pinned last, so it only takes drops above it.
	if (Category.IsNone())
	{
		return EDropZone::Above;
	}
	const float LocalY = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition()).Y;
	return LocalY < MyGeometry.GetLocalSize().Y * 0.5f ? EDropZone::Above : EDropZone::Below;
}

FReply SKhaosPaletteCategorySection::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	DropZone = GetDropZone(MyGeometry, DragDropEvent);
	return DropZone == EDropZone::None ? FReply::Unhandled() : FReply::Handled();
}

void SKhaosPaletteCategorySection::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	DropZone = EDropZone::None;
}

FReply SKhaosPaletteCategorySection::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const EDropZone Zone = GetDropZone(MyGeometry, DragDropEvent);
	DropZone = EDropZone::None;
	if (Zone == EDropZone::None)
	{
		return FReply::Unhandled();
	}
	OnCategoryDropped.ExecuteIfBound(DragDropEvent.GetOperationAs<FKhaosPaletteCategoryDragDropOp>()->Category, Category, Zone == EDropZone::Below);
	return FReply::Handled();
}

int32 SKhaosPaletteCategorySection::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (DropZone != EDropZone::None)
	{
		// Centre the line in the gap above (the previous section's GapBelow) or below this section. Nothing between
		// sections clips, so drawing just outside our own bounds is fine.
		const FVector2f Size = AllottedGeometry.GetLocalSize();
		const float CentreY = DropZone == EDropZone::Above ? -GapBelow * 0.5f : Size.Y + GapBelow * 0.5f;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(Size.X, DropLineThickness), FSlateLayoutTransform(FVector2f(0.f, CentreY - DropLineThickness * 0.5f))),
			FAppStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FAppStyle::GetSlateColor("SelectionColor").GetColor(InWidgetStyle));
	}
	return LayerId;
}

#undef LOCTEXT_NAMESPACE
