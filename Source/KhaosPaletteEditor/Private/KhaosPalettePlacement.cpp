// Copyright 2026 Khaos Systems. All Rights Reserved.

#include "KhaosPalettePlacement.h"

#include "ActorFactories/ActorFactory.h"
#include "AssetSelection.h"
#include "Editor.h"
#include "Editor/ActorPositioning.h"
#include "EditorViewportClient.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "Layout/WidgetPath.h"
#include "LevelEditorViewport.h"
#include "SEditorViewport.h"
#include "SLevelViewport.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"
#include "SnappingUtils.h"
#include "AssetThumbnail.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

float FKhaosPalettePlacement::YawOffset = 0.f;
float FKhaosPalettePlacement::Scale = 1.f;

namespace
{
	// True when the cursor is on one of the controls drawn over the 3D view (the viewport toolbar's buttons and
	// menus, a text field...) rather than on the view itself. Those get the click, not placement.
	bool IsOverViewportControl(const FWidgetPath& Path, SEditorViewport& Viewport)
	{
		const TSharedPtr<FSceneViewport> SceneViewport = Viewport.GetSceneViewport();
		const TSharedPtr<SViewport> Surface = SceneViewport.IsValid() ? SceneViewport->GetViewportWidget().Pin() : nullptr;
		bool bPastSurface = false;
		for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
		{
			const TSharedRef<SWidget>& Widget = Path.Widgets[Index].Widget;
			if (!bPastSurface)
			{
				bPastSurface = Surface.IsValid() && Widget == Surface.ToSharedRef();
				continue;
			}
			const FString Type = Widget->GetTypeAsString();
			for (const TCHAR* Interactive : { TEXT("Button"), TEXT("Combo"), TEXT("CheckBox"), TEXT("Menu"), TEXT("Editable"), TEXT("Spin"), TEXT("Slider"), TEXT("ToolBar") })
			{
				if (Type.Contains(Interactive))
				{
					return true;
				}
			}
		}
		// Not over the 3D view at all: a toolbar or panel next to it inside the viewport widget.
		return !bPastSurface;
	}

	// The factory that makes an actor from the asset, or null if none can.
	UActorFactory* FindFactory(const FAssetData& AssetData, UObject*& OutObject)
	{
		OutObject = AssetData.GetAsset();
		UActorFactory* Found = OutObject ? FActorFactoryAssetProxy::GetFactoryForAssetObject(OutObject) : nullptr;
		FText Reason;
		return Found && Found->CanCreateActorFrom(AssetData, Reason) ? Found : nullptr;
	}
}

TSharedPtr<FKhaosPalettePlacement> FKhaosPalettePlacement::Begin(FKhaosPalettePlacementArgs Args)
{
	if (!GEditor || !FSlateApplication::IsInitialized() || !Args.Assets.IsValidIndex(Args.StartIndex))
	{
		return nullptr;
	}
	// Keep only what can be placed, and start on the asked-for asset if it is among them.
	const FSoftObjectPath StartPath = Args.Assets[Args.StartIndex].GetSoftObjectPath();
	TArray<FAssetData> Placeable;
	int32 StartIndex = 0;
	for (const FAssetData& AssetData : Args.Assets)
	{
		UObject* Object = nullptr;
		if (FindFactory(AssetData, Object))
		{
			if (AssetData.GetSoftObjectPath() == StartPath)
			{
				StartIndex = Placeable.Num();
			}
			Placeable.Add(AssetData);
		}
	}
	if (Placeable.IsEmpty())
	{
		return nullptr;
	}

	const TSharedRef<FKhaosPalettePlacement> Placement = MakeShareable(new FKhaosPalettePlacement());
	Placement->Assets = MoveTemp(Placeable);
	Placement->bRandomAfterPlace = Args.bRandomAfterPlace;
	Placement->bKeepPlacing = Args.bKeepPlacing;
	Placement->OnEnded = MoveTemp(Args.OnEnded);
	Placement->OnAssetChanged = MoveTemp(Args.OnAssetChanged);
	if (Args.FromDrag.IsValid())
	{
		Placement->Mode = EMode::WaitingForDrag;
		Placement->WatchedDrag = Args.FromDrag;
	}
	Placement->SelectAsset(StartIndex);
	FSlateApplication::Get().RegisterInputPreProcessor(Placement);
	Placement->bRegistered = true;
	return Placement;
}

