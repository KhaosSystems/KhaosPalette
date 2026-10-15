// Copyright Khaos Systems. All Rights Reserved.

#include "SKhaosPaletteTab.h"

#include "Algo/AllOf.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetThumbnail.h"
#include "ContentBrowserModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Text/TextLayout.h"
#include "GenericPlatform/GenericWindow.h"
#include "IContentBrowserSingleton.h"
#include "KhaosPalette.h"
#include "KhaosPaletteEditorHelpers.h"
#include "KhaosPaletteStyle.h"
#include "Layout/WidgetPath.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "SAssetDropTarget.h"
#include "SDropTarget.h"
#include "SKhaosPaletteCategoryHeader.h"
#include "SKhaosPaletteTile.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleDefaults.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#define LOCTEXT_NAMESPACE "KhaosPaletteEditor"

namespace
{
	const TCHAR* ConfigSection = TEXT("KhaosPalette");
	const TCHAR* ConfigLastPaletteKey = TEXT("LastPalette");
	const TCHAR* ConfigGroupByCategoryKey = TEXT("GroupByCategory");
	const TCHAR* ConfigClickToPlaceKey = TEXT("ClickToPlace");
	const TCHAR* ConfigFadeWhenNotHoveredKey = TEXT("FadeWhenNotHovered");
	const TCHAR* ConfigFadedOpacityKey = TEXT("FadedOpacity");
	// Matches the Place Actors category sidebar (FCategoryDrivenContentBuilderBase, unlabelled).
	const FName SidebarToolbarStyle("CategoryDrivenContentBuilderToolbarWithoutLabels");
	constexpr float SidebarWidth = 44.f;
	constexpr float MinTileSize = 48.f;
	constexpr float MaxTileSize = 192.f;
	constexpr float SectionGap = 6.f; // between category sections; the reorder line is drawn in it
	constexpr float FadePollPeriod = 0.1f;
	// STabDrawer::OnPaint colours its background and shadow with FSlateBrush::GetTint alone, which drops the render
	// opacity that fades its content (SBorder multiplies by InWidgetStyle's tint; the drawer does not). So while
	// faded, these two style brushes get their alpha lowered directly, and are restored afterwards.
	const FName DrawerBrushNames[] = { TEXT("Docking.Sidebar.DrawerBackground"), TEXT("Docking.Sidebar.DrawerShadow") };

	void SetWindowOpacity(SWindow& Window, float Opacity)
	{
		if (!Window.GetNativeWindow().IsValid() || Window.GetOpacity() == Opacity)
		{
			return;
		}
#if PLATFORM_WINDOWS
		// Tab windows are created without WS_EX_LAYERED, which FWindowsWindow::SetOpacity's
		// SetLayeredWindowAttributes needs; a layered window stays hidden until that first call.
		// Style edits only take effect after SWP_FRAMECHANGED (FWindowsWindow does the same).
		const HWND Hwnd = static_cast<HWND>(Window.GetNativeWindow()->GetOSWindowHandle());
		const LONG_PTR ExStyle = GetWindowLongPtr(Hwnd, GWL_EXSTYLE);
		if (!(ExStyle & WS_EX_LAYERED))
		{
			SetWindowLongPtr(Hwnd, GWL_EXSTYLE, ExStyle | WS_EX_LAYERED);
			SetWindowPos(Hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
			UE_LOG(LogSlate, Log, TEXT("KhaosPalette: enabled per-window alpha on '%s'"), *Window.GetTitle().ToString());
		}
#endif
		Window.SetOpacity(Opacity);
	}

	bool IsPaletteAsset(const FAssetData& AssetData)
	{
		return AssetData.AssetClassPath == UKhaosPalette::StaticClass()->GetClassPathName();
	}

	TArray<FAssetData> DraggedAssets(TSharedPtr<FDragDropOperation> Op)
	{
		return Op.IsValid() && Op->IsOfType<FAssetDragDropOp>() ? StaticCastSharedPtr<FAssetDragDropOp>(Op)->GetAssets() : TArray<FAssetData>();
	}

	// Palettes themselves are never entries.
	TArray<FSoftObjectPath> DroppablePaths(TArrayView<FAssetData> Assets)
	{
		TArray<FSoftObjectPath> Paths;
		for (const FAssetData& AssetData : Assets)
		{
			if (!AssetData.IsInstanceOf(UKhaosPalette::StaticClass()))
			{
				Paths.Add(AssetData.GetSoftObjectPath());
			}
		}
		return Paths;
	}
}

void SKhaosPaletteTab::Construct(const FArguments& InArgs)
{
	ThumbnailPool = UThumbnailManager::Get().GetSharedThumbnailPool();
	GConfig->GetBool(ConfigSection, ConfigGroupByCategoryKey, bGroupByCategory, GEditorPerProjectIni);
	GConfig->GetBool(ConfigSection, ConfigClickToPlaceKey, bClickToPlace, GEditorPerProjectIni);
	GConfig->GetBool(ConfigSection, ConfigFadeWhenNotHoveredKey, bFadeWhenNotHovered, GEditorPerProjectIni);
	GConfig->GetFloat(ConfigSection, ConfigFadedOpacityKey, FadedOpacity, GEditorPerProjectIni);

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SKhaosPaletteTab::OnObjectPropertyChanged);
	TransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddSP(this, &SKhaosPaletteTab::OnObjectTransacted);
	DirtyStateHandle = UPackage::PackageDirtyStateChangedEvent.AddSP(this, &SKhaosPaletteTab::OnPackageDirtyStateChanged);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetsAddedHandle = AssetRegistry.OnAssetsAdded().AddSP(this, &SKhaosPaletteTab::OnAssetsAddedOrRemoved);
	AssetsRemovedHandle = AssetRegistry.OnAssetsRemoved().AddSP(this, &SKhaosPaletteTab::OnAssetsAddedOrRemoved);
	AssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddSP(this, &SKhaosPaletteTab::OnAssetRenamed);
	FilesLoadedHandle = AssetRegistry.OnFilesLoaded().AddSP(this, &SKhaosPaletteTab::RequestSidebarRefresh);

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeSidebar()
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeHeader()
			]

			// Grid; the whole area is a drop target for assets dragged from the Content Browser. When grouped, the
			// sections give the feedback, so this one stays silent (a panel-wide tint would mark all of them).
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
				.Padding(4.f)
				[
					SNew(SDropTarget)
					.bOnlyRecognizeOnDragEnter(true)
					.VerticalImage(FStyleDefaults::GetNoBrush())
					.HorizontalImage(FStyleDefaults::GetNoBrush())
					.BackgroundImage_Lambda([this]() { return bGroupByCategory ? FStyleDefaults::GetNoBrush() : FAppStyle::GetBrush("DropTarget.Background"); })
					.OnIsRecognized_Lambda([](TSharedPtr<FDragDropOperation> Op) { return Op.IsValid() && Op->IsOfType<FAssetDragDropOp>(); })
					.OnAllowDrop_Lambda([this](TSharedPtr<FDragDropOperation> Op) { return AreAssetsAcceptableForDrop(DraggedAssets(Op)); })
					.OnDropped_Lambda([this](const FGeometry&, const FDragDropEvent& Event)
					{
						OnAssetsDropped(Event, DraggedAssets(Event.GetOperation()));
						return FReply::Handled();
					})
					[
						SNew(SOverlay)

						+ SOverlay::Slot()
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(GridBox, SBox)
							]
						]

						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Padding(24.f)
						[
							SNew(SBox)
							.MaxDesiredWidth(360.f)
							[
								SNew(STextBlock)
								.Visibility(this, &SKhaosPaletteTab::GetEmptyHintVisibility)
								.Text(this, &SKhaosPaletteTab::GetEmptyHintText)
								.Justification(ETextJustify::Center)
								.AutoWrapText(true)
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							]
						]
					]
				]
			]
		]
	];

	FString LastPalettePath;
	if (GConfig->GetString(ConfigSection, ConfigLastPaletteKey, LastPalettePath, GEditorPerProjectIni) && !LastPalettePath.IsEmpty())
	{
		SetPalette(LoadObject<UKhaosPalette>(nullptr, *LastPalettePath));
	}
	RebuildSidebar();
	SetFadeWhenNotHovered(bFadeWhenNotHovered);
}

