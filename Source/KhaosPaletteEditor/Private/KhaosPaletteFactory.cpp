// Copyright Khaos Systems. All Rights Reserved.

#include "KhaosPaletteFactory.h"

#include "KhaosPalette.h"

UKhaosPaletteFactory::UKhaosPaletteFactory()
{
	SupportedClass = UKhaosPalette::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UKhaosPaletteFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UKhaosPalette* Palette = NewObject<UKhaosPalette>(InParent, InClass, InName, Flags);
	Palette->AddAssets(InitialAssets);
	return Palette;
}
