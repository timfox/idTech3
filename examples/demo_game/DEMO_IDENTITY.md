# idtech3_demo — project identity

This example mod is **original to the Gopex idTech3 engine repository**. It does not ship maps, characters, audio samples, or gameplay from any third-party game.

## What is unique here

| Asset | Origin |
|-------|--------|
| Configs (`mod/*.cfg`) | Engine-authored, GPL |
| `gameinfo.txt`, `readme_demo.txt` | Engine-authored |
| Bootstrap PNG/TTF in `bootstrap_media/` | Generated or OFL-licensed (see `fonts/LICENSE.txt`) |
| Native `ui` stub (`native/ui_skeleton_stub.c`) | Engine-authored |
| Script hooks (`demo_hooks.js`, `demo_hooks.lua`, …) | Engine-authored |
| Neural placeholder manifests (`niv/demo.niv`, …) | Empty/scaffold stubs for CI |

## What you provide separately

- **Base game data** under `base/` from a **compatible install** you already own (see [docs/COMPATIBILITY.md](../../docs/COMPATIBILITY.md)).
- **Maps** when you want in-world testing (`+map yourmap`).

The demo never requires a specific commercial title in configs or scripts.
