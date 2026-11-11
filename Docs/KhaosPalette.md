# Khaos Palette

Artist-authored palettes of placeable assets, shown in a dockable tab for drag-and-drop set dressing.
A palette is a `UKhaosPalette` data asset holding a list of assets (blueprint actors, static/skeletal meshes,
Niagara systems, sounds, decal materials...). Typical use: one palette per biome.

## Using it

- **Open the tab**: Tools > Khaos Palette, or double-click a palette asset. The sidebar lists every palette in the
  project, Place Actors style; the `+` under the last one creates a new palette.
- **Place things**: drag a tile into the level viewport. Tiles start a normal asset drag, so the viewport places
  them exactly as a Content Browser drag would (surface snapping, multi-select, etc.). With **Click to Place** on
  (gear menu in the header, on by default) a plain click picks the tile up instead: the asset follows the cursor
  until the next click in a viewport places it, and Escape cancels. Ctrl/Shift-click only changes the selection.
- **See through it**: pin the tab open in the sidebar (or drag it out into its own window) and turn on
  **Fade When Not Hovered** in the gear menu; the panel goes semi-transparent (Faded Opacity slider) whenever the
  cursor is outside it, so the viewport shows through. Has no effect while the tab is docked in a regular
  layout, where nothing is drawn behind it.
- **Create a palette**: Content Browser > Add > Khaos > Khaos Palette, or right-click assets > Add to Palette >
  New Palette from Selection.
- **Fill a palette**: drop assets from the Content Browser onto the tab, right-click assets > Add to Palette,
  or right-click actors in the level > Add to Palette (resolves to their blueprint / mesh).
- **Organise**: right-click tiles for Rename (F2) and a Category submenu (pick one, None, or New Category).
  The group toggle shows one collapsible section per category; drop Content Browser assets or the tab's own tiles
  onto a section to put them in that category, drag a section header to reorder it, right-click a header for
  Icon / Rename / Open by Default / Remove, and Add Category at the bottom makes an empty section to drag into.
  Categories (order, icon, default open state) are saved in the palette. The search box filters the grid. The Edit button in the header opens the
  palette's properties for its display name, description, sidebar icon and colour.
- **Remove entries**: select tiles and press Delete, or right-click > Remove from Palette.
- **Save**: a palette with unsaved changes shows `*` after its name, an asterisk badge on its sidebar icon, and a
  Save button in the header.

## Modules

- `KhaosPaletteRuntime`: the data asset only, so cooked content can reference a palette without breaking.
- `KhaosPaletteEditor`: tab, asset definition, factory and context-menu extensions. Nothing here ships.