FKhaosPalettePlacement::~FKhaosPalettePlacement()
{
	RemoveOverlay();
	DestroyPreview();
}

void FKhaosPalettePlacement::SelectAsset(int32 Index)
{
	if (!Assets.IsValidIndex(Index) || Index == Current)
	{
		return;
	}
	Current = Index;
	Asset = Assets[Index];
	UObject* Object = nullptr;
	Factory = FindFactory(Asset, Object);
	AssetObject = Object;
	DestroyPreview(); // the next tick makes the new one where the cursor is
	bHasPlacement = false;
	RefreshThumbnail();
	if (OnAssetChanged)
	{
		OnAssetChanged(Asset);
	}
}

void FKhaosPalettePlacement::End()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	bHasPlacement = false;
	DestroyPreview();
	RemoveOverlay();
	// After a placing click, stay registered until its release, so the viewport never sees half a click.
	if (bRegistered && !bSwallowNextLeftUp && FSlateApplication::IsInitialized())
	{
		bRegistered = false;
		FSlateApplication::Get().UnregisterInputPreProcessor(AsShared());
	}
	if (OnEnded)
	{
		TFunction<void()> Callback = MoveTemp(OnEnded);
		Callback();
	}
}

void FKhaosPalettePlacement::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	if (!bActive)
	{
		return;
	}
	if (Mode == EMode::WaitingForDrag)
	{
		// The drag ended without reaching a viewport (dropped on a category, cancelled): nothing to do.
		const TSharedPtr<FDragDropOperation> Drag = WatchedDrag.Pin();
		if (!Drag.IsValid() || SlateApp.GetDragDroppingContent() != Drag)
		{
			End();
			return;
		}
		FIntPoint Pixel;
		if (!FindViewportUnderCursor(SlateApp.GetCursorPos(), Pixel))
		{
			return;
		}
		// Over a level viewport: end Unreal's drag (its preview goes with it) and carry on as our own, which
		// snaps. The button is still held, so its release places.
		SlateApp.CancelDragDrop();
		Mode = EMode::Dragging;
	}
	// Moving the camera (right or middle button held): no preview flying along with it.
	const TSet<FKey>& Pressed = SlateApp.GetPressedMouseButtons();
	if (Pressed.Contains(EKeys::RightMouseButton) || Pressed.Contains(EKeys::MiddleMouseButton))
	{
		HidePreview();
		return;
	}
	UpdatePreview(SlateApp.GetCursorPos());
	EasePreview(DeltaTime);
}

namespace KhaosPalettePlacementStyle
{
	// Opaque, like the editor's own panels and toolbars: its panel colour, rounded, with the dropdown outline.
	// Keycaps and the thumbnail frame below use the editor's input field colours.
	const FSlateBrush* Panel()
	{
		static const FSlateRoundedBoxBrush Brush(FStyleColors::Panel, 6.f, FStyleColors::DropdownOutline, 1.f);
		return &Brush;
	}
	// Keys look like the editor's input fields.
	const FSlateBrush* Keycap()
	{
		static const FSlateRoundedBoxBrush Brush(FStyleColors::Input, 4.f, FStyleColors::InputOutline, 1.f);
		return &Brush;
	}
	const FSlateBrush* Thumb()
	{
		static const FSlateRoundedBoxBrush Brush(FStyleColors::Recessed, 4.f, FStyleColors::InputOutline, 1.f);
		return &Brush;
	}
	FSlateFontInfo Bold(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Bold", Size); }
	FSlateFontInfo Regular(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }

	// One keycap holding the whole combination ("Shift + Click"), a step quieter than the action beside it, so
	// the eye reads what happens first and the key second.
	TSharedRef<SWidget> Key(const TArray<FText>& Keys)
	{
		return SNew(SBorder)
			.BorderImage(Keycap())
			.Padding(FMargin(6.f, 1.f, 6.f, 2.f))
			[
				SNew(STextBlock)
				.Text(FText::Join(FText::FromString(TEXT(" + ")), Keys))
				.Font(Bold(8))
				.ColorAndOpacity(FStyleColors::Foreground)
			];
	}

	// One hint: its keys and what they do.
	TSharedRef<SWidget> Hint(const TArray<FText>& Keys, const FText& Action)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				Key(Keys)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock).Text(Action).Font(Regular(9)).ColorAndOpacity(FStyleColors::ForegroundHover)
			];
	}

	// The thin rule between two hints.
	TSharedRef<SWidget> Divider()
	{
		return SNew(SBox)
			.WidthOverride(1.f)
			.Padding(FMargin(0.f, 7.f))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("WhiteBrush"))
				.ColorAndOpacity(FStyleColors::Hover)
			];
	}
}

