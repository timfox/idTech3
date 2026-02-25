# Branch Strategy

## Active Branches

### `main`
Production-ready code. Always buildable, tested, and release-candidate quality. No experimental features. All merges require CI validation.

### `next-gen`
Active development branch for next-generation renderer features. Contains:
- Vulkan 1.4 renderer with PBR, volumetric fog, fluid simulation
- SMAA anti-aliasing, shadow mapping (CSM + spot atlas + point cubemaps)
- Bloom, SSAO, HDR tonemapping pipeline
- GLSL shader compilation pipeline

### `next-gen-2`
Integration branch built on top of `next-gen`. Adds:
- Modern video codec system (FFmpeg, dav1d, libvpx, Theora)
- Modularized volumetric fog and fluid simulation modules
- Documentation, automation, and CI improvements

This branch will be merged into `next-gen` when all features are validated, then `next-gen` merges into `main` for release.

## Layer Branches

### `vanilla`
Core engine changes that touch the original id Tech 3 foundation. Changes here must maintain 100% backward compatibility.

### `chocolate`
Enhancement features that improve performance or quality without breaking changes. Features can be disabled completely.

### `layercake`
Architectural changes and modern system additions. Clean separation of concerns, extensible design.

## Historical / Feature Branches

| Branch | Purpose |
|--------|---------|
| `bloom-fix-pr` | Bloom pipeline fixes |
| `duktape` | Duktape JavaScript scripting integration |
| `lua` | Lua scripting integration |
| `glints` | Specular glint rendering |
| `archive` | Archived experimental work |

## Merge Flow

```
feature branches → next-gen-2 → next-gen → main
                                    ↑
                    layer branches ──┘
```

1. Feature work happens on topic branches
2. Validated features merge into `next-gen-2` or `next-gen`
3. `next-gen` merges into `main` for releases
4. Layer branches (`vanilla`, `chocolate`, `layercake`) merge into the appropriate integration branch
