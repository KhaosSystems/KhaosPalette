// Copyright Khaos Games ApS. All Rights Reserved.

#include "KhaosPaletteStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Engine/Texture2D.h"
#include "Interfaces/IPluginManager.h"
#include "KhaosPalette.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateStyleRegistry.h"

TUniquePtr<FKhaosPaletteStyle> FKhaosPaletteStyle::Instance;

FKhaosPaletteStyle& FKhaosPaletteStyle::Get()
{
	if (!Instance.IsValid())
	{
		Instance = TUniquePtr<FKhaosPaletteStyle>(new FKhaosPaletteStyle());
	}
	return *Instance;
}

void FKhaosPaletteStyle::Shutdown()
{
	Instance.Reset();
}

FKhaosPaletteStyle::FKhaosPaletteStyle()
	: FSlateStyleSet(TEXT("KhaosPaletteStyle"))
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("KhaosPalette"));
	SetContentRoot(FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Resources")));

	Set("KhaosPalette.TabIcon", new IMAGE_BRUSH_SVG("Icons/PaletteClass", CoreStyleConstants::Icon16x16));
	Set("ClassIcon.KhaosPalette", new IMAGE_BRUSH_SVG("Icons/PaletteClass", CoreStyleConstants::Icon16x16));
	Set("ClassThumbnail.KhaosPalette", new IMAGE_BRUSH_SVG("Icons/PaletteClass", CoreStyleConstants::Icon64x64));
	Set("KhaosPalette.Sidebar.Palette", new IMAGE_BRUSH_SVG("Icons/PaletteSidebar", CoreStyleConstants::Icon20x20));
	Set("KhaosPalette.Sidebar.Add", new IMAGE_BRUSH_SVG("Icons/Plus", CoreStyleConstants::Icon20x20));
	// Asterisk badge layered over a sidebar icon whose palette has unsaved changes.
	Set("KhaosPalette.Sidebar.Modified", new IMAGE_BRUSH_SVG("Icons/Modified", CoreStyleConstants::Icon20x20));
	Set("KhaosPalette.GroupByCategory", new IMAGE_BRUSH_SVG("Icons/GroupByCategory", CoreStyleConstants::Icon16x16));

	// Category sections draw their own arrow at the end of the header; blank the area's leading one.
	FExpandableAreaStyle CategoryArea = FAppStyle::GetWidgetStyle<FExpandableAreaStyle>("ExpandableArea");
	CategoryArea.SetCollapsedImage(FSlateNoResource()).SetExpandedImage(FSlateNoResource());
	Set("KhaosPalette.CategoryArea", CategoryArea);

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FKhaosPaletteStyle::~FKhaosPaletteStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

FName FKhaosPaletteStyle::GetSidebarIconName(UKhaosPalette* Palette)
{
	const FName BrushName(*FString::Printf(TEXT("KhaosPalette.Sidebar.%s"), *Palette->GetPathName()));
	UTexture2D* Texture = Palette->Icon.LoadSynchronous();

	if (FSidebarIcon* Existing = SidebarIcons.Find(BrushName))
	{
		if (Existing->Texture.Get() == Texture)
		{
			if (!Texture)
			{
				Existing->Brush->TintColor = Palette->Color;
			}
			return BrushName;
		}
	}

	// A kind change (texture <-> tinted glyph) needs a new brush. Set() replaces without freeing the old
	// one, which may still be referenced by a live widget this frame; that leak is tiny and rare.
	FSlateBrush* Brush = Texture
		? static_cast<FSlateBrush*>(new FSlateImageBrush(Texture, CoreStyleConstants::Icon20x20))
		: new FSlateVectorImageBrush(RootToContentDir(TEXT("Icons/PaletteSidebar"), TEXT(".svg")), CoreStyleConstants::Icon20x20, Palette->Color);
	Set(BrushName, Brush);
	SidebarIcons.Add(BrushName, { Brush, TStrongObjectPtr<UTexture2D>(Texture) });
	return BrushName;
}
