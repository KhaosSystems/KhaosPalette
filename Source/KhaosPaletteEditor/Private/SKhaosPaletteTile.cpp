// Copyright Khaos Games ApS. All Rights Reserved.

#include "SKhaosPaletteTile.h"

#include "AssetThumbnail.h"
#include "Framework/Text/TextLayout.h"
#include "SKhaosPaletteTab.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SKhaosPaletteTile::Construct(const FArguments& InArgs)
{
	Item = InArgs._Item;
	IsSelected = InArgs._IsSelected;
	OnClickedDelegate = InArgs._OnClicked;
	OnReleasedDelegate = InArgs._OnReleased;
	OnDoubleClickedDelegate = InArgs._OnDoubleClicked;
	OnContextMenuDelegate = InArgs._OnContextMenu;
	OnDragRequestedDelegate = InArgs._OnDragRequested;

	FAssetThumbnailConfig ThumbnailConfig;
	ThumbnailConfig.ThumbnailLabel = EThumbnailLabel::NoLabel;
	ThumbnailConfig.bAllowHintText = false;
	const TSharedRef<SWidget> ThumbnailWidget = Item->Thumbnail.IsValid()
		? Item->Thumbnail->MakeThumbnailWidget(ThumbnailConfig)
		: StaticCastSharedRef<SWidget>(SNew(SBorder).BorderImage(FAppStyle::GetBrush("AssetThumbnail.ClassBackground")));

	SetCursor(EMouseCursor::GrabHand);
	SetToolTipText(FText::FromString(Item->AssetData.GetObjectPathString()));

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(this, &SKhaosPaletteTile::GetBackground)
		.Padding(4.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(InArgs._TileSize)
				.HeightOverride(InArgs._TileSize)
				[
					ThumbnailWidget
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.f, 3.f, 0.f, 0.f)
			[
				SNew(SBox)
				.WidthOverride(InArgs._TileSize)
				[
					SNew(STextBlock)
					.Text(Item->Label)
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.Justification(ETextJustify::Center)
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
			]
		]
	];
}

FReply SKhaosPaletteTile::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (OnClickedDelegate.IsBound())
		{
			OnClickedDelegate.Execute(Item, MouseEvent);
		}
		// Focus so Delete reaches the tab through the focus path.
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton).SetUserFocus(AsShared(), EFocusCause::Mouse);
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SKhaosPaletteTile::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && OnContextMenuDelegate.IsBound())
	{
		return OnContextMenuDelegate.Execute(Item, MouseEvent);
	}
	// A drag started from the press is already routed as a drop, so reaching here means a plain click.
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OnReleasedDelegate.IsBound())
	{
		return OnReleasedDelegate.Execute(Item, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply SKhaosPaletteTile::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OnDoubleClickedDelegate.IsBound())
	{
		return OnDoubleClickedDelegate.Execute(Item, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply SKhaosPaletteTile::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnDragRequestedDelegate.IsBound() ? OnDragRequestedDelegate.Execute(Item, MouseEvent) : FReply::Unhandled();
}

const FSlateBrush* SKhaosPaletteTile::GetBackground() const
{
	if (IsSelected.Get(false))
	{
		return FAppStyle::GetBrush("Brushes.Select");
	}
	return IsHovered() ? FAppStyle::GetBrush("Brushes.Hover") : FAppStyle::GetBrush("NoBrush");
}
