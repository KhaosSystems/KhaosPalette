"""Build the Fab listing images (1920x1080 PNG) from the screenshots in Shots/ into Images/.

    python render.py

Needs Chrome or Edge, and network access for Google Fonts. A screenshot that isn't in Shots/ yet is drawn as a
stand-in (a mock of the tab, a grey box for the rest) and the image is written as .draft.png, which git ignores.
Upload only the images without .draft.

Layout rules: 96 px margins; headlines in Montserrat Black, three words or fewer; one accent per image, taken
from the dots of the plugin icon; flat shapes, no gradients.
"""

import base64
import html
import pathlib
import struct
import subprocess
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
ICONS = HERE.parent / "Resources" / "Icons"
SHOTS = HERE / "Shots"
OUT = HERE / "Images"
LOGO = (HERE / "Brand" / "KhaosSystems.svg").as_uri()
ICON = (ICONS / "PaletteClass.svg").as_uri()
BROWSER = next(p for p in [
    pathlib.Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe"),
    pathlib.Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
] if p.exists())

RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE = "#E5484D", "#F5A623", "#F2D94E", "#46B95F", "#3E8BFF", "#A05BEF"

# Points on the tab as fractions of its screenshot (x, y). They match the mock; measure them again on tab.png.
SEARCH, SIDEBAR, CATEGORY, TILE, FOLDED = (0.02, 0.085), (0.048, 0.213), (0.926, 0.149), (0.876, 0.25), (0.926, 0.778)
TILE_ROW_EDGE = (0.978, 0.25)

SLIDES = [
    {"id": "00-cover", "layout": "cover", "panel": "tab.png"},
    {"id": "01-overview", "layout": "callouts", "panel": "tab.png", "callouts": [
        ("Search", SEARCH, YELLOW, "left"), ("Every palette", SIDEBAR, BLUE, "left"),
        ("Categories", CATEGORY, GREEN, "right"), ("Drag or click", TILE, ORANGE, "right"),
        ("Fold away", FOLDED, PURPLE, "right")]},
    {"id": "02-drag", "layout": "close-up", "panel": "tab.png", "title": ("Drag", "it in"), "accent": RED},
    # target: where the tile is being placed in scene.png, as page x, y and ring radius (right half of the image).
    {"id": "03-click", "layout": "split", "panel": "tab.png", "title": ("Click.", "Place."), "accent": GREEN,
     "target": (1460, 760, 150)},
    {"id": "04-categories", "layout": "colour", "panel": "tab-menu.png", "title": ("Sort into", "categories"),
     "accent": ORANGE},
    {"id": "05-palettes", "layout": "large", "panel": "tab.png", "title": ("A palette", "per biome"), "accent": YELLOW},
    {"id": "06-drawer", "layout": "drawer", "panel": "drawer.png", "title": ("Pin it", "to the side"), "accent": BLUE},
    {"id": "07-fade", "layout": "drawer", "panel": "fade.png", "title": ("Fades while", "you work"), "accent": RED,
     "faded": True},
    {"id": "08-fill", "layout": "colour", "panel": "fill.png", "title": ("Add from", "anywhere"), "accent": PURPLE},
    {"id": "09-asset", "layout": "colour", "panel": "asset.png", "title": ("Shared", "as assets"), "accent": GREEN},
]


def svg_uri(name, colour):
    text = (ICONS / f"{name}.svg").read_text(encoding="utf-8").replace("#FFFFFF", colour)
    return "data:image/svg+xml;base64," + base64.b64encode(text.encode()).decode()


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    return struct.unpack(">II", head[16:24])