void FKhaosPalettePlacement::UpdateOverlay(FLevelEditorViewportClient* Client)
{
	using namespace KhaosPalettePlacementStyle;

	const TSharedPtr<SEditorViewport> Widget = Client ? Client->GetEditorViewportWidget() : nullptr;
	const TSharedPtr<SLevelViewport> Target = Widget.IsValid() && Widget->GetType() == TEXT("SLevelViewport")
		? StaticCastSharedPtr<SLevelViewport>(Widget) : nullptr;
	if (Target == OverlayViewport.Pin())
	{
		return;
	}
	RemoveOverlay();
	if (!Target.IsValid())
	{
		return;
	}
	if (!ThumbnailPool.IsValid())
	{
		ThumbnailPool = MakeShared<FAssetThumbnailPool>(8);
	}

	// The keys that apply right now.
	const FText Click = NSLOCTEXT("KhaosPalettePlacement", "KeyClick", "Click");
	const FText Shift = NSLOCTEXT("KhaosPalettePlacement", "KeyShift", "Shift");
	const FText Ctrl = NSLOCTEXT("KhaosPalettePlacement", "KeyCtrl", "Ctrl");
	const FText Scroll = NSLOCTEXT("KhaosPalettePlacement", "KeyScroll", "Scroll");
	const FText Tab = NSLOCTEXT("KhaosPalettePlacement", "KeyTab", "Tab");
	TSharedRef<SHorizontalBox> Hints = SNew(SHorizontalBox);
	// Hints in one line, each padded evenly, with a thin rule between them.
	auto AddHint = [&Hints](const TArray<FText>& Keys, const FText& Action)
	{
		if (Hints->NumSlots() > 0)
		{
			Hints->AddSlot().AutoWidth()[Divider()];
		}
		Hints->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.f, 7.f))
		[
			Hint(Keys, Action)
		];
	};
	if (Mode == EMode::Dragging || Mode == EMode::WaitingForDrag)
	{
		AddHint({ NSLOCTEXT("KhaosPalettePlacement", "KeyRelease", "Release") }, NSLOCTEXT("KhaosPalettePlacement", "ActPlace", "Place"));
	}
	else
	{
		AddHint({ Click }, NSLOCTEXT("KhaosPalettePlacement", "ActPlace", "Place"));
		if (!bKeepPlacing)
		{
			AddHint({ Shift, Click }, NSLOCTEXT("KhaosPalettePlacement", "ActKeep", "Keep placing"));
		}
		if (Assets.Num() > 1)
		{
			AddHint({ Tab }, NSLOCTEXT("KhaosPalettePlacement", "ActSwitch", "Next"));
		}
	}
	AddHint({ Shift, Scroll }, NSLOCTEXT("KhaosPalettePlacement", "ActRotate", "Rotate"));
	AddHint({ Ctrl, Scroll }, NSLOCTEXT("KhaosPalettePlacement", "ActScale", "Scale"));
	AddHint({ NSLOCTEXT("KhaosPalettePlacement", "KeyEsc", "Esc") }, NSLOCTEXT("KhaosPalettePlacement", "ActStop", "Stop"));

	TSharedRef<SOverlay> Root = SNew(SOverlay).Visibility(EVisibility::HitTestInvisible);

	// The label that rides beside the cursor: thumbnail, name, position in the set, and the turn and scale
	// once they are changed. Sized to its content; hidden while the preview is (camera moving, cursor off the
	// level).
	Root->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Top)
	.Padding(TAttribute<FMargin>::CreateLambda([this]()
	{
		const FVector2D Cursor = CursorInOverlay();
		return FMargin(Cursor.X + 18.0, Cursor.Y + 22.0, 0.0, 0.0);
	}))
	[
		SNew(SBorder)
		.BorderImage(Panel())
		.Padding(FMargin(6.f, 6.f, 12.f, 6.f))
		.Visibility_Lambda([this]() { return bHasPlacement ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(Thumb())
				.Padding(1.f)
				[
					SAssignNew(ThumbnailBox, SBox)
					.WidthOverride(32.f)
					.HeightOverride(32.f)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return FText::FromName(Asset.AssetName); })
						.Font(Bold(10))
						.ColorAndOpacity(FStyleColors::ForegroundHover)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					.Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Visibility(Assets.Num() > 1 ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
						.Text_Lambda([this]() { return FText::Format(NSLOCTEXT("KhaosPalettePlacement", "Position", "{0} / {1}"), Current + 1, Assets.Num()); })
						.Font(Regular(9))
						.ColorAndOpacity(FStyleColors::Foreground)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 2.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([]() { return !FMath::IsNearlyZero(YawOffset) || !FMath::IsNearlyEqual(Scale, 1.f) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					.Text_Lambda([]()
					{
						// Degree sign, middle dot and multiplication sign, written as code points so the source stays ASCII.
						return FText::FromString(FString::Printf(TEXT("%d%c   %c   %s%c"),
							FMath::RoundToInt(YawOffset), TCHAR(0x00B0), TCHAR(0x00B7),
							*FString::SanitizeFloat(FMath::RoundToFloat(Scale * 100.f) / 100.f), TCHAR(0x00D7)));
					})
					.Font(Regular(9))
					.ColorAndOpacity(FStyleColors::AccentOrange)
				]
			]
		]
	];

	// The key hints, in a slim bar at the bottom.
	Root->AddSlot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(FMargin(16.f, 0.f, 16.f, 16.f))
	[
		SNew(SBorder)
		.BorderImage(Panel())
		.Padding(FMargin(4.f, 0.f))
		[
			Hints
		]
	];

	Overlay = Root;
	Target->AddOverlayWidget(Overlay.ToSharedRef());
	OverlayViewport = Target;
	RefreshThumbnail();
}

