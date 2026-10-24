# Public repository publication audit

## Findings

- `.fetchcontent-cache/` was tracked. It contains a generated SDL download,
  build products, Gradle state, and a 15 MB source archive. It is not source
  material and is now excluded from future commits.
- Benchmark logs under `sponza_benchmark_results/` were tracked. They can
  expose local paths and machine/build details and are now excluded.
- Reachable history contains generated build/test paths and logs from older
  revisions. Removing them from public history requires a coordinated history
  rewrite and force-push; this is intentionally not performed automatically.
- No obvious API-key, private-key, FTP URL, or password value was found in the
  current tracked text during the initial scan. Configuration examples should
  remain placeholders only.
- Commit history contains contributor and automation identities. Those are
  historical authorship records, not evidence that a tool or service authored
  the code. Do not rewrite them without a specific authorship/legal decision.

## Publication boundary

The public repository should contain GPL-2.0 engine source, compatible source
dependencies with their original notices, build metadata, tests, and clearly
identified examples. It should not contain credentials, private deployment
configuration, licensed game/mod data, generated dependency caches, local
build directories, crash dumps, or machine-specific logs.

See [`LICENSES/THIRD_PARTY.md`](../LICENSES/THIRD_PARTY.md) for the separate
third-party licensing boundary.

## History cleanup still required

If the repository has already published secrets or private files, rotate the
secrets first, then use an approved `git filter-repo`/BFG migration, force-push
the rewritten refs, and ask all downstream clones to reclone. A normal commit
cannot remove those objects from existing public history.
