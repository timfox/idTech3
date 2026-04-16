# Self-hosted Tier B (stable `GAME_BASE` on every `main` push)

Tier B runs **`renderer_regression_check.sh`** and **`renderer_regression_maps.sh`** with a real game tree. GitHub-hosted runners do not include your proprietary `base/` assets, so this uses a **self-hosted** runner that can read a stable path on disk.

## 1. Prepare `GAME_BASE`

- Use a directory named **`base`** (or adjust scripts / `fs_game` if your tree differs) containing VMs, stock assets, and the renderer regression pack (`z_renderer_regression.pk3` or loose maps). Paths must match [docs/samples/renderer_regression/](../samples/renderer_regression/).
- Same contract as [examples/renderer/game_base.env.example](../../examples/renderer/game_base.env.example).

## 2. Register a self-hosted runner

Follow [GitHub Docs: Adding self-hosted runners](https://docs.github.com/en/actions/hosting-your-own-runners/managing-self-hosted-runners/adding-self-hosted-runners).

On the runner machine:

- Install build prerequisites from [docs/DEVELOPMENT_SETUP.md](../DEVELOPMENT_SETUP.md) (CMake, Ninja, compilers, `glslang-tools`, SDL2, etc.-mirror Ubuntu CI).
- Ensure the runner user can **read** `GAME_BASE` and execute the built `idtech3_server`.

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

**Repository → Settings → Secrets and variables → Actions**

**Masking:** If the path is sensitive, add it to **Organization or repository secrets → Actions** as a separate masking pattern only if GitHub supports your path shape; otherwise treat Tier B as running on a **trusted** self-hosted runner with disk paths that are not secret.

If neither variable nor secret is set, the Tier B workflow **does not run** (job is skipped), so forks and engine-only repos stay green without a runner.

**Alternative:** Leave GitHub unset and run the same two scripts from internal CI or a cron job on the runner machine (`GAME_BASE=... ./scripts/renderer_regression_check.sh` and `renderer_regression_maps.sh`).

## 5. Verify

- Push to `main` or run **Actions → renderer-tier-b → Run workflow**.
- First run may take longer (full Vulkan build on the runner).

## Troubleshooting

- **Job queued forever**: no runner with label `idtech3-tierb` online for this repo/org.
- **Map load failures**: see server log hints in `scripts/renderer_regression_maps.sh`; confirm `+set fs_basepath` / `fs_game` layout matches your install.
- **Missing glslang**: install `glslang-tools` (or equivalent) on the runner so `renderer_regression_check.sh` can validate GLSL.

## Security

- Restrict runner labels to trusted repos; the runner can read `GAME_BASE` and execute the engine.
- Prefer a dedicated machine or VM for CI workloads.
