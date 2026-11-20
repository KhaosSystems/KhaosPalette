// Copyright 2026 Khaos Systems. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "Framework/Application/IInputProcessor.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class SLevelViewport;
class SWidget;
class FDragDropOperation;
class FLevelEditorViewportClient;
class UActorFactory;

struct FKhaosPalettePlacementArgs
{
	// What can be placed; scrolling over a viewport switches between them. Assets no actor factory can place are
	// left out.
	TArray<FAssetData> Assets;
	// The one placed first.
	int32 StartIndex = 0;
	// After each placement, switch to a random other one of Assets.
	bool bRandomAfterPlace = false;
	// Each click places and placement goes on until Escape or a click elsewhere. Off, a click places one and
	// ends placement, unless Shift is held.
	bool bKeepPlacing = false;
	// A tile drag to take over once it reaches a level viewport, placing once on release. Without one, clicks
	// on a viewport place (see bKeepPlacing), and Escape or a click anywhere else ends placement.
	TSharedPtr<FDragDropOperation> FromDrag;
	// Runs once, when placement ends for any reason.
	TFunction<void()> OnEnded;
	// Runs whenever the asset being placed changes, the first one included.
	TFunction<void(const FAssetData&)> OnAssetChanged;
};

// Placing from the palette, with a preview of our own. Unreal's drag-and-drop preview follows the raw cursor hit
// and only snaps the final drop; this one snaps exactly as the actor will be placed (grid, surface snapping and
// alignment from the level editor's settings), and the actor lands where the preview shows.
//
// Runs as a Slate input pre-processor while active, so it sees clicks, the wheel and keys before the viewport
// does: a left click on a level viewport places, the wheel over one switches asset (when there is more than one),
// a left click anywhere else ends placement and goes through (so clicks in the palette still select), Escape
// ends it. Everything else, such as right-drag to look around, reaches the viewport as usual.
//
// For a tile dragged out of the palette it waits instead: the drag stays Unreal's own while it is over the
// palette, so tiles still drop onto category sections, and the moment it reaches a level viewport this ends that
// drag and takes over, placing on release.
class FKhaosPalettePlacement : public IInputProcessor, public TSharedFromThis<FKhaosPalettePlacement>
{
public:
	// Returns null if none of the assets can be placed.
	static TSharedPtr<FKhaosPalettePlacement> Begin(FKhaosPalettePlacementArgs Args);

	virtual ~FKhaosPalettePlacement() override;

	void End();
	bool IsActive() const { return bActive; }
	const FAssetData& GetAsset() const { return Asset; }
	int32 NumAssets() const { return Assets.Num(); }

	// IInputProcessor
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("KhaosPalettePlacement"); }

private:
	FKhaosPalettePlacement() = default;

	// Makes Assets[Index] the one being placed.
	void SelectAsset(int32 Index);
	// The level viewport under the cursor, if it is the topmost thing there (not covered by a window or menu),
	// with the cursor in that viewport's pixels.
	FLevelEditorViewportClient* FindViewportUnderCursor(const FVector2D& ScreenPosition, FIntPoint& OutPixel) const;
	// Where the actor would go for the cursor at that pixel, snapped as the level editor's settings say.
	bool ComputePlacement(FLevelEditorViewportClient& Client, const FIntPoint& Pixel, FTransform& OutTransform);
	void UpdatePreview(const FVector2D& ScreenPosition);
	void HidePreview();
	// The hint panel, shown over the viewport the cursor is in.
	void UpdateOverlay(FLevelEditorViewportClient* Client);
	void RemoveOverlay();
	FVector2D CursorInOverlay() const;
	void RefreshThumbnail();
	AActor* GetOrCreatePreview(UWorld* World);
	void DestroyPreview();
	void Place();

	TArray<FAssetData> Assets;
	int32 Current = INDEX_NONE;
	bool bRandomAfterPlace = false;
	bool bKeepPlacing = false;
	FAssetData Asset; // Assets[Current]
	TWeakObjectPtr<UObject> AssetObject;
	TWeakObjectPtr<UActorFactory> Factory;
	TFunction<void()> OnEnded;
	TFunction<void(const FAssetData&)> OnAssetChanged;
	TWeakObjectPtr<AActor> Preview;
	FTransform PreviewTransform;
	enum class EMode : uint8
	{
		Click,           // places on a left click on a viewport; more with bKeepPlacing or Shift
		WaitingForDrag,  // a tile drag still over the palette; nothing shown yet
		Dragging,        // took over that drag; places once on release
	};
	EMode Mode = EMode::Click;
	TWeakPtr<FDragDropOperation> WatchedDrag;
	bool bHasPlacement = false; // PreviewTransform is valid for the viewport under the cursor
	// Shift-scroll and Ctrl-scroll, on top of the snapped placement. Kept for the next placement too.
	static float YawOffset;
	static float Scale;
	TWeakPtr<SLevelViewport> OverlayViewport;
	TSharedPtr<SWidget> Overlay;
	TSharedPtr<class SBox> ThumbnailBox;
	TSharedPtr<class FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<class FAssetThumbnail> Thumbnail;
	bool bSwallowNextLeftUp = false; // the press that placed was ours, so its release is too
	bool bRegistered = false;
	bool bActive = true;
};
