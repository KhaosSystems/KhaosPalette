// Copyright Khaos Games ApS. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "KhaosPaletteFactory.generated.h"

UCLASS(HideCategories = Object)
class UKhaosPaletteFactory : public UFactory
{
	GENERATED_BODY()

public:
	UKhaosPaletteFactory();

	// Seeded into the new palette; set by "New Palette from Selection".
	TArray<FSoftObjectPath> InitialAssets;

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