FVector2D FKhaosPalettePlacement::CursorInOverlay() const
{
	if (!Overlay.IsValid() || !FSlateApplication::IsInitialized())
	{
		return FVector2D::ZeroVector;
	}
	return Overlay->GetTickSpaceGeometry().AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
}

void FKhaosPalettePlacement::RefreshThumbnail()
{
	if (!ThumbnailBox.IsValid() || !ThumbnailPool.IsValid())
	{
		return;
	}
	Thumbnail = MakeShared<FAssetThumbnail>(Asset, 64, 64, ThumbnailPool);
	FAssetThumbnailConfig Config;
	Config.ThumbnailLabel = EThumbnailLabel::NoLabel;
	Config.bAllowHintText = false;
	Config.AssetTypeColorOverride = FLinearColor::Transparent;
	ThumbnailBox->SetContent(Thumbnail->MakeThumbnailWidget(Config));
}

void FKhaosPalettePlacement::RemoveOverlay()
{
	if (const TSharedPtr<SLevelViewport> Target = OverlayViewport.Pin())
	{
		if (Overlay.IsValid())
		{
			Target->RemoveOverlayWidget(Overlay.ToSharedRef());
		}
	}
	OverlayViewport.Reset();
	Overlay.Reset();
	ThumbnailBox.Reset();
}

