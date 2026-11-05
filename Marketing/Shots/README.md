# Screenshots

`render.py` builds the listing images from these PNGs.

Before shooting:
- Editor in its default dark theme, with Application Scale at 2 (Editor Preferences > Appearance) so the UI stays
  sharp when enlarged.
- A dressed level with real props, so the tiles have real thumbnails.
- Nothing that names the project or a person: no title bar, project folders or user paths in shot.
- No FPS or stat overlays, no notifications.
- Crop tight to the feature. A whole editor window shrinks to unreadable in a listing image.

| File | Shows | Used in |
| --- | --- | --- |
| `scene.png` | The dressed level in Game View (G), no UI, at least 1920×1080 | cover, overview, drag, click, palettes |
| `tab.png` | The Khaos Palette tab on its own: two categories open, one folded, a tile selected, three palettes in the sidebar | cover, overview, drag, click, palettes |
| `tab-menu.png` | The same tab with a category header's right-click menu open | categories |
| `tile.png` | One tile, cropped square | drag |
| `fade.png` | The editor with the tab pinned and faded over the viewport, 16:9 | fade |
| `fill.png` | The Content Browser with assets selected and right-click > Add to Palette open | fill |
| `asset.png` | The palette's properties (the Edit button): name, description, icon and colour | asset |

After adding `tab.png`, measure the callout points and `TILE_ROW_EDGE` at the top of `render.py` on it. After
adding `scene.png`, set the click image's `target` to where the placed prop stands.
