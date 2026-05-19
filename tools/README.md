# Optional development tools

These directories are **not** linked into the idTech3 engine build. They exist for level design and content workflows.

| Path | Project | License | Notes |
|------|---------|---------|-------|
| [`tiled/`](tiled/) | [Tiled Map Editor](https://www.mapeditor.org/) | GPL-2.0 (see `tiled/COPYING`) | Git submodule; pinned tag in parent commit |

## Clone with submodules

```bash
git clone --recurse-submodules <repo-url>
# or after a plain clone:
git submodule update --init tools/tiled
```

See [docs/TILED.md](../docs/TILED.md) for version pin and build pointers.
