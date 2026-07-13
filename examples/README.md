# Examples

Small, **copy-paste** workflows and templates for this engine repo. They do not ship game data; point `GAME_BASE` at your own `base/` (or game-specific folder) when a step needs maps and VMs.

| Path | Purpose |
|------|---------|
| [engine/](engine/) | Local build and automated validation (smoke, `ctest`, production readiness). |
| [renderer/](renderer/) | Renderer regression and `GAME_BASE` environment template. |
| [mods/](mods/) | Typical mod / `fs_game` layout and launch examples. |
| [templates/](templates/) | Project scaffolds: `game.minimal`, `game.starter`, and addon basics. |
| [scripts/](scripts/) | Pointers to in-repo script and UI samples under `docs/samples/`. |
| [title-repo/](title-repo/) | Templates for a **game** repo: platform cert, telemetry, soak, submissions. |
| [demo_game/](demo_game/) | **Buildable** config mod `idtech3_demo.pk3` (+ optional helper) to toggle renderer cvars over a real `base/`. |
| [demo_skeleton/](demo_skeleton/) | **Easiest demo run**: `./scripts/run_demo.sh` after adding game data to `demo_skeleton/base/` (see README). |
| [tiled/](tiled/) | Optional **Tiled Map Editor** workflow (`.tmx` sample); init submodule with `./scripts/init_optional_submodules.sh --tiled`. |

See also:

- [docs/QUICKSTART.md](../docs/QUICKSTART.md) - install release binaries and game data.
- [docs/MINIMAL_GAME_SHELL.md](../docs/MINIMAL_GAME_SHELL.md) - smallest valid `base/` + `.pk3` bootstrap.
- [docs/CONTENT_IDENTITY.md](../docs/CONTENT_IDENTITY.md) - default shell and opinionated starter workflow.
- [docs/PRODUCTION_CERTIFICATION.md](../docs/PRODUCTION_CERTIFICATION.md) - tiered production bar.
- [docs/samples/renderer_regression/README.md](../docs/samples/renderer_regression/README.md) - visual regression pack.
- [docs/samples/box3d_examples/README.md](../docs/samples/box3d_examples/README.md) - Box3D sample-family scenes runnable through the demo mod.
- [docs/TILED.md](../docs/TILED.md) - optional Tiled submodule (GPL-2.0).
