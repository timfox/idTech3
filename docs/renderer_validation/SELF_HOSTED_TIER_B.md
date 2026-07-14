# Self-hosted Tier B (stable `GAME_BASE` on every `main` push)

Tier B runs **`renderer_regression_check.sh`**, **`renderer_regression_maps.sh`**, and the modern renderer profile client smoke with a real game tree. GitHub-hosted runners do not include your proprietary `base/` assets or a reliable Vulkan display session, so this uses a **self-hosted** runner that can read a stable path on disk and launch the Vulkan client.

## 1. Prepare `GAME_BASE`

- Use a directory named **`base`** (or adjust scripts / `fs_game` if your tree differs) containing VMs, stock assets, and the renderer regression pack (`z_renderer_regression.pk3` or loose maps). Paths must match [docs/samples/renderer_regression/](../samples/renderer_regression/).
- Same contract as [examples/renderer/game_base.env.example](../../examples/renderer/game_base.env.example).

## 2. Register a self-hosted runner

Follow [GitHub Docs: Adding self-hosted runners](https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/adding-self-hosted-runners).

On the runner machine:

- Install build prerequisites from [docs/DEVELOPMENT_SETUP.md](../DEVELOPMENT_SETUP.md) (CMake, Ninja, compilers, `glslang-tools`, SDL3, etc.-mirror Ubuntu CI).
- Ensure the runner user can **read** `GAME_BASE` and execute the built `idtech3_server`.
- Provide a working client display/Vulkan path for `tests/scripts/test_modern_renderer_profile_runtime.sh runtime`: set `DISPLAY`, install Vulkan userspace drivers/ICD for the GPU, and verify the runner user can execute the built `release/idtech3`.

## 3. Label the runner

Add these labels to the runner (repository or organization settings):

- `self-hosted` (often added by default)
- `idtech3-tierb` (**required** - workflow `.github/workflows/renderer-tier-b.yml` selects this label)

Example CLI when configuring the runner: `--labels self-hosted,Linux,X64,idtech3-tierb`

## 4. Configure the path in GitHub

The workflow enables when **either** a repository **variable** or **secret** named `IDTECH3_GAME_BASE_PATH` is set (non-empty). If **both** are set, the **variable** wins.

| Kind | Name | Value |
|------|------|--------|
| **Actions variable** (recommended) | `IDTECH3_GAME_BASE_PATH` | Absolute path on the runner host, e.g. `/data/idtech3-regression/base` |
| **Actions secret** (optional) | `IDTECH3_GAME_BASE_PATH` | Same path; use if you do not want the string visible under **Variables** (values are still visible in job logs as `env` unless you mask - see below) |
| **Actions variable** (optional) | `IDTECH3_MAPS_EXTRA` | Space-separated BSP names appended to `renderer_regression_maps.sh` after the stock `rtest_*` list (e.g. a map with mixed point + linear dlights). Leave unset if you only use the built-in regression maps. |

**Repository → Settings → Secrets and variables → Actions**

**Masking:** If the path is sensitive, add it to **Organization or repository secrets → Actions** as a separate masking pattern only if GitHub supports your path shape; otherwise treat Tier B as running on a **trusted** self-hosted runner with disk paths that are not secret.

If neither variable nor secret is set, the Tier B workflow **does not run** (job is skipped), so forks and engine-only repos stay green without a runner.

**Alternative:** Leave GitHub unset and run the same two scripts from internal CI or a cron job on the runner machine (`GAME_BASE=... ./scripts/renderer_regression_check.sh` and `renderer_regression_maps.sh`).

## 5. Verify

- Push to `main` or run **Actions → renderer-tier-b → Run workflow**.
- First run may take longer (full Vulkan build on the runner).
- The workflow also runs `IDTECH3_RENDERER_RUNTIME_REQUIRED=1 IDTECH3_REQUIRE_RELEASE_CFGS=1 ./tests/scripts/test_modern_renderer_profile_runtime.sh all`, which checks the source contract, verifies packaged renderer cfgs under `release/base`, and fails if the self-hosted runner cannot launch the Vulkan client.
- Optional: set repository variable **`IDTECH3_MAPS_EXTRA`** to a space-separated list of extra BSP names; the workflow passes it as **`MAPS_EXTRA`** to `renderer_regression_maps.sh` (after the stock `rtest_*` maps). See `docs/samples/renderer_regression/scenes/08_tier_b_mixed_dlights.md`.

## Troubleshooting

- **Job queued forever**: no runner with label `idtech3-tierb` online for this repo/org.
- **Map load failures**: see server log hints in `scripts/renderer_regression_maps.sh`; confirm `+set fs_basepath` / `fs_game` layout matches your install.
- **Client runtime smoke fails immediately**: confirm `DISPLAY` is set for the runner service/session and that Vulkan can create a surface on that display.
- **Missing glslang**: install `glslang-tools` (or equivalent) on the runner so `renderer_regression_check.sh` can validate GLSL.

## Security

- Restrict runner labels to trusted repos; the runner can read `GAME_BASE` and execute the engine.
- Prefer a dedicated machine or VM for CI workloads.
