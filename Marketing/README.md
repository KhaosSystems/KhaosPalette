# Marketing

The Fab listing for Khaos Palette. Nothing here ships in the plugin.

- [Listing.md](Listing.md): the listing text to paste into Fab.
- [Shots/](Shots/): the editor screenshots, and the list of what to take.
- [Images/](Images/): the 1920×1080 listing images to upload. Rebuild them with `python render.py` (needs Chrome or Edge).
- [Brand/](Brand/): the Khaos Systems logo.

Until every screenshot exists, `render.py` fills the gaps with stand-ins and writes those images as `.draft.png`,
which git ignores. Upload only the images without `.draft`.