CSS = """
@import url('https://fonts.googleapis.com/css2?family=Montserrat:wght@700;900&family=Inter:wght@400;500;600&display=block');
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { width: 1920px; height: 1080px; overflow: hidden; }
body { position: relative; background: #111; color: #fff; font-family: 'Inter', sans-serif; -webkit-font-smoothing: antialiased; }
.display { font-family: 'Montserrat', sans-serif; font-weight: 900; text-transform: uppercase; letter-spacing: -0.01em; line-height: 0.92; }
.scene { position: absolute; top: 0; right: 0; bottom: 0; background-size: cover; background-position: center; }
.missing { background: #2B2F35; display: flex; align-items: center; justify-content: center; color: #6E747C;
  font-size: 28px; font-weight: 600; }
.shot { position: absolute; border-radius: 6px; box-shadow: 0 40px 100px rgba(0,0,0,0.6), 0 0 0 1px #000; }
.dim { position: absolute; inset: 0; background: rgba(8,10,14,0.55); }
.ks { position: absolute; height: 60px; filter: drop-shadow(0 2px 10px rgba(0,0,0,.6)); }
.label { position: absolute; left: 72px; bottom: 72px; background: #101014; padding: 30px 40px 34px 36px; border-radius: 6px;
  display: flex; gap: 28px; align-items: center; }
.label .chip { width: 72px; height: 72px; border-radius: 50%; flex: none; }
.label h2 { font-size: 62px; line-height: 1; }
.head { position: absolute; font-size: 112px; text-shadow: 0 6px 30px rgba(0,0,0,.45); }
em { font-style: normal; display: block; }

/* Mock of the palette tab, 540 px wide unscaled, for while tab.png doesn't exist. */
.tab { position: absolute; width: 540px; background: #1A1A1A; border-radius: 6px; overflow: hidden;
  transform-origin: 0 0; box-shadow: 0 40px 100px rgba(0,0,0,0.6), 0 0 0 1px #000; font-size: 15px; color: #C0C0C0; }
.tab .title { height: 40px; background: #0F0F0F; display: flex; align-items: flex-end; padding-left: 8px; }
.tab .title span { background: #1A1A1A; padding: 9px 14px; border-radius: 5px 5px 0 0; color: #EEE; display: flex;
  gap: 8px; align-items: center; font-weight: 500; }
.tab .title img { width: 16px; }
.tab .bar { height: 52px; display: flex; gap: 6px; align-items: center; padding: 0 12px; border-bottom: 1px solid #0F0F0F; }
.tab .search { flex: 1; height: 32px; background: #0F0F0F; border-radius: 4px; color: #6A6A6A; padding: 7px 12px; }
.tab .btn { width: 28px; height: 28px; display: flex; align-items: center; justify-content: center; }
.tab .btn img { width: 16px; }
.tab .body { display: flex; height: calc(100% - 92px); }
.tab .side { width: 52px; background: #141414; padding-top: 10px; display: flex; flex-direction: column; align-items: center; gap: 10px; }
.tab .side div { width: 36px; height: 36px; border-radius: 4px; display: flex; align-items: center; justify-content: center; }
.tab .side div.on { background: #2C2C2C; box-shadow: inset 2px 0 0 #3E8BFF; }
.tab .side img { width: 22px; }
.tab .main { width: 488px; padding: 0 12px; }
.tab .cat { background: #242424; color: #D8D8D8; font-weight: 600; padding: 0 10px; border-radius: 3px; margin: 8px 0;
  height: 32px; display: flex; align-items: center; gap: 8px; }
.tab .grid { display: grid; grid-template-columns: repeat(4, 110px); gap: 8px; }
.tile { text-align: center; font-size: 12px; color: #A8A8A8; }
.tile i { display: block; aspect-ratio: 1; border-radius: 4px; background: #2A2A2A url('ICON_URI') center / 34% no-repeat; margin-bottom: 4px; }
.tile.sel i { box-shadow: 0 0 0 2px #3E8BFF; }
.menu { position: absolute; left: 180px; top: 120px; width: 250px; background: #1F1F1F; border-radius: 4px; padding: 6px 0;
  color: #D0D0D0; box-shadow: 0 12px 30px rgba(0,0,0,.6), 0 0 0 1px #000; }
.menu div { padding: 8px 16px 8px 40px; position: relative; }
.menu div.hi { background: #3E8BFF; color: #fff; }
.menu div.check::before { content: '\\2713'; position: absolute; left: 16px; }
.menu hr { border: 0; border-top: 1px solid #333; margin: 5px 0; }
.ghost { position: absolute; } .ghost i { box-shadow: 0 0 0 4px var(--c); }
/* The editor's sidebar: docked tabs as a vertical strip, the open one as a drawer over the viewport. */
.strip { position: absolute; right: 0; top: 0; bottom: 0; width: 40px; background: #151515; box-shadow: -1px 0 0 #000;
  padding-top: 10px; display: flex; flex-direction: column; align-items: center; gap: 4px; font-size: 15px; color: #9A9A9A; }
.strip div { writing-mode: vertical-rl; padding: 14px 10px; display: flex; align-items: center; gap: 8px; }
.strip div.on { background: #262626; color: #fff; box-shadow: inset -2px 0 0 #3E8BFF; }
.strip img { width: 16px; }
.tab.drawer { border-radius: 0; box-shadow: -30px 0 80px rgba(0,0,0,0.55), 0 0 0 1px #000; }
.tab .dhead { height: 40px; background: #0F0F0F; display: flex; align-items: center; gap: 8px; padding: 0 12px; color: #EEE; font-weight: 500; }
.tab .dhead img { width: 16px; } .tab .dhead span { margin-left: auto; display: flex; }
"""

