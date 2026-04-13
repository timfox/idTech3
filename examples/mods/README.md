# Mod examples — `fs_game` and layout

Mods usually ship as a **folder** or **`.pk3`** archives alongside the base game. The engine searches game directories according to `fs_game`, `fs_basegame`, and the launch cwd.

## Directory layout (typical)

```
YourInstall/
├── idtech3
├── base/                    # or baseq3 — stock game pk3s
│   └── pak0.pk3 ...
└── mymod/                   # mod as a sibling directory
    ├── pak_mymod.pk3        # your assets
    └── (optional) vm/       # native or qvm modules, depending on project
```

## Launch examples

Dedicated server with mod as game dir:

```bash
./idtech3_server +set dedicated 1 +set fs_game mymod +set com_hunkMegs 64 +map q3dm1
```

Client (Linux) with Vulkan and mod:

```bash
./idtech3 +set cl_renderer vulkan +set fs_game mymod
```

Use `+set fs_basegame base` (or `baseq3`) if your stock assets live under a non-default name.

## Native modules

Filename probing and `com_nativeLibraryDebug` are documented in [docs/ARCHITECTURE.md](../../docs/ARCHITECTURE.md) (Native game modules) and [docs/QUICKSTART.md](../../docs/QUICKSTART.md) (troubleshooting).

## Packaging

Ship **`mymod/*.pk3`** (and optional `vm/` or platform DLLs) as a zip; users extract next to `base/` and set `fs_game` accordingly.
