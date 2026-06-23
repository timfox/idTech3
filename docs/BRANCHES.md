# Branch Strategy (2026 trunk)

## Production trunk

### `main` (only active integration branch)

- **Single source of truth** for releases, CI, and agent work.
- Must stay buildable: `./scripts/compile_engine.sh vulkan` + `./scripts/smoke_test.sh release`.
- **No long-lived `cursor/*` agent branches** — work lands via short-lived `feature/*` PRs into `main`.
- Protected on GitHub: changes via PR + CI green (see `.github/workflows/build.yml`).

### Release tags

- Ship from **`main`** only: `vMAJOR.MINOR.PATCH` (see [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md)).
- Optional stabilization: `release/vX.Y` cut from a tagged `main` commit; hotfixes merge to `release/vX.Y` then back to `main`.

## Short-lived development branches

| Pattern | Use |
|---------|-----|
| `feature/<name>` | New capability (Vulkan, mods, tooling) |
| `fix/<name>` | Bugfix |
| `docs/<name>` | Documentation-only |
| `chore/<name>` | CI, scripts, non-product code |

**Rules**

1. Branch from latest `main`; rebase or merge `main` before opening PR.
2. Delete the branch after merge (GitHub “delete branch” or `git push origin --delete feature/...`).
3. Do **not** open parallel integration branches (`next-gen-2`, `cursor/*`, etc.) — they diverge from trunk and cannot be merged safely.

## Layer concepts (not separate long-lived remotes)

The constitution still describes **vanilla / chocolate / layercake** as *change classification*, not separate git remotes:

| Layer | Meaning |
|-------|---------|
| Vanilla | Core Q3 compatibility — must not break retail QVM/pk3 |
| Chocolate | Opt-in enhancements with fallbacks |
| Layercake | Modern modules (Vulkan, open world, research) behind CMake/profile gates |

Tag PRs and commits with layer intent in the description; all layers integrate into **`main`**.

## Archived legacy remotes

Historical branches (`chocolate`, `cherry`, `archive`, `overbaked`, `glints`, …) were **tagged and removed** from `origin` during trunk consolidation. Tags follow:

```text
archive/<branch-name>-YYYYMMDD
```

List tags:

```bash
git fetch --tags origin
git tag -l 'archive/*'
```

Restore read-only inspection:

```bash
git checkout archive/chocolate-YYYYMMDD
```

GitHub protection checklist: [core/BRANCH_PROTECTION.md](core/BRANCH_PROTECTION.md).

Scripts:

- `scripts/archive_legacy_remote_branches.sh` — tag tips + delete legacy remotes
- `scripts/cleanup_remote_cursor_branches.sh` — remove stale `cursor/*` remotes
- `scripts/integrate_cursor_branches.sh` — optional cherry-pick from agent branches (prefer PRs instead)

## Merge flow

```text
feature/fix branch → PR → CI → main → release tag
```

## CI expectations (every PR to `main`)

- Ubuntu + Windows matrix in `.github/workflows/build.yml`
- Smoke test on release artifacts where configured
- `./scripts/q3_openarena_compat_check.sh` for QVM/OA regressions
- `ctest -R unit` for fast unit gates when CMake test target exists locally

## Agent / automation policy

- Cloud agents and Cursor tasks: **commit to `feature/*`, open PR to `main`** — do not push `cursor/*` remotes.
- Run `./scripts/pr_ready_summary.sh` before requesting review.