CSS = CSS.replace("ICON_URI", svg_uri("PaletteSidebar", "#3A3A3A"))

GEAR = ('<svg width="18" height="18" viewBox="0 0 24 24"><path fill="#C0C0C0" d="M19.4 13a7.5 7.5 0 0 0 0-2l2.1-1.6'
        '-2-3.5-2.5 1a7.6 7.6 0 0 0-1.7-1L15 3.3h-4l-.4 2.6a7.6 7.6 0 0 0-1.7 1l-2.5-1-2 3.5L6.5 11a7.5 7.5 0 0 0 0 2'
        'l-2.1 1.6 2 3.5 2.5-1a7.6 7.6 0 0 0 1.7 1l.4 2.6h4l.4-2.6a7.6 7.6 0 0 0 1.7-1l2.5 1 2-3.5zM13 15.5a3.5 3.5 0 '
        '1 1 0-7 3.5 3.5 0 0 1 0 7z"/></svg>')
SAVE = ('<svg width="18" height="18" viewBox="0 0 24 24"><path fill="#C0C0C0" d="M5 3h11l4 4v12a2 2 0 0 1-2 2H6a2 2 '
        '0 0 1-2-2V5a2 2 0 0 1 1-2zm2 2v5h9V5zm5 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6z"/></svg>')
PIN = ('<svg width="16" height="16" viewBox="0 0 16 16"><path fill="#9A9A9A" d="M10 1l5 5-1.5.5-2.5 2.5.5 3-1.5 1.5'
       '-3-3L3 15H1v-2l4-4-3-3L3.5 4.5l3 .5L9 2.5z"/></svg>')
CURSOR = ('<svg width="44" height="56" viewBox="0 0 22 28"><path d="M1 1 L1 22 L7 17 L11 26 L15 24 L11 15 L19 15 Z" '
          'fill="#fff" stroke="#000" stroke-width="1.5" stroke-linejoin="round"/></svg>')
# Tile names for the mock tab. Its tiles stay plain: only the editor draws clean thumbnails, so real ones come
# with tab.png.
NAMES = ["SM_Barrel", "SM_Cannon", "SM_Skull", "SM_Bottle", "SM_Rope_Knot", "SM_Cutlass", "SM_Oar", "SM_Lantern",
         "SM_Palm_A", "SM_Palm_B", "SM_Coconut", "SM_Clam"]


def mock_tile(name, px=None, selected=False):
    """A tile of the mock tab: a plain square with the asset name under it."""
    return f'<div class="tile{" sel" if selected else ""}"><i></i>{name}</div>'