void FKhaosPalettePlacement::HidePreview()
{
	bHasPlacement = false;
	AActor* PreviewActor = Preview.Get();
	if (PreviewActor && !PreviewActor->IsTemporarilyHiddenInEditor())
	{
		PreviewActor->SetIsTemporarilyHiddenInEditor(true);
		GEditor->RedrawLevelEditingViewports(false);
	}
}

bool FKhaosPalettePlacement::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (bActive && InKeyEvent.GetKey() == EKeys::Escape)
	{
		End();
		return true;
	}
	// Tab and Shift-Tab: the next and previous asset.
	if (bActive && Mode == EMode::Click && Assets.Num() > 1 && InKeyEvent.GetKey() == EKeys::Tab
		&& !InKeyEvent.IsControlDown() && !InKeyEvent.IsAltDown())
	{
		const int32 Step = InKeyEvent.IsShiftDown() ? -1 : 1;
		SelectAsset((Current + Step + Assets.Num()) % Assets.Num());
		UpdatePreview(SlateApp.GetCursorPos());
		return true;
	}
	return false;
}

bool FKhaosPalettePlacement::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (!bActive || Mode != EMode::Click || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return false; // right-drag and the rest reach the viewport, so the camera still works
	}
	FIntPoint Pixel;
	if (FindViewportUnderCursor(MouseEvent.GetScreenSpacePosition(), Pixel))
	{
		// Place; with Keep Placing on, or Shift held, stay in placement for the next click. Its release is ours too.
		UpdatePreview(MouseEvent.GetScreenSpacePosition());
		const bool bPlaced = bHasPlacement;
		if (bPlaced)
		{
			Place();
		}
		bSwallowNextLeftUp = true;
		if (bPlaced && !bKeepPlacing && !MouseEvent.IsShiftDown())
		{
			End();
		}
		return true;
	}
	// Anywhere else (the palette, a menu, another panel): stop placing and let the click do what it does there.
	End();
	return false;
}

bool FKhaosPalettePlacement::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent)
{
	// Over a viewport while placing: Shift-scroll rotates, Ctrl-scroll scales. The plain wheel, or the wheel over
	// anything else, does what it always does, such as zooming the camera.
	if (!bActive || Mode == EMode::WaitingForDrag || InGestureEvent || InWheelEvent.GetWheelDelta() == 0.f || InWheelEvent.IsAltDown())
	{
		return false;
	}
	FIntPoint Pixel;
	if (!FindViewportUnderCursor(InWheelEvent.GetScreenSpacePosition(), Pixel))
	{
		return false;
	}
	const float Direction = InWheelEvent.GetWheelDelta() > 0.f ? 1.f : -1.f;
	const ULevelEditorViewportSettings* Settings = GetDefault<ULevelEditorViewportSettings>();
	if (InWheelEvent.IsShiftDown())
	{
		// The rotation snap step when rotation snapping is on, else 15 degrees.
		const float Step = Settings->RotGridEnabled && GEditor->GetRotGridSize().Yaw > 0.f ? GEditor->GetRotGridSize().Yaw : 15.f;
		YawOffset = FRotator::NormalizeAxis(YawOffset + Direction * Step);
	}
	else if (InWheelEvent.IsControlDown())
	{
		// The scale snap step when scale snapping is on, else 10% at a time.
		const float Step = GEditor->GetScaleGridSize();
		Scale = Settings->SnapScaleEnabled && Step > 0.f
			? FMath::Max(FMath::Max(Step, 0.1f), Scale + Direction * Step)
			: FMath::Clamp(Scale * (Direction > 0.f ? 1.1f : 1.f / 1.1f), 0.1f, 100.f);
	}
	else
	{
		return false;
	}
	UpdatePreview(InWheelEvent.GetScreenSpacePosition());
	return true;
}