SKhaosPaletteTab::~SKhaosPaletteTab()
{
	RestoreFaded();
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	FCoreUObjectDelegates::OnObjectTransacted.Remove(TransactedHandle);
	UPackage::PackageDirtyStateChangedEvent.Remove(DirtyStateHandle);
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->OnAssetsAdded().Remove(AssetsAddedHandle);
		AssetRegistry->OnAssetsRemoved().Remove(AssetsRemovedHandle);
		AssetRegistry->OnAssetRenamed().Remove(AssetRenamedHandle);
		AssetRegistry->OnFilesLoaded().Remove(FilesLoadedHandle);
	}
}

void SKhaosPaletteTab::SetPalette(UKhaosPalette* InPalette)
{
	Palette = InPalette;
	PaletteIconName = InPalette ? FKhaosPaletteStyle::Get().GetSidebarIconName(InPalette) : NAME_None;
	GConfig->SetString(ConfigSection, ConfigLastPaletteKey, InPalette ? *InPalette->GetPathName() : TEXT(""), GEditorPerProjectIni);
	RebuildItems();
}

FReply SKhaosPaletteTab::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		RemoveSelected();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::F2)
	{
		RenameSelected();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SKhaosPaletteTab::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Reaches here only for clicks nothing else handled, i.e. empty grid space.
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !SelectedItems.IsEmpty())
	{
		SelectedItems.Reset();
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
	}
	return FReply::Unhandled();
}

// ---------------------------------------------------------------------------------------------------------------------
// Sidebar

TSharedRef<SWidget> SKhaosPaletteTab::MakeSidebar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Brushes.Recessed"))
		.Padding(0.f)
		[
			SNew(SBox)
			.WidthOverride(SidebarWidth)
			[
				SNew(SScrollBox)
				.ScrollBarVisibility(EVisibility::Collapsed)
				+ SScrollBox::Slot()
				[
					SAssignNew(SidebarBox, SBox)
				]
			]
		];
}

TSharedRef<SWidget> SKhaosPaletteTab::MakePaletteToolbar()
{
	FVerticalToolBarBuilder Toolbar(nullptr, FMultiBoxCustomization::None);
	Toolbar.SetStyle(&FAppStyle::Get(), SidebarToolbarStyle);
	Toolbar.SetLabelVisibility(EVisibility::Collapsed);

	FKhaosPaletteStyle& Style = FKhaosPaletteStyle::Get();
	for (const FAssetData& PaletteAsset : KhaosPaletteEditor::FindAllPalettes())
	{
		UKhaosPalette* SidebarPalette = Cast<UKhaosPalette>(PaletteAsset.GetAsset());
		if (!SidebarPalette)
		{
			continue;
		}
		const TWeakObjectPtr<UKhaosPalette> WeakPalette = SidebarPalette;
		const FText Name = SidebarPalette->GetDisplayName();
		const FText ToolTip = SidebarPalette->Description.IsEmpty()
			? Name
			: FText::Format(LOCTEXT("SidebarTooltip", "{0}\n{1}"), Name, SidebarPalette->Description);

		Toolbar.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateLambda([this, WeakPalette]()
				{
					if (UKhaosPalette* Picked = WeakPalette.Get())
					{
						SetPalette(Picked);
					}
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, WeakPalette]() { return Palette.IsValid() && Palette == WeakPalette; })),
			NAME_None,
			Name,
			ToolTip,
			// The status-overlay slot (used elsewhere for source control badges) carries the unsaved-changes asterisk.
			FSlateIcon(Style.GetStyleSetName(), Style.GetSidebarIconName(SidebarPalette), NAME_None,
				SidebarPalette->GetPackage()->IsDirty() ? FName("KhaosPalette.Sidebar.Modified") : NAME_None),
			EUserInterfaceActionType::ToggleButton);
	}

	// "+" sits right under the last palette. Same toggle style as the palettes so it looks identical; it never reads as checked.
	Toolbar.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateSP(this, &SKhaosPaletteTab::CreateNewPalette),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([]() { return false; })),
		NAME_None,
		LOCTEXT("NewPalette", "New Palette"),
		LOCTEXT("NewPaletteTooltip", "Create a new, empty palette asset."),
		FSlateIcon(Style.GetStyleSetName(), "KhaosPalette.Sidebar.Add"),
		EUserInterfaceActionType::ToggleButton);

	return Toolbar.MakeWidget();
}

void SKhaosPaletteTab::RequestSidebarRefresh()
{
	// Registry events arrive mid-scan; defer so palettes load and the toolbar rebuilds once per frame.
	if (bSidebarRefreshPending)
	{
		return;
	}
	bSidebarRefreshPending = true;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSPLambda(this, [this](double, float)
	{
		RebuildSidebar();
		return EActiveTimerReturnType::Stop;
	}));
}

void SKhaosPaletteTab::RebuildSidebar()
{
	bSidebarRefreshPending = false;
	if (SidebarBox.IsValid())
	{
		SidebarBox->SetContent(MakePaletteToolbar());
	}
	if (!Palette.IsValid())
	{
		const TArray<FAssetData> Palettes = KhaosPaletteEditor::FindAllPalettes();
		if (!Palettes.IsEmpty())
		{
			SetPalette(Cast<UKhaosPalette>(Palettes[0].GetAsset()));
		}
	}
}

