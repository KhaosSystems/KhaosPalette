# Marketing

The Fab listing for Khaos Palette. Nothing here ships in the plugin.

- [Listing.md](Listing.md): the listing text to paste into Fab.
- [Images/](Images/): the ten 1920×1080 listing images to upload.
- [Shots/](Shots/): the editor screenshots the images are built from, and how to retake them.
- [render.py](render.py): builds `Images/` from `Shots/` (needs Chrome or Edge). [capture.py](capture.py) does the
  scripted part of retaking the shots through the editor's MCP server.
- [Brand/](Brand/): the Khaos Systems logo.

When the tab changes visibly, retake the shots and run `python render.py`. A missing shot renders as a stand-in
and that image is written as `.draft.png`, which git ignores; upload only images without `.draft`.
