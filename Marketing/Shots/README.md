# Screenshots

`render.py` builds the listing images from these PNGs. They are taken in a real project, in the editor at 100 %
scale, with the sample palettes in `palettes.json` (the project's own props, no third-party packs).

`capture.py` does the scripted part through the editor's MCP server (see its docstring for what it needs):

```
python capture.py palettes   # before the tab shots
python capture.py scene
python capture.py tile
python capture.py cleanup    # afterwards
```

The rest are screenshots of the tab: as a window grab where it is a single window, as a screen grab where a
menu or the viewport is in shot. No project name, level name, personal path or tooltip showing a path in shot.

| File | Shows | Used in |
| --- | --- | --- |
| `scene.png` | `capture.py scene`: the level from the camera in `palettes.json`, editor icons hidden | cover, overview, drag, click, palettes |
| `tile.png` | `capture.py tile`: the thumbnail of the asset being dragged | drag, click |
| `tab.png` | The Dock palette in a floating tab, 600×640, default tile size | cover, overview, click, palettes |
| `tab-menu.png` | The same, right-click on the Dock category, Rename Category highlighted | categories |
| `tab-large.png` | The same tab at 760×900 with the tile size slider at its maximum | drag |
| `drawer.png` | 1920×1080 crop of the editor: the tab docked to the sidebar (right-click its tab > Dock to Sidebar), its drawer open over the viewport, the cursor on it | drawer |
| `fade.png` | The same with Fade When Not Hovered on, Faded Opacity 0.8, the cursor in the viewport | fade |
| `fill.png` | Content Browser: right-click a prop > Add to Palette, New Palette from Selection highlighted | fill |
| `asset.png` | The palette's properties (the tab's Edit button), Categories expanded, 640×400 | asset |

After retaking `tab.png`, measure the callout points and `FIRST_TILE` at the top of `render.py` on it. After
retaking `scene.png`, check the click image's `target` still lands on an empty spot of the dock.