void SKhaosPaletteTab::OnAssetsAddedOrRemoved(TConstArrayView<FAssetData> Assets)
{
	for (const FAssetData& AssetData : Assets)
	{
		if (IsPaletteAsset(AssetData))
		{
			RequestSidebarRefresh();
			return;
		}
	}
}

void SKhaosPaletteTab::OnAssetRenamed(const FAssetData& AssetData, const FString& OldPath)
{
	if (IsPaletteAsset(AssetData))
	{
		RequestSidebarRefresh();
	}
}

void SKhaosPaletteTab::CreateNewPalette()
{
	KhaosPaletteEditor::CreatePalette({});
}

// ---------------------------------------------------------------------------------------------------------------------
// Header

TSharedRef<SWidget> SKhaosPaletteTab::MakeHeader()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.f, 6.f))
		[
			SNew(SVerticalBox)

			// Palette icon, name, description and asset actions.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride(20.f)
					.HeightOverride(20.f)
					[
						SNew(SImage)
						.Image(this, &SKhaosPaletteTab::GetPaletteIcon)
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SKhaosPaletteTab::GetPaletteName)
						.Font(FAppStyle::GetFontStyle("BoldFont"))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SKhaosPaletteTab::GetPaletteDescription)
						.Visibility_Lambda([this]() { return GetPaletteDescription().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					MakeIconButton("Icons.Edit",
						LOCTEXT("EditPaletteTooltip", "Edit the palette: entries, labels, categories, display name, icon and colour."),
						FOnClicked::CreateLambda([this]() { EditPaletteProperties(); return FReply::Handled(); }))
				]

				// Only shown while the palette has unsaved changes.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ContentPadding(FMargin(4.f, 2.f))
					.ToolTipText(LOCTEXT("SavePaletteTooltip", "Save the palette asset."))
					.Visibility_Lambda([this]() { return IsPaletteDirty() ? EVisibility::Visible : EVisibility::Collapsed; })
					.OnClicked_Lambda([this]() { SavePalette(); return FReply::Handled(); })
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Save"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					MakeIconButton("SystemWideCommands.FindInContentBrowser",
						LOCTEXT("BrowseToPaletteTooltip", "Find the palette asset in the Content Browser."),
						FOnClicked::CreateLambda([this]() { BrowseToPalette(); return FReply::Handled(); }))
				]
			]

			// Search, grouping and tile size.
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(SSearchBox)
					.HintText(LOCTEXT("SearchHint", "Search palette"))
					.OnTextChanged(this, &SKhaosPaletteTab::OnSearchTextChanged)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.Padding(FMargin(4.f, 3.f))
					.ToolTipText(LOCTEXT("GroupByCategoryTooltip", "Group tiles by category. Drop assets or tiles onto a section to put them in that category."))
					.IsChecked_Lambda([this]() { return bGroupByCategory ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetGroupByCategory(State == ECheckBoxState::Checked); })
					[
						SNew(SImage)
						.Image(FKhaosPaletteStyle::Get().GetBrush("KhaosPalette.GroupByCategory"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride(90.f)
					.ToolTipText(LOCTEXT("TileSizeTooltip", "Tile size"))
					[
						SNew(SSlider)
						.MinValue(MinTileSize)
						.MaxValue(MaxTileSize)
						.Value_Lambda([this]() { return TileSize; })
						.OnValueChanged_Lambda([this](float NewValue) { TileSize = NewValue; })
						.OnMouseCaptureEnd_Lambda([this]() { RebuildGrid(); })
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SComboButton)
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.HasDownArrow(false)
					.ToolTipText(LOCTEXT("SettingsTooltip", "Placement and window options."))
					.OnGetMenuContent(this, &SKhaosPaletteTab::MakeSettingsMenu)
					.ButtonContent()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Settings"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
		];
}

TSharedRef<SWidget> SKhaosPaletteTab::MakeIconButton(const FName BrushName, const FText& ToolTip, FOnClicked OnClicked)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(4.f, 2.f))
		.ToolTipText(ToolTip)
		.OnClicked(OnClicked)
		.IsEnabled_Lambda([this]() { return Palette.IsValid(); })
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush(BrushName))
			.ColorAndOpacity(FSlateColor::UseForeground())
		];
}

FText SKhaosPaletteTab::GetPaletteName() const
{
	if (!Palette.IsValid())
	{
		return LOCTEXT("NoPalette", "No palette selected");
	}
	return IsPaletteDirty() ? FText::Format(LOCTEXT("DirtyPaletteName", "{0}*"), Palette->GetDisplayName()) : Palette->GetDisplayName();
}

FText SKhaosPaletteTab::GetPaletteDescription() const
{
	return Palette.IsValid() ? Palette->Description : FText::GetEmpty();
}

const FSlateBrush* SKhaosPaletteTab::GetPaletteIcon() const
{
	return FKhaosPaletteStyle::Get().GetBrush(PaletteIconName.IsNone() ? FName("KhaosPalette.Sidebar.Palette") : PaletteIconName);
}

void SKhaosPaletteTab::OnSearchTextChanged(const FText& InText)
{
	SearchText = InText;
	RefreshFilteredItems();
}

void SKhaosPaletteTab::SetGroupByCategory(bool bEnabled)
{
	bGroupByCategory = bEnabled;
	GConfig->SetBool(ConfigSection, ConfigGroupByCategoryKey, bGroupByCategory, GEditorPerProjectIni);
	RebuildGrid();
}

TSharedRef<SWidget> SKhaosPaletteTab::MakeSettingsMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/ false, nullptr);

	MenuBuilder.BeginSection("Placement", LOCTEXT("PlacementSection", "Placement"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ClickToPlace", "Click to Place"),
		LOCTEXT("ClickToPlaceTooltip", "Clicking a tile picks it up: it follows the cursor until the next click in a viewport places it. Escape cancels. Dragging still works."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				bClickToPlace = !bClickToPlace;
				GConfig->SetBool(ConfigSection, ConfigClickToPlaceKey, bClickToPlace, GEditorPerProjectIni);
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]() { return bClickToPlace; })),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Fade", LOCTEXT("FadeSection", "Fade"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("FadeWhenNotHovered", "Fade When Not Hovered"),
		LOCTEXT("FadeWhenNotHoveredTooltip", "While the tab is pinned open in the sidebar or floats in its own window, make it semi-transparent whenever the cursor is outside it."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { SetFadeWhenNotHovered(!bFadeWhenNotHovered); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this]() { return bFadeWhenNotHovered; })),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddWidget(
		SNew(SBox)
		.WidthOverride(120.f)
		.Padding(FMargin(8.f, 0.f))
		[
			SNew(SSlider)
			.MinValue(0.1f)
			.MaxValue(0.9f)
			.Value_Lambda([this]() { return FadedOpacity; })
			.OnValueChanged_Lambda([this](float NewValue) { FadedOpacity = NewValue; })
			.OnMouseCaptureEnd_Lambda([this]() { GConfig->SetFloat(ConfigSection, ConfigFadedOpacityKey, FadedOpacity, GEditorPerProjectIni); })
		],
		LOCTEXT("FadedOpacity", "Faded Opacity"));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SKhaosPaletteTab::SetFadeWhenNotHovered(bool bEnabled)
{
	bFadeWhenNotHovered = bEnabled;
	GConfig->SetBool(ConfigSection, ConfigFadeWhenNotHoveredKey, bFadeWhenNotHovered, GEditorPerProjectIni);
	if (bEnabled && !FadeTimer.IsValid())
	{
		FadeTimer = RegisterActiveTimer(FadePollPeriod, FWidgetActiveTimerDelegate::CreateSP(this, &SKhaosPaletteTab::UpdateFade));
	}
	else if (!bEnabled && FadeTimer.IsValid())
	{
		UnRegisterActiveTimer(FadeTimer.ToSharedRef());
		FadeTimer.Reset();
		RestoreFaded();
	}
}