class Page:
    """One image: collects its HTML and remembers whether a stand-in was used."""

    def __init__(self):
        self.parts, self.draft = [], False

    def add(self, *parts):
        self.parts.extend(parts)

    def scene(self, left=0):
        path = SHOTS / "scene.png"
        if path.exists():
            self.add(f'<div class="scene" style="left:{left}px;background-image:url(\'{path.as_uri()}\')"></div>')
        else:
            self.draft = True
            self.add(f'<div class="scene missing" style="left:{left}px">scene.png</div>')

    def panel(self, name, left, top, height, right=None, max_width=None):
        """A screenshot at the given height, its left edge at `left` or its right edge at `right`. A screenshot
        wider than `max_width` is shrunk to fit and centred on the same middle line. Returns its box
        (left, top, width, height)."""
        path = SHOTS / name
        if path.exists():
            w, h = png_size(path)
            width = height * w / h
            if max_width and width > max_width:
                top, height, width = top + (height - max_width * h / w) / 2, max_width * h / w, max_width
            left = right - width if right is not None else left
            self.add(f'<img class="shot" src="{path.as_uri()}" style="left:{left}px;top:{top}px;height:{height}px">')
            return left, top, width, height
        self.draft = True
        if name.startswith("tab"):
            scale = height / 780
            width = 540 * scale
            left = right - width if right is not None else left
            self.add(mock_tab(left, top, scale, menu=name == "tab-menu.png"))
            return left, top, width, height
        width = height * 4 / 3
        left = right - width if right is not None else left
        self.add(f'<div class="shot missing" style="left:{left}px;top:{top}px;width:{width}px;height:{height}px">{name}</div>')
        return left, top, width, height

    def ghost(self, left, top, px, colour):
        path = SHOTS / "tile.png"
        if path.exists():
            tile = f'<img src="{path.as_uri()}" style="width:{px}px;border-radius:4px;box-shadow:0 0 0 4px {colour}">'
        else:
            self.draft = True
            tile = mock_tile(NAMES[0])
        self.add(f'<div class="ghost" style="left:{left}px;top:{top}px;width:{px}px;--c:{colour}">{tile}</div>',
                 f'<div style="position:absolute;left:{left + px * 0.7}px;top:{top + px * 0.75}px">{CURSOR}</div>')

    def logo(self, style):
        self.add(f'<img class="ks" style="{style}" src="{LOGO}">')

    def label(self, title, colour):
        self.add(f'<div class="label"><div class="chip" style="background:{colour}"></div><h2 class="display">'
                 f'{html.escape(title[0])}<em style="color:{colour}">{html.escape(title[1])}</em></h2></div>')

    def html(self, css=""):
        return f"<!doctype html><html><head><meta charset='utf-8'><style>{CSS}{css}</style></head><body>{''.join(self.parts)}</body></html>"


def mock_tab(left, top, scale, menu=False, height=780, drawer=False, opacity=1):
    sidebar = "".join(f'<div class="{"on" if i == 0 else ""}"><img src="{svg_uri("PaletteSidebar", c)}"></div>'
                      for i, c in enumerate([ORANGE, BLUE, GREEN]))
    tiles = [mock_tile(n, selected=i == 0) for i, n in enumerate(NAMES)]
    menu_html = ('<div class="menu"><div>Icon</div><div class="hi">Rename Category...</div>'
                 '<div class="check">Open by Default</div><hr><div>Remove Category</div></div>') if menu else ""
    title = (f'<div class="dhead"><img src="{ICON}">Khaos Palette<span>{PIN}</span></div>' if drawer
             else f'<div class="title"><span><img src="{ICON}">Khaos Palette</span></div>')
    return f"""<div class="tab{" drawer" if drawer else ""}" style="left:{left}px;top:{top}px;height:{height}px;transform:scale({scale});opacity:{opacity}">
  {title}
  <div class="bar"><div class="search">Search palette</div><div class="btn"><img src="{svg_uri("GroupByCategory", "#C0C0C0")}"></div>
    <div class="btn">{GEAR}</div><div class="btn">{SAVE}</div></div>
  <div class="body"><div class="side">{sidebar}</div><div class="main">
    <div class="cat">&#9662; Pirate Den (8)</div><div class="grid">{"".join(tiles[:8])}</div>
    <div class="cat">&#9662; Island (4)</div><div class="grid">{"".join(tiles[8:])}</div>
    <div class="cat">&#9656; Caves (6)</div></div></div>{menu_html}
</div>"""