bool FKhaosPalettePlacement::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// Releasing a drag we took over: place if it is over a level viewport, otherwise just stop.
	if (bActive && Mode == EMode::Dragging && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FIntPoint Pixel;
		const bool bOverViewport = FindViewportUnderCursor(MouseEvent.GetScreenSpacePosition(), Pixel) != nullptr;
		if (bOverViewport)
		{
			UpdatePreview(MouseEvent.GetScreenSpacePosition());
			if (bHasPlacement)
			{
				Place();
			}
		}
		End();
		return bOverViewport;
	}
	if (bSwallowNextLeftUp && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bSwallowNextLeftUp = false;
		if (!bActive && bRegistered && FSlateApplication::IsInitialized())
		{
			bRegistered = false;
			FSlateApplication::Get().UnregisterInputPreProcessor(AsShared());
		}
		return true;
	}
	return false;
}

bool FKhaosPalettePlacement::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// The second press of a quick double click on the viewport belongs to the placing click too.
	return bSwallowNextLeftUp && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
}

FLevelEditorViewportClient* FKhaosPalettePlacement::FindViewportUnderCursor(const FVector2D& ScreenPosition, FIntPoint& OutPixel) const
{
	if (!GEditor)
	{
		return nullptr;
	}
	FSlateApplication& SlateApp = FSlateApplication::Get();
	// The widgets actually under the cursor: a viewport covered by the palette, a menu or a dialog does not count.
	const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(ScreenPosition, SlateApp.GetInteractiveTopLevelWindows());
	if (!Path.IsValid())
	{
		return nullptr;
	}
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (!Client || !Client->Viewport || !Client->GetWorld())
		{
			continue;
		}
		const TSharedPtr<SEditorViewport> Widget = Client->GetEditorViewportWidget();
		if (!Widget.IsValid() || !Path.ContainsWidget(Widget.Get()) || IsOverViewportControl(Path, *Widget))
		{
			continue;
		}
		// Measure against the 3D view itself (the SViewport the scene renders into), not the whole viewport widget,
		// which in 5.8 also holds the toolbar row: scaling the cursor by that taller box put the hit below the cursor.
		const TSharedPtr<FSceneViewport> SceneViewport = Widget->GetSceneViewport();
		const TSharedPtr<SViewport> Surface = SceneViewport.IsValid() ? SceneViewport->GetViewportWidget().Pin() : nullptr;
		const FGeometry& Geometry = Surface.IsValid() ? Surface->GetTickSpaceGeometry() : Widget->GetTickSpaceGeometry();
		const FVector2D LocalSize = Geometry.GetLocalSize();
		const FIntPoint ViewportSize = Client->Viewport->GetSizeXY();
		if (LocalSize.X <= 0.0 || LocalSize.Y <= 0.0 || ViewportSize.X <= 0 || ViewportSize.Y <= 0)
		{
			continue;
		}
		const FVector2D Local = Geometry.AbsoluteToLocal(ScreenPosition);
		OutPixel = FIntPoint(
			FMath::FloorToInt32(Local.X / LocalSize.X * ViewportSize.X),
			FMath::FloorToInt32(Local.Y / LocalSize.Y * ViewportSize.Y));
		if (OutPixel.X < 0 || OutPixel.Y < 0 || OutPixel.X >= ViewportSize.X || OutPixel.Y >= ViewportSize.Y)
		{
			continue;
		}
		return Client;
	}
	return nullptr;
}