EActiveTimerReturnType SKhaosPaletteTab::UpdateFade(double InCurrentTime, float InDeltaTime)
{
	FSlateApplication& Slate = FSlateApplication::Get();

	// Pinned open in the sidebar, the tab sits in an STabDrawer overlaid on the dock area (STabSidebar), so fading
	// that widget's render opacity reveals the viewport under it. Otherwise fade the window if it is not the main one.
	TSharedPtr<SWidget> Drawer = GetParentWidget();
	while (Drawer.IsValid() && Drawer->GetType() != TEXT("STabDrawer"))
	{
		Drawer = Drawer->GetParentWidget();
	}
	TSharedPtr<SWindow> Window = Drawer.IsValid() ? nullptr : Slate.FindWidgetWindow(AsShared());
	if (Window == FGlobalTabmanager::Get()->GetRootWindow())
	{
		Window.Reset();
	}
	if (FadedDrawer.Pin() != Drawer || FadedWindow.Pin() != Window)
	{
		RestoreFaded();
	}

	// Cursor position rather than Slate hover state: hover is not updated while a drag from a tile is in flight.
	// Menus and popups opened from the tab are separate windows, so they count as hovered.
	const FVector2D CursorPos = Slate.GetCursorPos();
	if (Drawer.IsValid())
	{
		const bool bHovered = Drawer->GetTickSpaceGeometry().GetLayoutBoundingRect().ContainsPoint(CursorPos) || Slate.AnyMenusVisible();
		Drawer->SetRenderOpacity(bHovered ? 1.f : FadedOpacity);
		if (bHovered)
		{
			RestoreDrawerBrushes();
		}
		else
		{
			SetDrawerBrushAlpha(FadedOpacity);
		}
		FadedDrawer = Drawer;
	}
	else if (Window.IsValid())
	{
		const bool bHovered = Window->GetRectInScreen().ContainsPoint(CursorPos) || Slate.AnyMenusVisible();
		SetWindowOpacity(*Window, bHovered ? 1.f : FadedOpacity);
		FadedWindow = Window;
	}
	return EActiveTimerReturnType::Continue;
}

void SKhaosPaletteTab::RestoreFaded()
{
	if (const TSharedPtr<SWidget> Drawer = FadedDrawer.Pin())
	{
		Drawer->SetRenderOpacity(1.f);
	}
	RestoreDrawerBrushes();
	if (const TSharedPtr<SWindow> Window = FadedWindow.Pin())
	{
		SetWindowOpacity(*Window, 1.f);
	}
	FadedDrawer.Reset();
	FadedWindow.Reset();
}

void SKhaosPaletteTab::SetDrawerBrushAlpha(float Alpha)
{
	for (const FName& Name : DrawerBrushNames)
	{
		FSlateBrush* Brush = const_cast<FSlateBrush*>(FAppStyle::GetBrush(Name));
		const FSlateColor& Original = DrawerBrushTints.FindOrAdd(Name, Brush->TintColor);
		FLinearColor Color = Original.GetSpecifiedColor();
		Color.A *= Alpha;
		Brush->TintColor = FSlateColor(Color);
	}
}

void SKhaosPaletteTab::RestoreDrawerBrushes()
{
	for (const TPair<FName, FSlateColor>& Pair : DrawerBrushTints)
	{
		const_cast<FSlateBrush*>(FAppStyle::GetBrush(Pair.Key))->TintColor = Pair.Value;
	}
	DrawerBrushTints.Reset();
}

// ---------------------------------------------------------------------------------------------------------------------
// Grid

void SKhaosPaletteTab::RebuildItems()
{
	AllItems.Reset();
	SelectedItems.Reset();
	Categories.Reset();

	if (const UKhaosPalette* CurrentPalette = Palette.Get())
	{
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		const uint32 ThumbnailResolution = static_cast<uint32>(MaxTileSize);

		for (const FKhaosPaletteEntry& Entry : CurrentPalette->Entries)
		{
			const FSoftObjectPath AssetPath = Entry.Asset.ToSoftObjectPath();
			if (AssetPath.IsNull())
			{
				continue;
			}
			FItemPtr Item = MakeShared<FKhaosPaletteTileItem>();
			Item->AssetData = AssetRegistry.GetAssetByObjectPath(AssetPath);
			Item->Category = Entry.Category;
			if (Item->AssetData.IsValid())
			{
				Item->Label = Entry.DisplayName.IsEmpty() ? FText::FromName(Item->AssetData.AssetName) : Entry.DisplayName;
				Item->Thumbnail = MakeShared<FAssetThumbnail>(Item->AssetData, ThumbnailResolution, ThumbnailResolution, ThumbnailPool);
			}
			else
			{
				// Keep missing entries visible so the artist can see and remove them; the path is the only identity left.
				Item->AssetData = FAssetData(AssetPath.GetLongPackageName(), AssetPath.GetAssetPathString(), FTopLevelAssetPath());
				Item->Label = FText::Format(LOCTEXT("MissingAsset", "{0} (missing)"), FText::FromString(AssetPath.GetAssetName()));
			}
			AllItems.Add(MoveTemp(Item));
		}
		Categories = CurrentPalette->GetCategoryOrder();
	}

	RefreshFilteredItems();
}

void SKhaosPaletteTab::RefreshFilteredItems()
{
	FilteredItems.Reset();
	const FString Search = SearchText.ToString();
	for (const FItemPtr& Item : AllItems)
	{
		if (!Search.IsEmpty() && !Item->Label.ToString().Contains(Search) && !Item->AssetData.AssetName.ToString().Contains(Search))
		{
			continue;
		}
		FilteredItems.Add(Item);
	}
	RebuildGrid();
}

