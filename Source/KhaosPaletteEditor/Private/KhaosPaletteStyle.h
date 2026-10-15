// Copyright Khaos Systems. All Rights Reserved.

#pragma once

#include "Engine/Texture2D.h"
#include "Styling/SlateStyle.h"
#include "UObject/StrongObjectPtr.h"

class UKhaosPalette;

// Plugin icons (tab, class icon/thumbnail) plus one sidebar brush per palette:
// the palette's own texture, or the built-in glyph tinted with its Color.
class FKhaosPaletteStyle : public FSlateStyleSet
{
public:
	static FKhaosPaletteStyle& Get();
	static void Shutdown();

	// Registers or refreshes the sidebar brush for a palette and returns its brush name.
	FName GetSidebarIconName(UKhaosPalette* Palette);

	virtual ~FKhaosPaletteStyle() override;

private:
	FKhaosPaletteStyle();

	struct FSidebarIcon
	{
		FSlateBrush* Brush = nullptr;
		TStrongObjectPtr<UTexture2D> Texture; // keeps the icon alive; the palette only holds it softly
	};
	TMap<FName, FSidebarIcon> SidebarIcons;

	static TUniquePtr<FKhaosPaletteStyle> Instance;
};
