# Khaos Palette: Fab listing

## Title

Khaos Palette

## Short description

Your most-used props in one tab beside the viewport. Click a tile, then click in the level. Palettes are assets the whole team shares.

## Description

Set dressing means reaching for the same few dozen props again and again, and digging through the Content Browser
for each one. Khaos Palette keeps them in one tab beside the viewport, sorted the way you work, one click from the
level.

**Click or drag to place.** Click a tile and the prop follows the cursor until you click in the level; Escape
cancels. Or drag tiles in like Content Browser assets, with the same surface snapping, several at once with
Ctrl-click.

**Sorted into categories.** Collapsible categories you name, reorder by dragging, give icons and choose which start
open. Drop assets or tiles on a category to file them there.

**A palette per biome.** Every palette in the project sits in the tab's sidebar, each with its own name,
description, icon and colour. Search the palette and resize the tiles to taste.

**Out of your way.** Dock the tab anywhere, pin it to the editor's sidebar as a drawer, or float it. Turn on Fade
When Not Hovered and it goes see-through while you work in the viewport.

**Add from anywhere.** Drop assets on the tab, use Add to Palette on assets in the Content Browser or actors in the
level, or make a new palette from a selection.

**Shared as assets.** Palettes are plain data assets in your project: in source control, shared by the whole team,
edited like any other asset.

### Features

- Dockable tab with every palette in the project in a sidebar.
- Click to Place: click a tile, then click in any level viewport. Escape cancels.
- Drag tiles into the viewport with Content Browser snapping; Ctrl-click to drag several.
- Places Blueprint actors, static and skeletal meshes, Niagara systems, sounds and decal materials.
- Collapsible categories: create, rename, reorder, set icons, choose which start open.
- Search, adjustable tile size, and tile names you can change without renaming the asset.
- Fade When Not Hovered, with adjustable opacity, for a pinned drawer or a floating tab.
- Add to Palette from the Content Browser and the level; New Palette from Selection.
- Each palette has a name, description, sidebar icon and colour.
- Unsaved palettes are marked, with a Save button in the tab.
- Editor-only: the runtime module holds just the palette data asset, so nothing else ships in your game.

## Technical details

- **Code modules:**
  - `KhaosPaletteRuntime` (Runtime): the palette data asset
  - `KhaosPaletteEditor` (Editor): the tab, asset definition, factory and menu extensions
- **Number of Blueprints:** 0
- **Number of C++ classes:** 3 UObject classes and 2 structs, plus editor widgets
- **Network replicated:** No (editor tool)
- **Supported development platforms:** Windows
- **Supported target build platforms:** Windows, Mac, Linux, Android, iOS (the runtime module is a data asset only)
- **Supported engine versions:** 5.8
- **Documentation:** https://github.com/KhaosSystems/KhaosPalette/blob/main/Content/Docs/KhaosPalette.md
- **Example project:** none
- **Important notes:** Editor tool. Create a palette with Content Browser > Add > Khaos > Khaos Palette, then open
  the tab with the Palette button on the level editor toolbar or from Tools > Khaos Palette.

## Version notes (1.0.0)

First release: palettes of placeable assets in a dockable tab, with categories, search, Click to Place and
drag-and-drop.

## Tags

level design, set dressing, editor tool, placement, palette, workflow, productivity, environment art, tools, prop placement
