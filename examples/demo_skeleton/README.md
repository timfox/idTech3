# Demo playfield - run the engine with the `idtech3_demo` mod

This is the **easiest path** to try renderer hooks and demo configs on top of **your** game data. The repo does not include maps or retail `.pk3` files - you add those once.

---

## Quick start (three steps)

### Step 1 - Demo mod `.pk3` (usually already present)

The repo ships **`idtech3_demo/idtech3_demo.pk3`** (configs, **gameinfo.txt**, native **`vm/ui*.so`**, **Inter** font, **bootstrap shaders/gfx**). Rebuild when you change files under **`examples/demo_game/mod/`**:

```bash
./examples/demo_game/build_demo_pack.sh
cp build-vk-Release/idtech3_demo.pk3 examples/demo_skeleton/idtech3_demo/
```

*(Or run **`./examples/demo_skeleton/setup_demo_layout.sh`** after a local build; it copies the pack if found.)*

### Step 2 - Base game data

**`base/`** includes **`z_minimal_bootstrap.pk3`** (GPL: **default.cfg** + **gameinfo.txt**) so the filesystem gate passes with **no retail files**. That is enough for a **client window** together with **`idtech3_demo.pk3`**.

Add your compatible game `.pk3` files under **`base/`** when you want **maps, retail menus, and qagame**.

See **`base/README.txt`** if your game uses **`baseq3`** instead of **`base`**.

### Step 3 - Run

From the repo root:

```bash
./scripts/run_demo.sh
```

Or directly:

```bash
./examples/demo_skeleton/run_demo_client.sh
```

The scripts look for **`base/`** and **`idtech3_demo/`** next to themselves, so you often **do not need** a config file if you use this folder layout.

**Optional:** set a map and renderer in **`local.env`** (copy from **`demo_skeleton.env.example`**). That file is gitignored so your paths stay private.

---

## If something goes wrong

| Problem | What to do |
|--------|------------|
| “Missing … idtech3_demo.pk3” | Run Step 1 and copy the `.pk3` into `idtech3_demo/`. |
| “No engine binary” | Build the engine (`./scripts/compile_engine.sh vulkan`) or set **`IDTECH3_ENGINE`** in `local.env` to your `idtech3` path. |
| **`VM_Create on UI failed`** / **`ui.qvm not found`** | Use an **up-to-date engine** (extracts `vm/*.so` from pk3 to `vm/native_cache/`) and rebuild **`idtech3_demo.pk3`** so **`vm/ui*.so`** / **`vm/ui.*.so`** are inside the zip. Copy the `.pk3` into **`idtech3_demo/`** next to `base/`, not only `release/`. |
| “No game data” | Ensure **`base/*.pk3`** exists (ship **`z_minimal_bootstrap.pk3`** or run **`setup_demo_layout.sh`** from the repo copy). |
| Dedicated exits / no map | **`run_demo_dedicated.sh`** no longer forces **`q3dm1`**. Set **`DEMO_MAP`** in **`local.env`** or pass **`+map …`** when you have BSP + **qagame**. |
| Renderer init fails | Confirm Vulkan drivers and SDL Vulkan support; use **`DEMO_RENDERER=vulkan`** (default) and `vid_restart`. |
| Windows | Use **`run_demo_client.bat`** from the same folder layout; put **`idtech3.exe`** next to it or on `PATH`. |

---

## Layout (what goes where)

```text
examples/demo_skeleton/     ← IDTECH3_DEMO_ROOT (folder that contains the two dirs below)
├── base/                   ← z_minimal_bootstrap.pk3 + your licensed .pk3 (see base/README.txt)
├── idtech3_demo/
│   └── idtech3_demo.pk3    ← demo mod (rebuild after editing mod/; tracked in git)
├── local.env               ← optional; copy from demo_skeleton.env.example
├── run_demo_client.sh
├── run_demo_dedicated.sh
└── run_demo_client.bat     ← Windows
```

**`fs_basepath`** is set to **IDTECH3_DEMO_ROOT** - the directory that **contains** `base/` and `idtech3_demo/`.

---

## Optional: `local.env`

Copy **`demo_skeleton.env.example`** → **`local.env`** and adjust:

| Variable | Meaning |
|----------|--------|
| `IDTECH3_DEMO_ROOT` | Folder containing `base/` and `idtech3_demo/` (only needed if not using `examples/demo_skeleton/`). |
| `IDTECH3_ENGINE` | Full path to `idtech3` if not using `release/idtech3` from this repo. |
| `DEMO_BASE_DIR` | If your data lives in **`baseq3`** instead of **`base`**, set to `baseq3` (passed as `+set fs_basegame`). |
| `DEMO_MAP` | Map to load (e.g. `q3dm1`). Leave unset to open to the main menu. |
| `DEMO_RENDERER` | `vulkan` (default). |

---

## Helpers

- **`setup_demo_layout.sh`** - Creates `base/` and `idtech3_demo/`, copies **`z_minimal_bootstrap.pk3`** when missing, copies `local.env` template, tries to copy **`idtech3_demo.pk3`** from a local build.
- **`run_demo_dedicated.sh`** - **`idtech3_server`** with **`dedicated 1`** and the same **`fs_basepath`** / **`fs_game`**; optional **`DEMO_MAP`** or **`+map`**.
- **`scripts/smoke_demo_skeleton.sh`** - Short client run (**`+quit`**) that fails if **`R_Init`** or bootstrap shaders/fonts are broken (needs display).

---

## Copy this folder elsewhere?

You can copy **`demo_skeleton/`** next to your install and set **`IDTECH3_DEMO_ROOT`** to that copy’s path, or pass the path as the **first argument**:

```bash
./run_demo_client.sh /path/to/my/playfield
```

---

## Legal

Only share **engine binaries** and **configs you built** (e.g. `idtech3_demo.pk3` from this repo). **`z_minimal_bootstrap.pk3`** is engine-authored GPL text. Do not redistribute copyrighted game `.pk3` files.

## See also

- [demo_game README](../demo_game/README.md) - what the mod does (cvars, JS hooks, bootstrap media)
- [QUICKSTART](../../docs/QUICKSTART.md) - releases and first run
- [mods README](../mods/README.md) - `fs_game` details