def point(box, frac):
    left, top, width, height = box
    return left + frac[0] * width, top + frac[1] * height


def cover(page, slide):
    page.scene()
    page.panel(slide["panel"], 0, 70, 940, right=1856, max_width=900)
    page.add(f'<div style="position:absolute;left:96px;top:300px;display:flex;flex-direction:column;gap:30px">'
             f'<img src="{ICON}" style="width:170px;filter:drop-shadow(0 10px 30px rgba(0,0,0,.5))">'
             f'<h1 class="display" style="font-size:136px;text-shadow:0 8px 40px rgba(0,0,0,.45)">Khaos<br>Palette</h1>'
             f'<div style="font:700 30px Montserrat;letter-spacing:.18em;text-transform:uppercase;'
             f'text-shadow:0 2px 16px rgba(0,0,0,.6)">For Unreal Engine 5</div></div>')
    page.logo("left:96px;top:60px")


def callouts(page, slide):
    page.scene()
    page.add('<div class="dim"></div>')
    height = 950
    width = height * 540 / 780
    path = SHOTS / slide["panel"]
    if path.exists():
        w, h = png_size(path)
        width = height * w / h
    box = page.panel(slide["panel"], (1920 - width) / 2, 64, height)
    svg = ['<svg style="position:absolute;left:0;top:0" width="1920" height="1080">']
    for text, frac, colour, side in slide["callouts"]:
        x, y = point(box, frac)
        end = box[0] - 70 if side == "left" else box[0] + box[2] + 70
        dx, anchor = (-16, "end") if side == "left" else (16, "start")
        svg.append(f'<line x1="{x}" y1="{y}" x2="{end}" y2="{y}" stroke="#fff" stroke-width="3"/>'
                   f'<circle cx="{x}" cy="{y}" r="9" fill="{colour}" stroke="#fff" stroke-width="3"/>'
                   f'<text x="{end + dx}" y="{y + 12}" text-anchor="{anchor}" fill="#fff" '
                   f'style="font:900 34px Montserrat;text-transform:uppercase">{html.escape(text)}</text>')
    page.add("".join(svg) + "</svg>")
    page.logo("right:72px;bottom:60px")


def close_up(page, slide):
    page.scene()
    page.panel(slide["panel"], 1010, 150, 1640)
    page.ghost(560, 520, 190, slide["accent"])
    page.add('<svg style="position:absolute;left:0;top:0" width="1920" height="1080"><path d="M1180 470 C 1000 420, '
             '880 470, 780 560" fill="none" stroke="#fff" stroke-width="5" stroke-dasharray="14 12"/></svg>')
    page.add(f'<h1 class="display head" style="left:96px;top:90px">{html.escape(slide["title"][0])}'
             f'<em style="color:{slide["accent"]}">{html.escape(slide["title"][1])}</em></h1>')
    page.logo("left:96px;bottom:72px")


def split(page, slide):
    page.scene(820)
    page.add('<div style="position:absolute;left:0;top:0;bottom:0;width:820px;background:#121116"></div>')
    box = page.panel(slide["panel"], 96, 300, 740, max_width=628)
    sx, sy = point(box, TILE_ROW_EDGE)
    tx, ty, r = slide["target"]
    page.add(f'<h1 class="display head" style="left:96px;top:70px;font-size:96px;text-shadow:none">'
             f'{html.escape(slide["title"][0])}<em style="color:{slide["accent"]}">{html.escape(slide["title"][1])}</em></h1>')
    page.add(f'<svg style="position:absolute;left:0;top:0" width="1920" height="1080"><defs><marker id="a" markerWidth="8" '
             f'markerHeight="8" refX="4" refY="4" orient="auto"><path d="M0 0 L8 4 L0 8 Z" fill="#fff"/></marker></defs>'
             f'<path d="M{sx + 14} {sy} C {sx + 220} {sy - 120}, {tx - r - 190} {ty - 120}, {tx - r - 15} {ty}" '
             f'fill="none" stroke="#fff" stroke-width="5" marker-end="url(#a)"/>'
             f'<circle cx="{tx}" cy="{ty}" r="{r}" fill="none" stroke="{slide["accent"]}" stroke-width="8"/></svg>')
    # With Click to Place on, the tile follows the cursor until the click.
    page.ghost(tx - 60, ty - 70, 120, slide["accent"])
    page.logo("right:72px;bottom:60px")


