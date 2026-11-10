# Khaos Palette: Fab listing

## Title

Khaos Palette

## Short description

The props you place most, one click from the level.

## Description

Set dressing means reaching for the same props again and again. Khaos Palette keeps them in one tab beside the viewport, sorted the way you work.

Click a tile, then click where it goes. Pin the tab to the side and it fades out of the way while you build. Palettes are plain assets, so the whole team dresses levels from the same set.

Editor-only. Nothing ships in your game.

## FAQ

**What can go in a palette?**
Anything you can drag into the viewport from the Content Browser: Blueprint actors, meshes, Niagara systems, sounds, decal materials.

**Do I have to use Click to Place?**
No. Dragging works too, with the usual snapping. Escape cancels a click placement.

**Can the team share palettes?**
Yes. A palette is a data asset in your project, so it goes in source control like any other.

**Does it add anything to my game?**
No. The editor module does all the work; the runtime module only defines the palette asset, and nothing references it unless you do.

**Can I use my own icons and colours?**
Yes. Each palette has a sidebar icon and colour, and each category an icon.

**How do I open it?**
Create a palette with Content Browser > Add > Khaos > Khaos Palette, then press Palette on the level editor toolbar.

## Technical details

- **Code modules:** `KhaosPaletteRuntime` (Runtime, the palette data asset), `KhaosPaletteEditor` (Editor)
- **Number of Blueprints:** 0
- **Number of C++ classes:** 3 UObject classes, 2 structs
- **Network replicated:** No
- **Supported development platforms:** Windows
- **Supported target build platforms:** Windows, Mac, Linux, Android, iOS
- **Supported engine versions:** 5.8
- **Documentation:** https://github.com/KhaosSystems/KhaosPalette/blob/main/Content/Docs/KhaosPalette.md
- **Important notes:** Create a palette with Content Browser > Add > Khaos > Khaos Palette. Open it with the Palette button on the toolbar.

## Version notes (1.0.0)

First release.

## Tags

level design, set dressing, editor tool, placement, palette, workflow, environment art, prop placement