bool FKhaosPalettePlacement::ComputePlacement(FLevelEditorViewportClient& Client, const FIntPoint& Pixel, FTransform& OutTransform)
{
	AActor* PreviewActor = GetOrCreatePreview(Client.GetWorld());
	if (!PreviewActor)
	{
		return false;
	}

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Client.Viewport, Client.GetScene(), Client.EngineShowFlags)
		.SetRealtimeUpdate(Client.IsRealtime()));
	FSceneView* View = Client.CalcSceneView(&ViewFamily);
	if (!View)
	{
		return false;
	}
	FViewportCursorLocation Cursor(View, &Client, Pixel.X, Pixel.Y);

	// The same steps as a Content Browser drop: trace under the cursor (past our own preview), snap the hit to
	// the grid when grid snapping is on, then let the factory sit the actor on the surface, aligned to it when
	// surface snapping says so.
	const TArray<AActor*> IgnoreActors = { PreviewActor };
	const FActorPositionTraceResult Trace = FActorPositioning::TraceWorldForPositionWithDefault(Cursor, *View, &IgnoreActors);
	FVector Location = Trace.Location;
	FVector Normal = Trace.SurfaceNormal;
	if (Trace.State != FActorPositionTraceResult::HitSuccess)
	{
		// Nothing under the cursor (the sky, empty space). Unreal's fallback puts the actor just in front of the
		// camera, which is never what anyone wants; use the ground plane instead, and nothing above the horizon.
		const FVector Origin = Cursor.GetOrigin();
		const FVector Direction = Cursor.GetDirection();
		if (Direction.Z > -UE_KINDA_SMALL_NUMBER || Origin.Z <= 0.0)
		{
			return false;
		}
		Location = Origin + Direction * (-Origin.Z / Direction.Z);
		Normal = FVector::UpVector;
	}
	FSnappingUtils::SnapPointToGrid(Location, FVector::ZeroVector);

	const FSnappedPositioningData Positioning = FSnappedPositioningData(&Client, Location, Normal)
		.UseFactory(Factory.Get())
		.UsePlacementExtent(PreviewActor->GetPlacementExtent());
	OutTransform = FActorPositioning::GetSnappedSurfaceAlignedTransform(Positioning);
	// Shift-scroll and Ctrl-scroll on top: turn about the actor's own up axis, so a surface-aligned actor
	// turns about the surface normal, and scale uniformly.
	OutTransform.SetRotation(OutTransform.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(YawOffset)));
	OutTransform.SetScale3D(OutTransform.GetScale3D() * Scale);
	return true;
}

void FKhaosPalettePlacement::UpdatePreview(const FVector2D& ScreenPosition)
{
	FIntPoint Pixel;
	FLevelEditorViewportClient* Client = FindViewportUnderCursor(ScreenPosition, Pixel);
	UpdateOverlay(Client);
	FTransform Transform;
	bHasPlacement = Client && ComputePlacement(*Client, Pixel, Transform);

	AActor* PreviewActor = Preview.Get();
	if (!PreviewActor)
	{
		return;
	}
	if (!bHasPlacement)
	{
		if (!PreviewActor->IsTemporarilyHiddenInEditor())
		{
			PreviewActor->SetIsTemporarilyHiddenInEditor(true);
			GEditor->RedrawLevelEditingViewports(false);
		}
		return;
	}
	PreviewTransform = Transform;
	if (PreviewActor->IsTemporarilyHiddenInEditor())
	{
		// Appearing: straight to where it belongs, never sliding in from where it was last shown.
		ShownTransform = PreviewTransform;
		FTransform Drawn = ShownTransform;
		Drawn.SetScale3D(Drawn.GetScale3D() * FMath::Max(GetPopScale(), 0.01f));
		PreviewActor->SetActorTransform(Drawn);
		PreviewActor->SetIsTemporarilyHiddenInEditor(false);
		PreviewActor->PostEditMove(false);
		Client->Invalidate();
	}
}

