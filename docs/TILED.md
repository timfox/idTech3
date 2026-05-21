# Tiled Map Editor (optional submodule)

[Tiled](https://www.mapeditor.org/) is a general-purpose **2D level editor** (tile maps, object layers, exports). This fork vendors it as an **optional Git submodule** for designers working on tile-based or hybrid content. It is **not** compiled into `idtech3` or the renderer plugins.

## License

Tiled is **GPL-2.0** (see `tools/tiled/COPYING` and `tools/tiled/LICENSE.GPL`). The engine remains GPL-2.0-compatible; the submodule is a separate work—you build and run Tiled on its own terms. Do not link Tiled Qt code into the engine binary.

## Submodule

| Item | Value |
|------|--------|
| Path | `tools/tiled` |
| Upstream | https://github.com/mapeditor/tiled |
| Pin | Tag **`v1.9.91`** (recorded in the parent-repo submodule commit) |

Initialize after clone:

```bash
./scripts/init_optional_submodules.sh --tiled
# equivalent:
git submodule update --init tools/tiled
```

Update to a newer upstream release (maintainers):

```bash
cd tools/tiled
git fetch --tags origin
git checkout v1.10.0   # example: pick a release tag
cd ../..
git add tools/tiled
git commit -m "Bump Tiled submodule to v1.10.0"
```

## Building Tiled (local)

Tiled is a **Qt** application. Build instructions change by platform; use upstream docs:

- https://doc.mapeditor.org/en/stable/development/building-from-source.html

Typical Linux deps (Qt 5 or 6 per upstream branch): `qtbase5-dev` / `qt6-base-dev`, `libqt5svg5-dev`, build tools. The submodule is large; only init it when you need the editor.

## Engine relationship

- Quake III–style maps remain **`.map` / BSP** in the vanilla pipeline.
- Tiled **`.tmx` / `.json`** exports are a **content-authoring** path; any runtime loader would be a separate game/mod feature, not this submodule.
- Sample map and designer notes: [examples/tiled/README.md](../examples/tiled/README.md) (`minimal_demo.tmx`).
