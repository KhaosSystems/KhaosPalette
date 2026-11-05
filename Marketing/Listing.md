# Khaos Palette: Fab listing

## Title

Khaos Palette

## Short description

Palettes of the props you place most, one click away in a dockable tab. Drag or click a tile to dress your level.

## Description

Set dressing means reaching for the same few dozen assets again and again, and digging through the Content
Browser for each one. Khaos Palette keeps them at hand. A palette is an asset that lists the things you place
(Blueprint actors, meshes, Niagara systems, sounds, decals), and a dockable tab shows it as a grid of
thumbnails beside the viewport. Make one per biome, per level or per artist, and switch between them from the
sidebar.

Placing works exactly like dragging from the Content Browser, with surface snapping and multi-select, or turn on
Click to Place: click a tile, then click in the level. Organise a palette into collapsible categories you can
rename, give icons, reorder and fill by dropping assets onto them. Pin the tab in the sidebar and it fades out
while you work in the viewport.

Palettes are plain data assets in your project, so they live in source control and the whole team shares them.

## Features

- A dockable tab with every palette in the project in a sidebar, Place Actors style. Double-click a palette to open it.
- Drag a tile into the viewport, with the same snapping and multi-select as a Content Browser drag.
- Click to Place: click a tile and it follows the cursor until you click in a viewport. Escape cancels.
- Places Blueprint actors, static and skeletal meshes, Niagara systems, sounds and decal materials.
- Collapsible categories: create, rename, reorder by dragging, set an icon, choose which start open, and drop assets or tiles onto them.
- Search box and adjustable tile size.
- Fill a palette by dropping assets on the tab, from the Content Browser's right-click menu, or from actors in the level. New Palette from Selection makes one in a click.
- Fade When Not Hovered: pinned in the sidebar or floating, the panel turns see-through while the cursor is in the viewport.
- Each palette has its own name, description, sidebar icon and colour.
- Unsaved palettes are marked, with a Save button in the tab.
- Editor tooling only: the runtime module holds nothing but the palette data asset, so nothing else ships in your game.

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
- **Important notes:** Editor tool. Palettes are data assets in your project; create one with Content Browser > Add > Khaos > Khaos Palette and open the tab from Tools > Khaos Palette.

## Version notes (1.0.0)

First release: palettes of placeable assets in a dockable tab, with categories, search, drag-and-drop and click-to-place.

## Tags

level design, set dressing, editor tool, placement, palette, workflow, productivity, environment art, tools, prop placement
