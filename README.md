# Khaos Palette

[![CI](https://github.com/KhaosSystems/KhaosPalette/actions/workflows/ci.yml/badge.svg)](https://github.com/KhaosSystems/KhaosPalette/actions/workflows/ci.yml)
[![Tests](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/KhaosSystems/KhaosPalette/badges/tests.json)](https://github.com/KhaosSystems/KhaosPalette/actions/workflows/ci.yml)
![Unreal Engine](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/KhaosSystems/KhaosPalette/badges/unreal.json)
![Lines of code](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/KhaosSystems/KhaosPalette/badges/loc.json)
[![Release](https://img.shields.io/github/v/release/KhaosSystems/KhaosPalette)](https://github.com/KhaosSystems/KhaosPalette/releases)

Artist-authored palettes of placeable assets (blueprints, meshes, Niagara systems, sounds,
decals) in a dockable tab, for drag-and-drop set dressing.

## Install

```bash
git submodule add -b release https://github.com/KhaosSystems/KhaosPalette Plugins/KhaosPalette
```

Unreal Engine 5.8. Only the palette asset itself is a runtime module; everything else is
editor-only.

## Docs

[Khaos Palette](Content/Docs/KhaosPalette.md), which also opens in the editor through
[KhaosDocs](https://github.com/KhaosSystems/KhaosDocs).

## Development

Tests are automation specs in `Source/KhaosPaletteEditor/Private/Tests`. Commits are
[conventional commits](https://www.conventionalcommits.org). CI and releases run on
[KhaosSystems/unreal-actions](https://github.com/KhaosSystems/unreal-actions): to release, run the
CI workflow on `main` with *Release the next version* ticked.
