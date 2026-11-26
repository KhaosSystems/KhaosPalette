# Khaos Palette: Fab listing

## Title

Khaos Palette

## Short description

The props you place most, one click from the level.

## Description

Set dressing means reaching for the same props again and again. Khaos Palette keeps them in one tab beside the viewport, sorted the way you work.

Click a prop, then click where it goes. A live preview shows exactly where it will land, snapped to your grid and settled onto the surface, so you place it right the first time. Turn it, scale it and keep going, prop after prop, without leaving the viewport.

Pin the tab to the side and it fades out of the way while you build. Palettes are plain assets, so the whole team dresses levels from the same set.

Editor-only. Nothing ships in your game.

## FAQ

**What can go in a palette?**
Anything you can drag into the viewport from the Content Browser: Blueprint actors, meshes, Niagara systems, sounds, decal materials.

**Do I have to use Click to Place?**
No. Dragging works too, with the usual snapping. Escape cancels a click placement.

**Does placing respect my snapping settings?**
Yes. The preview uses the viewport's grid, rotation and scale snapping, and surface snapping and alignment, so the actor lands exactly where the preview shows.

**Is placing undoable?**
Yes. Each placement is one undo step, the same as dropping the asset from the Content Browser.

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
- **Documentation:** https://docs.khaos.systems/unreal/khaos-palette/
- **Important notes:** Create a palette with Content Browser > Add > Khaos > Khaos Palette. Open it with the Palette button on the toolbar.

## Version notes

Paste the [CHANGELOG.md](../CHANGELOG.md) sections of every release since the version on Fab.

## Tags

level design, set dressing, editor tool, placement, palette, workflow, environment art, prop placement, snapping
