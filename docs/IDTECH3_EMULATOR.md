# idTech3 Emulator submodule

Optional Git submodule: a **QEMU fork** for sandboxed guest operating systems that can be shown on in-world surfaces (monitor props, terminals, holograms) via a **Vulkan render texture** fed from the emulator display pipeline.

| Item | Value |
|------|--------|
| **Repository** | [timfox/idTech3-Emulator](https://github.com/timfox/idTech3-Emulator) |
| **Path** | `third_party/idtech3-emulator` |
| **Build** | **Not** linked into `idtech3` by default; build QEMU inside the submodule tree |
| **Engine flag** | `USE_IDTECH3_EMULATOR=OFF` (enable with `-DUSE_IDTECH3_EMULATOR=ON` or `./scripts/compile_engine.sh vulkan emulator`) |

## Cvars and commands (engine bridge)

| Cvar | Default | Role |
|------|---------|------|
| `cl_emulator` | 0 | Master toggle — pumps guest frames and uploads to renderer |
| `cl_emulator_width` | 640 | Frame pump width (test pattern / shm) |
| `cl_emulator_height` | 480 | Frame pump height |
| `cl_emulator_drawHud` | 0 | Debug HUD preview (needs `r_emulatorScreen 1`) |
| `cl_emulator_captureKeys` | 0 | Route keyboard to guest input shm |
| `r_emulatorScreen` | 0 | Renderer uploads to `*emulator_screen` texture |

| Command | Description |
|---------|-------------|
| `emulator_start [disk.img]` | Launch out-of-process QEMU guest (`microvm`, headless) |
| `emulator_stop` | Terminate guest |
| `emulator_status` | Guest pid, shm attach, texture/input ring state |
| `emulator_capture [0\|1]` | Toggle keyboard capture (ESC releases) |

Quick test (after building with `emulator` flag):

```bash
./release/idtech3 +set cl_emulator 1 +set r_emulatorScreen 1 +set cl_emulator_drawHud 1
emulator_start
emulator_capture 1
emulator_status
```

Demo config (demo_game pk3 when built): `exec demo_emulator.cfg`. Map shader: `scripts/emulator_screen.shader` → `emulator_screen` stage using `*emulator_screen`.

## POSIX shared-memory contract

Engine and QEMU fork agree on two shm segments (Linux only for now):

| Segment | Name | Writer | Reader |
|---------|------|--------|--------|
| Display | `/idtech3_emulator_frame` | QEMU fork | Engine (`Emulator_Frame_Pump`) |
| Input | `/idtech3_emulator_input` | Engine | QEMU fork |

**Frame header** (`emulatorFrameHeader_t`): magic `EMULATOR_FRAME_MAGIC` (`0x314d5545`, `'EUM1'`), width, height, stride, frameIndex, format (`0` = RGBA8888), followed by `width*height*4` bytes.

**Input header** (`emulatorInputHeader_t`): magic `EMULATOR_INPUT_MAGIC` (`0x31504945`, `'EIP1'`), writeIdx, readIdx, ringSize (`256`). Ring of `emulatorInputEvent_t`: type (keydown/keyup/char), engine keynum, ascii, modifier bitmask.

QEMU child receives env vars on launch:

- `IDTECH3_EMULATOR_FRAME_SHM`
- `IDTECH3_EMULATOR_INPUT_SHM`
- `IDTECH3_EMULATOR_WIDTH` / `IDTECH3_EMULATOR_HEIGHT`

Phase 3 (planned): virtio-gpu / vhost-user-gpu fd import for zero-copy (VUDA-style).

## Initialize

```bash
git submodule update --init third_party/idtech3-emulator
# or:
./scripts/init_optional_submodules.sh --emulator
./scripts/init_optional_submodules.sh --all
```

Clone with submodules:

```bash
git clone --recurse-submodules <idtech3-repo-url>
```

## Architecture

```text
  Guest OS (QEMU)          idTech3 client
  ───────────────          ──────────────
  VGA / virtio-gpu    →    /idtech3_emulator_frame (RGBA + header)
                           ↓
                      Vulkan *emulator_screen texture
                           ↓
                      World mesh shader (emulator_screen)
                           ↓
  keyboard/mouse      ←    /idtech3_emulator_input (ring buffer)
```

Design constraints (from project constitution):

- **Opt-in** — cvar toggle + startup log when enabled.
- **Sandboxed** — guest runs out-of-process; no direct engine memory access.
- **Fallback** — test pattern when shm missing; feature disabled cleanly when submodule or QEMU binary is missing.
- **No default dependency** — stock engine and retail QVM mods must build and run unchanged.

## Build QEMU (submodule)

Linux example (inside the submodule; requires usual QEMU build deps):

```bash
cd third_party/idtech3-emulator
mkdir -p build && cd build
../configure --target-list=x86_64-softmmu --enable-kvm
make -j"$(nproc)"
```

Binary (default): `third_party/idtech3-emulator/build/qemu-system-x86_64`.

See the submodule [QEMU documentation](https://www.qemu.org/documentation/) for host-specific prerequisites.

## CMake

```bash
cmake -DUSE_IDTECH3_EMULATOR=ON ..
```

When `ON`, configure **requires** the initialized submodule. The engine does not compile QEMU sources; it links the client bridge and defines `IDTECH3_EMULATOR_DIR`.

Tests: `ctest -R test_idtech3_emulator`, `ctest -R unit_emulator_contract`.

## Update pin

```bash
cd third_party/idtech3-emulator
git fetch origin && git checkout <commit>
cd ../..
git add third_party/idtech3-emulator
```

Commit the updated submodule pointer in the parent repo when you bump the emulator revision.
