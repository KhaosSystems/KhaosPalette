// Copyright Khaos Games ApS. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"

class AActor;
class UToolMenu;

namespace KhaosPaletteEditor
{
	// Every palette asset known to the asset registry, sorted by name.
	TArray<FAssetData> FindAllPalettes();

	// Source assets that recreate the given actors when dropped: the blueprint for BP actors, the mesh for StaticMeshActors.
	TArray<FSoftObjectPath> GatherPlaceableAssets(const TArray<AActor*>& Actors);

	// Opens the new-asset dialog for a palette seeded with the given assets.
	void CreatePalette(TArray<FSoftObjectPath> InitialAssets);

	// Fills a submenu with one "add to" entry per existing palette plus "New Palette from Selection...".
	void BuildAddToPaletteMenu(UToolMenu* Menu, TArray<FSoftObjectPath> Assets);
}