void SKhaosPaletteTab::RebuildGrid()
{
	if (!GridBox.IsValid())
	{
		return;
	}
	if (!bGroupByCategory)
	{
		GridBox->SetContent(MakeTileWrap(FilteredItems));
		return;
	}

	// One collapsible section per category (empty ones included, so they can be dragged into); uncategorised last.
	TMap<FName, TArray<FItemPtr>> Groups;
	for (const FName& Category : Categories)
	{
		Groups.Add(Category);
	}
	for (const FItemPtr& Item : FilteredItems)
	{
		Groups.FindOrAdd(Item->Category).Add(Item);
	}
	TArray<FName> Order = Categories;
	Order.Add(NAME_None);

	TSharedRef<SVerticalBox> Sections = SNew(SVerticalBox);
	for (const FName& Category : Order)
	{
		if (const TArray<FItemPtr>* Items = Groups.Find(Category))
		{
			Sections->AddSlot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, SectionGap)
			[
				MakeCategorySection(Category, *Items)
			];
		}
	}

	Sections->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Left)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ToolTipText(LOCTEXT("AddCategoryTooltip", "Add an empty category to drag entries into. Drag headers to reorder; right-click one for icon, rename and remove."))
		.IsEnabled_Lambda([this]() { return Palette.IsValid(); })
		.OnClicked_Lambda([this]() { AddCategory(); return FReply::Handled(); })
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Plus"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("AddCategory", "Add Category"))
			]
		]
	];
	GridBox->SetContent(Sections);
}

TSharedRef<SWidget> SKhaosPaletteTab::MakeCategorySection(FName Category, const TArray<FItemPtr>& Items)
{
	const FText Title = Category.IsNone() ? LOCTEXT("Uncategorised", "Uncategorised") : FText::FromName(Category);
	const FKhaosPaletteCategory* CategoryData = Palette.IsValid() ? Palette->FindCategory(Category) : nullptr;

	// The header toggles the section, drags to reorder and takes other headers; the body is thicker than the
	// Place Actors default so the grip and icon fit. Set the area on the header afterwards, since it wraps it.
	TSharedRef<SKhaosPaletteCategoryHeader> Header = SNew(SKhaosPaletteCategoryHeader)
		.Category(Category)
		.Title(Title)
		.Count(Items.Num())
		.Icon(CategoryData ? CategoryData->Icon.LoadSynchronous() : nullptr)
		.bReorderable(!Category.IsNone());

	const FString Key = SectionKey(Category);
	const bool* SessionCollapsed = SectionCollapsed.Find(Key);
	const bool bInitiallyCollapsed = SessionCollapsed ? *SessionCollapsed : (CategoryData && CategoryData->bCollapsedByDefault);
	TSharedRef<SExpandableArea> Area = SNew(SExpandableArea)
		.Style(&FKhaosPaletteStyle::Get().GetWidgetStyle<FExpandableAreaStyle>("KhaosPalette.CategoryArea"))
		.AreaTitlePadding(FMargin(0.f))
		.InitiallyCollapsed(bInitiallyCollapsed)
		.OnAreaExpansionChanged_Lambda([this, Key](bool bExpanded) { SectionCollapsed.Add(Key, !bExpanded); })
		.BorderImage(FAppStyle::GetBrush("PaletteToolBar.ExpandableAreaHeader"))
		.BodyBorderImage(FAppStyle::GetBrush("PaletteToolBar.ExpandableAreaBody"))
		.BodyBorderBackgroundColor(FLinearColor::White)
		.HeaderPadding(FMargin(8.f, 6.f))
		.HeaderContent()
		[
			Header
		]
		.BodyContent()
		[
			MakeTileWrap(Items)
		];
	Header->SetOwnerArea(Area);

	// Each section is its own drop target so assets (or this tab's tiles) can be dropped straight into a category.
	// A rejected drop bubbles to the grid-wide target, which only adds new assets. Right-click outside a tile
	// opens the category menu. The outer wrapper takes dragged headers for reordering.
	return SNew(SKhaosPaletteCategorySection)
		.Category(Category)
		.GapBelow(SectionGap)
		.OnCategoryDropped(FOnKhaosPaletteCategoryDropped::CreateSP(this, &SKhaosPaletteTab::OnCategoryDropped))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBrush"))
			.Padding(0.f)
			.OnMouseButtonUp(this, &SKhaosPaletteTab::OnCategorySectionMouseUp, Category)
			[
				SNew(SAssetDropTarget)
				.bSupportsMultiDrop(true)
				.bOnlyRecognizeOnDragEnter(true)
				.OnAreAssetsAcceptableForDrop(this, &SKhaosPaletteTab::AreAssetsAcceptableForCategoryDrop, Category)
				.OnAssetsDropped(this, &SKhaosPaletteTab::OnAssetsDroppedOnCategory, Category)
				[
					Area
				]
			]
		];
}

FString SKhaosPaletteTab::SectionKey(FName Category) const
{
	return FString::Printf(TEXT("%s|%s"), Palette.IsValid() ? *Palette->GetPathName() : TEXT(""), *Category.ToString());
}

TSharedRef<SWidget> SKhaosPaletteTab::MakeTileWrap(const TArray<FItemPtr>& Items)
{
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true);
	for (const FItemPtr& Item : Items)
	{
		Wrap->AddSlot()
		[
			MakeTile(Item)
		];
	}
	return Wrap;
}

TSharedRef<SWidget> SKhaosPaletteTab::MakeTile(FItemPtr Item)
{
	return SNew(SKhaosPaletteTile)
		.Item(Item)
		.TileSize(TileSize)
		.IsSelected_Lambda([this, Item]() { return SelectedItems.Contains(Item); })
		.OnClicked(FOnKhaosPaletteTileEvent::CreateSP(this, &SKhaosPaletteTab::OnTileClicked))
		.OnReleased(FOnKhaosPaletteTileEvent::CreateSP(this, &SKhaosPaletteTab::OnTileReleased))
		.OnDoubleClicked(FOnKhaosPaletteTileEvent::CreateSP(this, &SKhaosPaletteTab::OnTileDoubleClicked))
		.OnContextMenu(FOnKhaosPaletteTileEvent::CreateSP(this, &SKhaosPaletteTab::OnTileContextMenu))
		.OnDragRequested(FOnKhaosPaletteTileEvent::CreateSP(this, &SKhaosPaletteTab::OnTileDragRequested));
}

