# Optional development tools

These directories are **not** linked into the idTech3 engine build. They exist for level design and content workflows.

| Path | Project | License | Notes |
|------|---------|---------|-------|
| [`../src/external/FreeUSD/`](../src/external/FreeUSD/) | [FreeUSD](https://github.com/gopexllc/FreeUSD) | (upstream) | **Linked into engine** when `USE_FREEUSD=ON` (default); USDA mesh import + `usd_*` tools |
| [`../src/external/idtech3backend/`](../src/external/idtech3backend/) | [idtech3backend](https://github.com/timfox/idtech3backend) | (upstream) | Optional backend/game logic tree; not linked by default — see [docs/IDTECH3_BACKEND.md](../docs/IDTECH3_BACKEND.md) |
| [`tiled/`](tiled/) | [Tiled Map Editor](https://www.mapeditor.org/) | GPL-2.0 (see `tiled/COPYING`) | Git submodule; not linked into engine build |

## Clone with submodules

```bash
git clone --recurse-submodules <repo-url>
# or after a plain clone:
./scripts/init_optional_submodules.sh --freeusd   # engine USD library (default build)
./scripts/init_optional_submodules.sh --backend   # timfox/idtech3backend
./scripts/init_optional_submodules.sh --tiled     # optional map editor
./scripts/init_optional_submodules.sh --all
```

See [docs/FREEUSD.md](../docs/FREEUSD.md), [docs/IDTECH3_BACKEND.md](../docs/IDTECH3_BACKEND.md), and [docs/TILED.md](../docs/TILED.md).
