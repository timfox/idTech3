# Minimal game shell (bootstrap game data)

This document describes the **smallest filesystem layout** that still satisfies the engine’s definition of “having game data,” so the client or dedicated server **starts instead of exiting with “No game data.”** It is aimed at **new games, tech demos, and CI smoke trees**—not at shipping a full commercial title.

For a **turn-key demo folder** inside this repository, see [examples/demo_skeleton/README.md](../examples/demo_skeleton/README.md). For **renderer regression / headless dedicated** trees, see [renderer_validation/devdata/README.md](renderer_validation/devdata/README.md).

---

## What the engine actually requires

After the filesystem starts (`FS_Restart` in `src/qcommon/files.c`), two gates matter for a normal local install:

1. **At least one loadable pack with at least one file inside it**  
   The engine counts files that live inside **`.pk3` / `.zip` / `.orb`** archives on the search path. If **no** such files load, **`fs_packFiles == 0`**, the process shows **“No game data”** and quits.  
   **Implication:** An empty `base/` directory, or a folder that only has loose files and **no** archives, is **not** enough. You need **at least one valid archive** the scanner recognizes (by extension), which unpacks to **one or more** virtual paths.

2. **`default.cfg` must resolve through the search path**  
   If `default.cfg` is missing and you are **not** on the special pure-server recovery path, the engine treats the tree as broken and again reports **“No game data”** (after attempting to write a tiny auto-generated `default.cfg` only when allowed).  
   **Practical rule:** ship **`default.cfg`** inside your minimal `.pk3` (or as a loose file under a **`.pk3dir`** tree if you use that workflow). Do not rely on the fallback unless you understand the pure-server exception in the source.

Optional but useful:

- **`gameinfo.txt`** — if present, parsed for window title / bumper text (`FS_ParseGameInfo`). Not required to boot.
- **`fs_basegame`** — stock idTech3 expects data under **`base/`** (`BASEGAME` is `"base"` in `q_shared.h`). Many Quake III installs use **`baseq3/`**; users can point the engine at that name with **`+set fs_basegame baseq3`** (see [QUICKSTART.md](QUICKSTART.md)).

---

## Directory layout (bare minimum)

Place this **next to** the engine binaries (or set **`fs_basepath`** to the directory that **contains** the game folder):

```text
your_install/
├── idtech3                    # client (or idtech3.exe)
├── idtech3_server             # optional
├── idtech3_vulkan.so          # when using dlopen renderers (Linux)
└── base/                      # or baseq3/ + fs_basegame (see above)
    └── z_your_bootstrap.pk3   # at least one .pk3; see contents below
```

**Rules of thumb:**

- **`fs_basepath`** — directory that **contains** `base/` (or your `fs_basegame` folder).
- **`fs_game`** — optional **mod** directory **beside** `base/` (e.g. `mymod/mymod.pk3`). The **base** tree must still satisfy the two gates above; mods layer on top.

---

## Minimal `.pk3` contents

A `.pk3` is a **normal ZIP** file with the `.pk3` extension. Create one that exposes at least:

| Path inside the zip | Purpose |
|---------------------|---------|
| `default.cfg` | **Required** for predictable startup; keep it tiny (cvars only, `exec` optional). |

That alone is enough for **“game data exists”** in the sense of the filesystem checks, as long as the zip is non-empty and loads.

**Strongly recommended next steps** (still “small shell,” but closer to something you can call a game):

| Path | Purpose |
|------|---------|
| `gameinfo.txt` | Window / branding (`title "My Game"`). |
| `menu/…` or UI VM / native UI | Without **some** UI or **+map** on the command line, the client may open to a black or minimal screen depending on renderer and cvars—still “launched,” but not player-friendly. |
| `maps/yourmap.bsp` | Lets you **`+map yourmap`** for a definite 3D view. |
| `vm/qagame.qvm` (or native game module) | **Dedicated server** map load and game logic expect **qagame** to exist for a real match; see the regression devdata doc for a GPL reference `qagame.qvm` and stub BSPs. |

---

## Example: build a bootstrap pack with common tools

From an empty folder:

```bash
printf '%s\n' \
  '// Minimal shell — extend for your game' \
  'set com_introPlayed 0' \
  'set in_mouse 1' \
  > default.cfg

zip -9 -X -r z_bootstrap.pk3 default.cfg
mkdir -p base
mv z_bootstrap.pk3 base/
```

Run (Linux):

```bash
./idtech3 +set fs_basepath "$PWD"
```

If your data lives in **`baseq3/`** instead:

```bash
./idtech3 +set fs_basepath "$PWD" +set fs_basegame baseq3
```

---

## Compile the engine (so you have binaries to point at)

From the repository root (see [BUILD.md](../BUILD.md) and [CLAUDE.md](../CLAUDE.md) for full detail):

```bash
./scripts/compile_engine.sh vulkan    # or: vulkan debug, etc.
```

Artifacts are copied under **`release/`** (and the CMake build tree under **`build-vk-Release/`** or similar). Your **`base/`** folder should sit **alongside** those binaries, **or** you pass an absolute **`+set fs_basepath /path/to/parent`**.

---

## Dedicated server vs client

| Goal | Minimum extra beyond `default.cfg` in a `.pk3` |
|------|-----------------------------------------------|
| **Process starts, no “No game data”** | One valid `.pk3` with `default.cfg` (and any one other file if you prefer—still one archive). |
| **`+map mapname` loads on dedicated** | Valid **`maps/mapname.bsp`** plus a loadable **`qagame`** (`.qvm` under `vm/` is the usual portable form). Use [build_renderer_devdata.sh](../scripts/build_renderer_devdata.sh) as a **recipe reference**, not as your shipping content. |
| **Interactive “game” on client** | Same as above **plus** UI or a map + key bindings so the player is not stuck at a blank screen. |

---

## In-repo shortcuts (recommended)

Instead of inventing a pack from scratch every time:

1. **`examples/demo_game/build_demo_pack.sh`** — builds **`idtech3_demo.pk3`**, which includes demo config and a **minimal UI** module so the **client** can show a real menu path when combined with **`examples/demo_skeleton/`** (see that README).
2. **`./scripts/build_renderer_devdata.sh`** — builds **`docs/renderer_validation/devdata/rtest_base/`**, a **GPL-friendly** tiny tree for **dedicated** map + **qagame** checks.

---

## Troubleshooting

| Symptom | Likely cause |
|--------|----------------|
| **“No game data”** immediately | No **`.pk3`/`.zip`/`.orb`** loaded, **`fs_packFiles == 0`**, or **`default.cfg`** not visible on the search path. Add a zip under `base/` and include `default.cfg`. |
| **Wrong folder name** | Data is under **`baseq3/`** but the engine looks for **`base/`**. Use **`+set fs_basegame baseq3`** or rename the directory. |
| **Black screen / no UI** | You satisfied FS only; add **map +map**, **menu assets**, or run with the **demo_skeleton** + **idtech3_demo.pk3** stack. |
| **Dedicated exits after map** | Missing or incompatible **`vm/qagame.qvm`** (or native **qagame**) for your BSP / game mode. |

---

## License note

Do **not** redistribute commercial **`pak0.pk3`** files. For a **bootstrap shell** you own, keep sources and assets **original or GPL-compatible** (as with the ioquake3-derived pieces referenced in the renderer devdata README).