TSharedRef<SWidget> SKhaosPaletteTab::MakeTileContextMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/ true, nullptr);
	MenuBuilder.BeginSection("KhaosPaletteItem", LOCTEXT("ItemSection", "Palette Item"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("RenameEntry", "Rename..."),
		LOCTEXT("RenameEntryTooltip", "Change the label shown under the tile (F2). Leave it as the asset name to clear the override."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(
			FExecuteAction::CreateSP(this, &SKhaosPaletteTab::RenameSelected),
			FCanExecuteAction::CreateLambda([this]() { return SelectedItems.Num() == 1; })));
	MenuBuilder.AddSubMenu(
		LOCTEXT("SetCategory", "Category"),
		LOCTEXT("SetCategoryTooltip", "Move the selected entries to a category."),
		FNewMenuDelegate::CreateSP(this, &SKhaosPaletteTab::FillCategoryMenu),
		/*bInOpenSubMenuOnClick*/ false,
		FSlateIcon(FKhaosPaletteStyle::Get().GetStyleSetName(), "KhaosPalette.GroupByCategory"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("OpenAsset", "Edit Asset"),
		LOCTEXT("OpenAssetTooltip", "Open the selected assets in their editors."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(FExecuteAction::CreateSP(this, &SKhaosPaletteTab::OpenSelected)));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("BrowseToAsset", "Browse to Asset"),
		LOCTEXT("BrowseToAssetTooltip", "Select the assets in the Content Browser."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SystemWideCommands.FindInContentBrowser"),
		FUIAction(FExecuteAction::CreateSP(this, &SKhaosPaletteTab::BrowseToSelected)));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("RemoveFromPalette", "Remove from Palette"),
		LOCTEXT("RemoveFromPaletteTooltip", "Remove the selected entries from this palette (Delete)."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
		FUIAction(FExecuteAction::CreateSP(this, &SKhaosPaletteTab::RemoveSelected)));
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

FReply SKhaosPaletteTab::OnTileClicked(FItemPtr Item, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsControlDown())
	{
		if (!SelectedItems.Remove(Item))
		{
			SelectedItems.Add(Item);
		}
	}
	else if (!SelectedItems.Contains(Item))
	{
		SelectedItems.Reset();
		SelectedItems.Add(Item);
	}
	return FReply::Handled();
}

FReply SKhaosPaletteTab::OnTileDoubleClicked(FItemPtr Item, const FPointerEvent& MouseEvent)
{
	if (Item.IsValid() && Item->Thumbnail.IsValid())
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Item->AssetData.GetSoftObjectPath());
	}
	return FReply::Handled();
}

FReply SKhaosPaletteTab::OnTileContextMenu(FItemPtr Item, const FPointerEvent& MouseEvent)
{
	if (!SelectedItems.Contains(Item))
	{
		SelectedItems.Reset();
		SelectedItems.Add(Item);
	}
	FSlateApplication::Get().PushMenu(AsShared(), FWidgetPath(), MakeTileContextMenu(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	return FReply::Handled();
}

FReply SKhaosPaletteTab::OnTileReleased(FItemPtr Item, const FPointerEvent& MouseEvent)
{
	// Click to place: begin the same asset drag on release. With no button held, Slate keeps routing it as a
	// drag until the next button release drops it (FSlateApplication::RoutePointerUpEvent) or Escape cancels.
	// Modifier clicks only edit the selection.
	if (!bClickToPlace || MouseEvent.IsControlDown() || MouseEvent.IsShiftDown() || FSlateApplication::Get().IsDragDropping())
	{
		return FReply::Unhandled();
	}
	return OnTileDragRequested(Item, MouseEvent);
}

FReply SKhaosPaletteTab::OnTileDragRequested(FItemPtr Item, const FPointerEvent& MouseEvent)
{
	const TArray<FItemPtr> Items = SelectedItems.Contains(Item) ? SelectedItems.Array() : TArray<FItemPtr>{ Item };
	TArray<FAssetData> Assets;
	for (const FItemPtr& DraggedItem : Items)
	{
		if (DraggedItem->Thumbnail.IsValid())
		{
			Assets.Add(DraggedItem->AssetData);
		}
	}
	if (Assets.IsEmpty())
	{
		return FReply::Unhandled();
	}
	return FReply::Handled().BeginDragDrop(FAssetDragDropOp::New(MoveTemp(Assets)));
}

EVisibility SKhaosPaletteTab::GetEmptyHintVisibility() const
{
	const bool bShowingSections = bGroupByCategory && !Categories.IsEmpty();
	return FilteredItems.IsEmpty() && !bShowingSections ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

FText SKhaosPaletteTab::GetEmptyHintText() const
{
	if (!Palette.IsValid())
	{
		return LOCTEXT("NoPaletteHint", "No palettes yet. Press + in the sidebar to create one, or right-click assets in the Content Browser and choose Add to Palette.");
	}
	if (AllItems.IsEmpty())
	{
		return LOCTEXT("EmptyPaletteHint", "This palette is empty. Drop assets from the Content Browser here, or right-click actors in the level and choose Add to Palette.");
	}
	return LOCTEXT("NoMatchHint", "No entries match the current search.");
}

bool SKhaosPaletteTab::AreAssetsAcceptableForDrop(TArrayView<FAssetData> Assets) const
{
	const UKhaosPalette* CurrentPalette = Palette.Get();
	if (!CurrentPalette)
	{
		return false;
	}
	for (const FAssetData& AssetData : Assets)
	{
		if (!AssetData.IsInstanceOf(UKhaosPalette::StaticClass()) && !CurrentPalette->ContainsAsset(AssetData.GetSoftObjectPath()))
		{
			return true;
		}
	}
	return false;
}

void SKhaosPaletteTab::OnAssetsDropped(const FDragDropEvent& DragDropEvent, TArrayView<FAssetData> Assets)
{
	UKhaosPalette* CurrentPalette = Palette.Get();
	if (!CurrentPalette)
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("AddToPaletteTransaction", "Add to Palette"));
	if (CurrentPalette->AddAssets(DroppablePaths(Assets)) > 0)
	{
		RebuildItems();
	}
}

bool SKhaosPaletteTab::AreAssetsAcceptableForCategoryDrop(TArrayView<FAssetData> Assets, FName Category) const
{
	const UKhaosPalette* CurrentPalette = Palette.Get();
	if (!CurrentPalette)
	{
		return false;
	}
	for (const FAssetData& AssetData : Assets)
	{
		if (AssetData.IsInstanceOf(UKhaosPalette::StaticClass()))
		{
			continue;
		}
		const FKhaosPaletteEntry* Entry = CurrentPalette->FindEntry(AssetData.GetSoftObjectPath());
		if (!Entry || Entry->Category != Category)
		{
			return true;
		}
	}
	return false;
}

void SKhaosPaletteTab::OnAssetsDroppedOnCategory(const FDragDropEvent& DragDropEvent, TArrayView<FAssetData> Assets, FName Category)
{
	UKhaosPalette* CurrentPalette = Palette.Get();
	if (!CurrentPalette)
	{
		return;
	}
	const TArray<FSoftObjectPath> Paths = DroppablePaths(Assets);
	const FScopedTransaction Transaction(LOCTEXT("AddToCategoryTransaction", "Add to Category"));
	if (CurrentPalette->SetCategory(Paths, Category) + CurrentPalette->AddAssets(Paths, Category) > 0)
	{
		RebuildItems();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Categories and renaming

void SKhaosPaletteTab::PromptForName(const FText& Label, const FText& Default, TFunction<void(const FString&)> OnCommitted)
{
	const TSharedRef<STextEntryPopup> Popup = SNew(STextEntryPopup)
		.Label(Label)
		.DefaultText(Default)
		.SelectAllTextWhenFocused(true)
		.ClearKeyboardFocusOnCommit(false)
		.OnTextCommitted_Lambda([WeakThis = TWeakPtr<SWidget>(AsShared()), OnCommitted](const FText& Text, ETextCommit::Type CommitType)
		{
			if (CommitType == ETextCommit::OnEnter)
			{
				FSlateApplication::Get().DismissAllMenus();
				if (WeakThis.IsValid())
				{
					OnCommitted(Text.ToString().TrimStartAndEnd());
				}
			}
		});
	FSlateApplication::Get().PushMenu(AsShared(), FWidgetPath(), Popup, FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup));
}

void SKhaosPaletteTab::FillCategoryMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewCategory", "New Category..."),
		LOCTEXT("NewCategoryTooltip", "Move the selected entries to a new category."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			PromptForName(LOCTEXT("NewCategoryLabel", "New Category"), FText::GetEmpty(), [this](const FString& Name)
			{
				if (!Name.IsEmpty())
				{
					SetSelectedCategory(FName(*Name));
				}
			});
		})));
	MenuBuilder.AddSeparator();

	auto AddCategoryEntry = [this, &MenuBuilder](FName Category, const FText& Label)
	{
		MenuBuilder.AddMenuEntry(
			Label,
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SKhaosPaletteTab::SetSelectedCategory, Category),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, Category]()
				{
					return !SelectedItems.IsEmpty() && Algo::AllOf(SelectedItems, [Category](const FItemPtr& Item) { return Item->Category == Category; });
				})),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	};
	AddCategoryEntry(NAME_None, LOCTEXT("NoCategory", "None"));
	for (const FName& Category : Categories)
	{
		AddCategoryEntry(Category, FText::FromName(Category));
	}
}

