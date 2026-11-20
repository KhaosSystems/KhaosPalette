# Khaos Palette

Artist-authored palettes of placeable assets, shown in a dockable tab for drag-and-drop set dressing.
A palette is a `UKhaosPalette` data asset holding a list of assets (blueprint actors, static/skeletal meshes,
Niagara systems, sounds, decal materials...). Typical use: one palette per biome.

## Using it

- **Open the tab**: Tools > Khaos Palette, or double-click a palette asset. The sidebar lists every palette in the
  project, Place Actors style; the `+` under the last one creates a new palette.
- **Place things**: drag a tile into the level viewport. Tiles start a normal asset drag, so the viewport places
  them exactly as a Content Browser drag would. With **Click to Place** on (gear menu in the header, on by
  default) a plain click picks the tile up instead: the next click in a viewport places it, Shift-click places it
  and keeps going (or turn on **Keep Placing** in the gear menu), and Escape or a click anywhere else stops. The
  preview hides while you move the camera, and where nothing is under the cursor it sits on the ground plane.
  Tab and Shift-Tab switch to the next and previous tile: the selected ones when the tile was clicked
  out of a multi-selection, otherwise the tiles of its category. **Random from Selection** (gear menu) switches to a random one after each placement. Shift-scroll
  rotates (by the rotation snap step when rotation snapping is on, else 15 degrees) and Ctrl-scroll scales (by the
  scale snap step, else 10%); both carry over to the next placement. The tile being placed is highlighted, a label
  beside the cursor shows what is being placed, and a bar at the bottom of the viewport lists these keys. Either way the preview snaps as the actor will: grid
  snapping, and surface snapping and alignment, from the viewport toolbar.
- **Select several**: as in the Content Browser. Ctrl-click adds or removes a tile, Shift-click selects everything
  from the last clicked tile to this one (Ctrl+Shift adds that range), Ctrl+A selects all. Ranges and Ctrl+A skip
  collapsed sections. Modifier clicks never pick a tile up, so this works with Click to Place on. Delete, the
  Category submenu and dragging a selected tile onto a category section act on the whole selection; placing in
  the level always places just the tile you drag or click.
- **Keep it open**: right-click the Khaos Palette tab and choose Dock to Sidebar, open it from the sidebar, then
  click the pin at the top of the drawer. It then stays open over the viewport.
- **See through it**: pin the tab open in the sidebar (or drag it out into its own window) and turn on
  **Fade When Not Hovered** in the gear menu; the panel goes semi-transparent (Faded Opacity slider) whenever the
  cursor is outside it, so the viewport shows through. Has no effect while the tab is docked in a regular
  layout, where nothing is drawn behind it.
- **Create a palette**: Content Browser > Add > Khaos > Khaos Palette, or right-click assets > Add to Palette >
  New Palette from Selection.
- **Fill a palette**: drop assets from the Content Browser onto the tab, right-click assets > Add to Palette,
  or right-click actors in the level > Add to Palette (resolves to their blueprint / mesh). Add to Palette is at
  the top of both menus. Click a palette to add the assets uncategorised, or hover it to pick a category, or type
  a new category name and press Enter; assets already in that palette move to the category picked.
- **Organise**: right-click tiles for Rename (F2) and a Category submenu (pick one, None, or New Category).
  The group toggle shows one collapsible section per category; drop Content Browser assets or the tab's own tiles
  onto a section to put them in that category, drag a section header to reorder it, right-click a header for
  Icon / Rename / Open by Default / Remove, and Add Category at the bottom makes an empty section to drag into.
  Categories (order, icon, default open state) are saved in the palette. The search box filters the grid. Tiles
  stretch to fill each row; the slider sets their smallest size. The Edit button in the header opens the
  palette's properties for its display name, description, sidebar icon and colour.
- **Remove entries**: select tiles and press Delete, or right-click > Remove from Palette.
- **Save**: a palette with unsaved changes shows `*` after its name, an asterisk badge on its sidebar icon, and a
  Save button in the header.

## Modules

- `KhaosPaletteRuntime`: the data asset only, so cooked content can reference a palette without breaking.
- `KhaosPaletteEditor`: tab, asset definition, factory and context-menu extensions. Nothing here ships.
