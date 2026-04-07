# Demo playfield — run the engine with the `idtech3_demo` mod

This is the **easiest path** to try renderer hooks and demo configs on top of **your** game data. The repo does not include maps or retail `.pk3` files — you add those once.

---

## Quick start (three steps)

### Step 1 — Build the demo mod (one file)

From the **engine repository root**:

```bash
./examples/demo_game/build_demo_pack.sh
```

That produces **`build-vk-Release/idtech3_demo.pk3`** (or your build directory). Copy it to:

```text
examples/demo_skeleton/idtech3_demo/idtech3_demo.pk3
```

*(The first time, create the folder: `mkdir -p examples/demo_skeleton/idtech3_demo`.)*

### Step 2 — Add game data

Put your compatible game `.pk3` files inside **`examples/demo_skeleton/base/`**.

See **`base/README.txt`** if your game uses **`baseq3`** instead of **`base`**.

### Step 3 — Run

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
| **`VM_Create on UI failed`** / **`ui.qvm not found`** | Your **`base/`** (or **`baseq3/`**) must contain the **full** game archives (e.g. Q3A **`pak0.pk3`** … **`pak8.pk3`**). The demo mod alone cannot start the client — it has no UI VM. |
| “No game data” / missing maps | Add `.pk3` files under **`base/`** (or set **`DEMO_BASE_DIR`** — see below). |
| Wrong renderer | Set **`DEMO_RENDERER=opengl`** in `local.env`, or run with `+set cl_renderer opengl`. |
| Windows | Use **`run_demo_client.bat`** from the same folder layout; put **`idtech3.exe`** next to it or on `PATH`. |

---

## Layout (what goes where)

```text
examples/demo_skeleton/     ← IDTECH3_DEMO_ROOT (folder that contains the two dirs below)
├── base/                   ← your .pk3 game data (see base/README.txt)
├── idtech3_demo/
│   └── idtech3_demo.pk3    ← built demo mod (Step 1)
├── local.env               ← optional; copy from demo_skeleton.env.example
├── run_demo_client.sh
├── run_demo_dedicated.sh
└── run_demo_client.bat     ← Windows
```

**`fs_basepath`** is set to **IDTECH3_DEMO_ROOT** — the directory that **contains** `base/` and `idtech3_demo/`.

---

## Optional: `local.env`

Copy **`demo_skeleton.env.example`** → **`local.env`** and adjust:

| Variable | Meaning |
|----------|--------|
| `IDTECH3_DEMO_ROOT` | Folder containing `base/` and `idtech3_demo/` (only needed if not using `examples/demo_skeleton/`). |
| `IDTECH3_ENGINE` | Full path to `idtech3` if not using `release/idtech3` from this repo. |
| `DEMO_BASE_DIR` | If your data lives in **`baseq3`** instead of **`base`**, set to `baseq3` (passed as `+set fs_basegame`). |
| `DEMO_MAP` | Map to load (e.g. `q3dm1`). Leave unset to open to the main menu. |
| `DEMO_RENDERER` | `vulkan` (default) or `opengl`. |

---

## Helpers

- **`setup_demo_layout.sh`** — Creates `base/` and `idtech3_demo/`, copies `local.env` template, tries to copy `idtech3_demo.pk3` from a local build.
- **`run_demo_dedicated.sh`** — Headless **`idtech3_server`** with the same paths (good for SSH/CI smoke).

---

## Copy this folder elsewhere?

You can copy **`demo_skeleton/`** next to your install and set **`IDTECH3_DEMO_ROOT`** to that copy’s path, or pass the path as the **first argument**:

```bash
./run_demo_client.sh /path/to/my/playfield
```

---

## Legal

Only share **engine binaries** and **configs you built** (e.g. `idtech3_demo.pk3` from this repo). Do not redistribute copyrighted game `.pk3` files.

## See also

- [demo_game README](../demo_game/README.md) — what the mod does (cvars, JS hooks)
- [QUICKSTART](../../docs/QUICKSTART.md) — releases and first run
- [mods README](../mods/README.md) — `fs_game` details