void SKhaosPaletteTab::SetSelectedCategory(FName Category)
{
	UKhaosPalette* CurrentPalette = Palette.Get();
	if (!CurrentPalette || SelectedItems.IsEmpty())
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const FItemPtr& Item : SelectedItems)
	{
		Paths.Add(Item->AssetData.GetSoftObjectPath());
	}
	const FScopedTransaction Transaction(LOCTEXT("SetCategoryTransaction", "Set Category"));
	const bool bAdded = CurrentPalette->AddCategory(Category);
	if (CurrentPalette->SetCategory(Paths, Category) > 0 || bAdded)
	{
		RebuildItems();
	}
}

void SKhaosPaletteTab::RenameSelected()
{
	if (!Palette.IsValid() || SelectedItems.Num() != 1)
	{
		return;
	}
	const FItemPtr Item = *SelectedItems.CreateConstIterator();
	const FString AssetName = Item->AssetData.AssetName.ToString();
	PromptForName(LOCTEXT("RenameEntryLabel", "Rename Entry"), Item->Label, [this, Item, AssetName](const FString& Name)
	{
		if (UKhaosPalette* CurrentPalette = Palette.Get())
		{
			// Typing the asset name back (or nothing) clears the override rather than storing a copy of it.
			const FText NewLabel = (Name.IsEmpty() || Name == AssetName) ? FText::GetEmpty() : FText::FromString(Name);
			const FScopedTransaction Transaction(LOCTEXT("RenameEntryTransaction", "Rename Palette Entry"));
			if (CurrentPalette->SetDisplayName(Item->AssetData.GetSoftObjectPath(), NewLabel))
			{
				RebuildItems();
			}
		}
	});
}

void SKhaosPaletteTab::AddCategory()
{
	PromptForName(LOCTEXT("NewCategoryLabel", "New Category"), FText::GetEmpty(), [this](const FString& Name)
	{
		if (UKhaosPalette* CurrentPalette = Palette.Get())
		{
			const FScopedTransaction Transaction(LOCTEXT("AddCategoryTransaction", "Add Category"));
			if (CurrentPalette->AddCategory(FName(*Name)))
			{
				RebuildItems();
			}
		}
	});
}

FReply SKhaosPaletteTab::OnCategorySectionMouseUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FName Category)
{
	if (MouseEvent.GetEffectingButton() != EKeys::RightMouseButton || Category.IsNone())
	{
		return FReply::Unhandled();
	}
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/ true, nullptr);
	MenuBuilder.BeginSection("KhaosPaletteCategory", FText::FromName(Category));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("RenameCategory", "Rename Category..."),
		FText::GetEmpty(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(FExecuteAction::CreateSP(this, &SKhaosPaletteTab::RenameCategory, Category)));
	MenuBuilder.AddSubMenu(
		LOCTEXT("CategoryIcon", "Icon"),
		LOCTEXT("CategoryIconTooltip", "Texture shown in the section header."),
		FNewMenuDelegate::CreateLambda([this, Category](FMenuBuilder& SubMenu)
		{
			const FKhaosPaletteCategory* CategoryData = Palette.IsValid() ? Palette->FindCategory(Category) : nullptr;
			const FAssetData Current = CategoryData && !CategoryData->Icon.IsNull()
				? IAssetRegistry::Get()->GetAssetByObjectPath(CategoryData->Icon.ToSoftObjectPath())
				: FAssetData();
			const TArray<const UClass*> AllowedClasses{ UTexture2D::StaticClass() };
			SubMenu.AddWidget(
				PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
					Current,
					/*AllowClear*/ true,
					AllowedClasses,
					TArray<UFactory*>(),
					FOnShouldFilterAsset(),
					FOnAssetSelected::CreateSP(this, &SKhaosPaletteTab::SetCategoryIcon, Category),
					FSimpleDelegate::CreateLambda([]() { FSlateApplication::Get().DismissAllMenus(); })),
				FText::GetEmpty(),
				/*bNoIndent*/ true);
		}),
		/*bInOpenSubMenuOnClick*/ false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("OpenByDefault", "Open by Default"),
		LOCTEXT("OpenByDefaultTooltip", "Whether this section starts expanded when the palette is shown. Saved in the palette."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SKhaosPaletteTab::ToggleCategoryOpenByDefault, Category),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &SKhaosPaletteTab::IsCategoryOpenByDefault, Category)),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("RemoveCategory", "Remove Category"),
		LOCTEXT("RemoveCategoryTooltip", "Entries stay in the palette and become uncategorised."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
		FUIAction(FExecuteAction::CreateSP(this, &SKhaosPaletteTab::RemoveCategory, Category)));
	MenuBuilder.EndSection();
	FSlateApplication::Get().PushMenu(AsShared(), FWidgetPath(), MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	return FReply::Handled();
}

