"""Set up and capture the editor screenshots through the Unreal editor's MCP server.

    python capture.py palettes   create the sample palettes from Shots/palettes.json (saved in its folder)
    python capture.py scene      capture Shots/scene.png from the camera in palettes.json, editor icons hidden
    python capture.py tile       render Shots/tile.png, the thumbnail of the asset in palettes.json
    python capture.py cleanup    delete the sample palettes

Needs the project open in the editor with the Model Context Protocol plugin and Epic's EditorToolset and
SlateInspectorToolset enabled, and the server started (http://127.0.0.1:8000/mcp). The shots of the tab itself
are taken by hand or over the same server; Shots/README.md lists them.
"""

import base64
import json
import pathlib
import sys
import time
import urllib.request

URL = "http://127.0.0.1:8000/mcp"
SHOTS = pathlib.Path(__file__).resolve().parent / "Shots"
CONFIG = json.loads((SHOTS / "palettes.json").read_text(encoding="utf-8"))
_session = None


def _post(payload):
    headers = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    if _session:
        headers["Mcp-Session-Id"] = _session
    req = urllib.request.Request(URL, json.dumps(payload).encode(), headers, method="POST")
    with urllib.request.urlopen(req, timeout=600) as resp:
        sid, body = resp.headers.get("Mcp-Session-Id"), resp.read().decode("utf-8", "replace")
    if body.lstrip().startswith("{"):
        return sid, json.loads(body)
    data = [json.loads(line[5:]) for line in body.splitlines() if line.startswith("data:")]
    return sid, data[-1] if data else None


def call(toolset, tool, **args):
    """Call a toolset tool and return its return value."""
    global _session
    if not _session:
        _session, _ = _post({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "capture", "version": "1"}}})
        _post({"jsonrpc": "2.0", "method": "notifications/initialized"})
    _, msg = _post({"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {
        "name": "call_tool", "arguments": {"toolset_name": toolset, "tool_name": tool, "arguments": args}}})
    text = "".join(c.get("text", "") for c in msg["result"].get("content", []))
    if msg["result"].get("isError"):
        raise RuntimeError(f"{tool}: {text[:500]}")
    try:
        return json.loads(text).get("returnValue")
    except ValueError:
        return text


def save_image(value, name):
    """Write the image in a tool's return value: the value itself or its first field holding image data."""
    if "data" not in value:
        value = next(v for v in value.values() if isinstance(v, dict) and "data" in v)
    path = SHOTS / name
    path.write_bytes(base64.b64decode(value["data"]))
    print(f"wrote {path.name}")


APP = "EditorToolset.EditorAppToolset"
SLATE = "SlateInspectorToolset.SlateInspectorToolset"


def palettes():
    folder = CONFIG["folder"]
    for name, p in CONFIG["palettes"].items():
        call("editor_toolset.toolsets.data_asset.DataAssetTools", "create", folder_path=folder, asset_name=name,
             asset_type={"refPath": "/Script/KhaosPaletteRuntime.KhaosPalette"})
        values = {
            "displayName": p["displayName"], "description": p["description"],
            "color": dict(zip("rgba", p["color"] + [1.0])),
            "categories": [{"name": c, "icon": {"refPath": ""}, "bCollapsedByDefault": folded}
                           for c, folded in p["categories"]],
            "entries": [{"asset": {"refPath": f"{path}.{path.rsplit('/', 1)[1]}"}, "displayName": label, "category": c}
                        for c, path, label in p["entries"]],
        }
        call("editor_toolset.toolsets.object.ObjectTools", "set_properties",
             instance={"refPath": f"{folder}/{name}.{name}"}, values=json.dumps(values))
        print(f"created {folder}/{name}")
    call("editor_toolset.toolsets.asset.AssetTools", "save_assets",
         asset_paths=[f"{folder}/{name}" for name in CONFIG["palettes"]])


def console(command):
    """Type a command into the editor's console box (the last text box in the main window)."""
    window = call(SLATE, "Snapshot", ref="", maxDepth=1).splitlines()[0]
    ref = window.split("[ref=")[1].rstrip("]")
    call(SLATE, "Observe", ref=ref, maxDepth=80)
    boxes = [line for line in call(SLATE, "Snapshot", ref=ref, maxDepth=80).splitlines() if "textbox" in line]
    box = max(boxes, key=lambda line: int(line.split("[pos=")[1].split(",")[1].split(" ")[0]))
    call(SLATE, "Type", ref=box.split("[ref=")[1].rstrip("]"), text=command, submit=True)


EDITOR_FLAGS = ["BillboardSprites", "Volumes", "Selection", "Grid"]


def scene():
    for flag in EDITOR_FLAGS:
        console(f"ShowFlag.{flag} 0")
    console("ShowFlag.Game 1")
    time.sleep(1)
    try:
        (x, y, z), (pitch, yaw) = CONFIG["camera"]["location"], CONFIG["camera"]["rotation"]
        value = call(APP, "CaptureViewport", bShowUI=False,
                     captureTransform={"location": {"x": x, "y": y, "z": z},
                                       "rotation": {"pitch": pitch, "yaw": yaw, "roll": 0},
                                       "scale": {"x": 1, "y": 1, "z": 1}},
                     annotations={"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0, "maxLabelDistance": 0,
                                  "classFilter": {"refPath": ""}, "maxLabels": 0})
        save_image(value, "scene.png")
    finally:
        for flag in EDITOR_FLAGS + ["Game"]:
            console(f"ShowFlag.{flag} 2")


def tile():
    save_image(call(APP, "CaptureAssetImage", assetPath=CONFIG["tile"]), "tile.png")


def cleanup():
    for name in CONFIG["palettes"]:
        call("editor_toolset.toolsets.asset.AssetTools", "delete", path=f"{CONFIG['folder']}/{name}")
        print(f"deleted {CONFIG['folder']}/{name}")


if __name__ == "__main__":
    {"palettes": palettes, "scene": scene, "tile": tile, "cleanup": cleanup}[sys.argv[1]]()
