# Demo skeleton — run `idtech3_demo` over your game data

This folder is a **minimal project layout** you copy or use as a template: it does **not** ship commercial `.pk3` files. You provide a **`base/`** (or `baseq3/`) with VMs and assets; the engine repo supplies the **`idtech3_demo`** config mod.

## Directory layout

Create (or symlink) this structure:

```text
demo_skeleton/                 # or any folder you choose
├── base/                      # YOUR game data (pk3s, maps, vm dlls) — not in git
│   └── …
├── idtech3_demo/
│   └── idtech3_demo.pk3       # built from examples/demo_game (see below)
├── local.env                  # copy from demo_skeleton.env.example (gitignored)
├── idtech3                    # optional: copy or symlink engine client here
├── idtech3_server             # optional: dedicated server
└── run_demo_client.sh         # scripts from this repo folder, or copy them here
```

**`fs_basepath`** must be the directory that **contains** `base/` (or set `fs_basegame` if your data folder has another name).

## 1. Build the demo `.pk3`

From the engine repository:

```bash
./examples/demo_game/build_demo_pack.sh
# → build-vk-Release/idtech3_demo.pk3 (or your build dir)
```

Copy that file to `idtech3_demo/idtech3_demo.pk3` inside your skeleton root.

## 2. Configure paths

```bash
cp demo_skeleton.env.example local.env
# Edit: IDTECH3_DEMO_ROOT, IDTECH3_ENGINE, optional DEMO_MAP
```

`IDTECH3_DEMO_ROOT` = absolute path to the folder that contains `base/` and `idtech3_demo/`.

## 3. Run (client)

**Linux / macOS** (from engine repo, or after copying scripts next to your data):

```bash
./run_demo_client.sh
```

Or with Vulkan launcher from a built tree:

```bash
IDTECH3_ENGINE=/path/to/engine/release/idtech3 \
IDTECH3_DEMO_ROOT=/path/to/demo_skeleton \
/path/to/engine/release/run_vulkan.sh +set fs_basepath "$IDTECH3_DEMO_ROOT" \
  +set fs_game idtech3_demo +set cl_renderer vulkan +map q3dm1
```

Replace `q3dm1` with any map present in your `base/`.

## 4. Run (dedicated, headless smoke)

Useful when you have no display (CI or SSH):

```bash
./run_demo_dedicated.sh
```

Uses `idtech3_server` if present next to the client, or `IDTECH3_ENGINE` can point at `idtech3_server` directly.

## Windows

Copy `run_demo_client.bat` next to your `base/` and `idtech3_demo/`, set `IDTECH3_DEMO_ROOT` inside the file (or same-folder layout), and ensure `idtech3.exe` / `idtech3_server.exe` are on `PATH` or beside the batch file.

## Legal

Only redistribute **engine binaries** and **idtech3_demo** configs you built yourself. Do not redistribute copyrighted game `.pk3` files.

## See also

- [examples/demo_game/README.md](../demo_game/README.md) — what the mod contains
- [docs/QUICKSTART.md](../../docs/QUICKSTART.md) — engine install
- [examples/mods/README.md](../mods/README.md) — `fs_game` conventions