void SKhaosPaletteTab::RenameCategory(FName Category)
{
	PromptForName(LOCTEXT("RenameCategoryLabel", "Rename Category"), FText::FromName(Category), [this, Category](const FString& Name)
	{
		if (UKhaosPalette* CurrentPalette = Palette.Get())
		{
			const FScopedTransaction Transaction(LOCTEXT("RenameCategoryTransaction", "Rename Category"));
			if (CurrentPalette->RenameCategory(Category, FName(*Name)))
			{
				bool bCollapsed = false;
				if (SectionCollapsed.RemoveAndCopyValue(SectionKey(Category), bCollapsed))
				{
					SectionCollapsed.Add(SectionKey(FName(*Name)), bCollapsed);
				}
				RebuildItems();
			}
		}
	});
}

void SKhaosPaletteTab::RemoveCategory(FName Category)
{
	if (UKhaosPalette* CurrentPalette = Palette.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveCategoryTransaction", "Remove Category"));
		if (CurrentPalette->RemoveCategory(Category))
		{
			RebuildItems();
		}
	}
}

void SKhaosPaletteTab::SetCategoryIcon(const FAssetData& Asset, FName Category)
{
	FSlateApplication::Get().DismissAllMenus();
	if (UKhaosPalette* CurrentPalette = Palette.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("SetCategoryIconTransaction", "Set Category Icon"));
		if (CurrentPalette->SetCategoryIcon(Category, TSoftObjectPtr<UTexture2D>(Asset.GetSoftObjectPath())))
		{
			RebuildItems();
		}
	}
}

bool SKhaosPaletteTab::IsCategoryOpenByDefault(FName Category) const
{
	const FKhaosPaletteCategory* CategoryData = Palette.IsValid() ? Palette->FindCategory(Category) : nullptr;
	return !CategoryData || !CategoryData->bCollapsedByDefault;
}

void SKhaosPaletteTab::ToggleCategoryOpenByDefault(FName Category)
{
	if (UKhaosPalette* CurrentPalette = Palette.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("OpenByDefaultTransaction", "Set Category Open by Default"));
		if (CurrentPalette->SetCategoryCollapsedByDefault(Category, IsCategoryOpenByDefault(Category)))
		{
			// Drop the session override so the section shows the new default straight away.
			SectionCollapsed.Remove(SectionKey(Category));
			RebuildItems();
		}
	}
}

void SKhaosPaletteTab::OnCategoryDropped(FName Dragged, FName Target, bool bAfter)
{
	UKhaosPalette* CurrentPalette = Palette.Get();
	if (!CurrentPalette || Dragged == Target)
	{
		return;
	}
	TArray<FName> Order = Categories;
	Order.Remove(Dragged);
	int32 NewIndex = Order.Num();
	if (!Target.IsNone())
	{
		NewIndex = Order.IndexOfByKey(Target);
		if (NewIndex == INDEX_NONE)
		{
			return;
		}
		NewIndex += bAfter ? 1 : 0;
	}

	const FScopedTransaction Transaction(LOCTEXT("MoveCategoryTransaction", "Move Category"));
	// Register any entry-only categories in their shown order first so list indices match what is on screen.
	for (const FName& Category : Categories)
	{
		CurrentPalette->AddCategory(Category);
	}
	if (CurrentPalette->MoveCategory(Dragged, NewIndex))
	{
		RebuildItems();
	}
}

void SKhaosPaletteTab::RemoveSelected()
{
	UKhaosPalette* CurrentPalette = Palette.Get();
	if (!CurrentPalette || SelectedItems.IsEmpty())
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("RemoveFromPaletteTransaction", "Remove from Palette"));
	for (const FItemPtr& Item : SelectedItems)
	{
		CurrentPalette->RemoveAsset(Item->AssetData.GetSoftObjectPath());
	}
	RebuildItems();
}

void SKhaosPaletteTab::BrowseToSelected()
{
	TArray<FAssetData> Assets;
	for (const FItemPtr& Item : SelectedItems)
	{
		if (Item->Thumbnail.IsValid())
		{
			Assets.Add(Item->AssetData);
		}
	}
	if (!Assets.IsEmpty())
	{
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().SyncBrowserToAssets(Assets);
	}
}

void SKhaosPaletteTab::OpenSelected()
{
	for (const FItemPtr& Item : SelectedItems.Array())
	{
		OnTileDoubleClicked(Item, FPointerEvent());
	}
}

void SKhaosPaletteTab::EditPaletteProperties()
{
	if (UKhaosPalette* CurrentPalette = Palette.Get())
	{
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateFloatingDetailsView({ CurrentPalette }, /*bIsLockable*/ false);
	}
}

void SKhaosPaletteTab::BrowseToPalette()
{
	if (UKhaosPalette* CurrentPalette = Palette.Get())
	{
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().SyncBrowserToAssets(TArray<UObject*>{ CurrentPalette });
	}
}

bool SKhaosPaletteTab::IsPaletteDirty() const
{
	return Palette.IsValid() && Palette->GetPackage()->IsDirty();
}

void SKhaosPaletteTab::SavePalette()
{
	if (UKhaosPalette* CurrentPalette = Palette.Get())
	{
		FEditorFileUtils::PromptForCheckoutAndSave({ CurrentPalette->GetPackage() }, /*bCheckDirty*/ true, /*bPromptToSave*/ false);
	}
}

void SKhaosPaletteTab::OnPackageDirtyStateChanged(UPackage* Package)
{
	// Sidebar badges are baked into the toolbar, so any palette's save/dirty flip rebuilds it.
	if (Package && Cast<UKhaosPalette>(Package->FindAssetInPackage()))
	{
		RequestSidebarRefresh();
	}
}

void SKhaosPaletteTab::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (!Object || !Object->IsA<UKhaosPalette>())
	{
		return;
	}
	// Name, icon or colour edits on any palette change the sidebar; entry edits on the shown one change the grid.
	RequestSidebarRefresh();
	if (Object == Palette.Get())
	{
		PaletteIconName = FKhaosPaletteStyle::Get().GetSidebarIconName(Palette.Get());
		RebuildItems();
	}
}

void SKhaosPaletteTab::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	if (Object && Object == Palette.Get())
	{
		RebuildItems();
	}
}

#undef LOCTEXT_NAMESPACE
