# GitHub branch protection (manual setup)

`main` is the **only** active integration branch. Configure these in GitHub → **Settings → Branches → Branch protection rules** for `main`.

## Required settings

| Setting | Recommendation |
|---------|----------------|
| **Require a pull request before merging** | On (1 approval if team > 1) |
| **Require status checks to pass** | On — require `build` (or your primary workflow job name from `.github/workflows/build.yml`) |
| **Require branches to be up to date** | On (optional but recommended for AAA) |
| **Do not allow bypassing** | On for production orgs |
| **Restrict pushes** | Maintainers only |
| **Allow force pushes** | Off |
| **Allow deletions** | Off |

## Repository settings (General)

| Setting | Recommendation |
|---------|----------------|
| **Automatically delete head branches** | On (after PR merge) |
| **Dependabot alerts** | On (see `.github/dependabot.yml` for Actions updates) |

## Verify trunk hygiene locally

```bash
./tests/scripts/test_trunk_policy.sh
git fetch origin --prune
git branch -r    # expect origin/main only
git tag -l 'archive/*'
```

## Restore archived legacy work

Legacy remotes were deleted after tagging:

```bash
git fetch --tags origin
git tag -l 'archive/*'
git checkout archive/chocolate-20260622   # example; read-only archaeology
```

See [BRANCHES.md](../BRANCHES.md) and `scripts/archive_legacy_remote_branches.sh`.

## Release gate

Before tagging `v*`, confirm [RELEASE_CHECKLIST.md](../RELEASE_CHECKLIST.md) including CI green on `main`.