def colour(page, slide):
    page.add(f'<div style="position:absolute;inset:0;background:{slide["accent"]}"></div>')
    page.panel(slide["panel"], 0, 80, 920, right=1824, max_width=960)
    page.add(f'<h1 class="display head" style="left:96px;top:300px;font-size:104px;color:#141318;text-shadow:none">'
             f'{html.escape(slide["title"][0])}<em style="color:#fff">{html.escape(slide["title"][1])}</em></h1>')
    page.logo("left:96px;bottom:72px;filter:brightness(0)")


def large(page, slide):
    page.scene()
    page.add('<div class="dim"></div>')
    page.panel(slide["panel"], 0, 40, 1000, right=1856, max_width=960)
    page.logo("left:72px;top:60px")
    page.label(slide["title"], slide["accent"])


def full(page, slide):
    path = SHOTS / slide["panel"]
    if path.exists():
        page.add(f'<div class="scene" style="left:0;background-image:url(\'{path.as_uri()}\')"></div>')
    else:
        page.draft = True
        page.add(f'<div class="scene missing" style="left:0">{slide["panel"]}</div>')
    page.logo("left:72px;top:60px")
    page.label(slide["title"], slide["accent"])


def drawer(page, slide):
    """The tab docked to the editor's sidebar, its drawer open over the viewport; faded for the fade image."""
    if (SHOTS / slide["panel"]).exists():
        return full(page, slide)
    page.draft = True
    page.scene()
    page.add(f'<div class="strip"><div>Outliner</div><div>Details</div><div class="on"><img src="{ICON}">Khaos Palette</div></div>')
    scale = 1.2
    page.add(mock_tab(1880 - 540 * scale, 0, scale, height=1080 / scale, drawer=True,
                      opacity=0.8 if slide.get("faded") else 1))  # Faded Opacity is a setting; 0.4 by default
    if slide.get("faded"):
        page.add(f'<div style="position:absolute;left:760px;top:520px">{CURSOR}</div>')
    page.logo("left:72px;top:60px")
    page.label(slide["title"], slide["accent"])


LAYOUTS = {"drawer": drawer, "cover": cover, "callouts": callouts, "close-up": close_up, "split": split, "colour": colour,
           "large": large, "full": full}


def render(slide):
    page = Page()
    LAYOUTS[slide["layout"]](page, slide)
    name = f"KhaosPalette-{slide['id']}{'.draft' if page.draft else ''}.png"
    target = OUT / name
    target.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as work:
        source = pathlib.Path(work) / "page.html"
        source.write_text(page.html(), encoding="utf-8")
        # A fresh profile each time: a browser already running on the profile takes the command over.
        subprocess.run([
            str(BROWSER), "--headless=new", f"--user-data-dir={pathlib.Path(work) / 'profile'}", "--no-first-run",
            "--disable-gpu", "--hide-scrollbars", "--force-device-scale-factor=1", "--allow-file-access-from-files",
            "--window-size=1920,1080", "--virtual-time-budget=8000", f"--screenshot={target}", source.as_uri(),
        ], check=True, capture_output=True)
    if not target.exists():
        raise RuntimeError(f"{BROWSER.stem} wrote no {name}")
    print(name)


if __name__ == "__main__":
    OUT.mkdir(exist_ok=True)
    for old in OUT.glob("KhaosPalette-*.png"):
        old.unlink()
    for slide in SLIDES:
        render(slide)