void FKhaosPalettePlacement::EasePreview(float DeltaTime)
{
	// The preview glides to each new snapped spot, turn and scale instead of jumping; the actor is still placed
	// at the snapped transform itself. Exponential easing, so it covers about 95% of the way in 0.1 s.
	AActor* PreviewActor = Preview.Get();
	const float Pop = GetPopScale();
	const bool bPopping = Pop < 1.f || bPopEnding;
	if (!PreviewActor || !bHasPlacement || PreviewActor->IsTemporarilyHiddenInEditor()
		|| (!bPopping && ShownTransform.Equals(PreviewTransform, UE_KINDA_SMALL_NUMBER)))
	{
		return;
	}
	bPopEnding = Pop < 1.f; // one more frame at full size once the pop is over
	constexpr float Speed = 30.f;
	const float Alpha = 1.f - FMath::Exp(-Speed * DeltaTime);
	FTransform Next;
	Next.SetLocation(FMath::Lerp(ShownTransform.GetLocation(), PreviewTransform.GetLocation(), Alpha));
	Next.SetRotation(FQuat::Slerp(ShownTransform.GetRotation(), PreviewTransform.GetRotation(), Alpha).GetNormalized());
	Next.SetScale3D(FMath::Lerp(ShownTransform.GetScale3D(), PreviewTransform.GetScale3D(), Alpha));
	// Close enough to see no difference: finish, so it stops redrawing.
	if (Next.Equals(PreviewTransform, 0.05))
	{
		Next = PreviewTransform;
	}
	ShownTransform = Next;
	FTransform Drawn = ShownTransform;
	Drawn.SetScale3D(Drawn.GetScale3D() * FMath::Max(Pop, 0.01f));
	PreviewActor->SetActorTransform(Drawn);
	PreviewActor->PostEditMove(false);
	GEditor->RedrawLevelEditingViewports(false);
}

float FKhaosPalettePlacement::GetPopScale() const
{
	// After a placement the preview grows back from nothing with a slight overshoot (ease-out-back), so the
	// placed actor and the next preview read as two things.
	constexpr double Duration = 0.22;
	const double Elapsed = FPlatformTime::Seconds() - LastPlacedTime;
	if (Elapsed >= Duration || LastPlacedTime <= 0.0)
	{
		return 1.f;
	}
	const float T = static_cast<float>(Elapsed / Duration);
	constexpr float Overshoot = 0.6f; // peaks about 3% over full size
	const float U = T - 1.f;
	return 1.f + (Overshoot + 1.f) * U * U * U + Overshoot * U * U;
}

AActor* FKhaosPalettePlacement::GetOrCreatePreview(UWorld* World)
{
	if (AActor* Existing = Preview.Get())
	{
		if (Existing->GetWorld() == World)
		{
			return Existing;
		}
		DestroyPreview(); // the viewport now shows another world
	}
	UObject* Object = AssetObject.Get();
	UActorFactory* ActorFactory = Factory.Get();
	ULevel* Level = World ? World->GetCurrentLevel() : nullptr;
	if (!Object || !ActorFactory || !Level)
	{
		return nullptr;
	}
	// Like the viewport's own drag preview: transient, kept out of the Outliner and undo, never saved.
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags = RF_Transient;
	SpawnParameters.bTemporaryEditorActor = true;
	SpawnParameters.bHideFromSceneOutliner = true;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = ActorFactory->CreateActor(Object, Level, FTransform::Identity, SpawnParameters);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->bIsEditorPreviewActor = true;
	Actor->SetActorEnableCollision(false); // so the cursor trace never hits the preview itself
	Actor->SetIsTemporarilyHiddenInEditor(true);
	Preview = Actor;
	return Actor;
}

void FKhaosPalettePlacement::DestroyPreview()
{
	AActor* Actor = Preview.Get();
	Preview.Reset();
	if (Actor && GEditor)
	{
		if (UWorld* World = Actor->GetWorld())
		{
			World->EditorDestroyActor(Actor, /*bShouldModifyLevel*/ false);
			GEditor->RedrawLevelEditingViewports(false);
		}
	}
}

void FKhaosPalettePlacement::Place()
{
	UActorFactory* ActorFactory = Factory.Get();
	if (!ActorFactory || !bHasPlacement)
	{
		return;
	}
	const FTransform Transform = PreviewTransform;
	DestroyPreview(); // the next tick puts it back under the cursor
	bHasPlacement = false;
	// One undo step, selected afterwards, exactly as the engine places from a factory.
	if (GEditor->UseActorFactory(ActorFactory, Asset, &Transform))
	{
		LastPlacedTime = FPlatformTime::Seconds();
	}

	if (bRandomAfterPlace && Assets.Num() > 1)
	{
		// Any of the others, so the next one is always different.
		const int32 Offset = FMath::RandRange(1, Assets.Num() - 1);
		SelectAsset((Current + Offset) % Assets.Num());
	}
}
