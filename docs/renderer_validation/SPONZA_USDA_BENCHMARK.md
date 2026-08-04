# Sponza USDA benchmark (`SPONZA_USDA_BENCHMARK.md`)

This is the repeatable benchmark scene for USDA material and lighting work.
The source payload is `sponza/main_sponza/NewSponza_Main_USD_Zup_003.usda`,
loaded through `world/sponza_fixture.usda`. The fixture keeps the USDA camera
and isolates the host BSP, so a capture is not accidentally made from a
random OpenArena map position. Its `fullMesh` metadata explicitly owns
the full Sponza payload instead of relying on an implicit slug path.

Run the isolated benchmark with a display and the demo game data:

```sh
./scripts/run_sponza_benchmark.sh
```

For local source-tree validation, make the two game-tree links once before
running the capture (the 417 MiB USDA and textures remain outside game data):

```sh
mkdir -p release/demo_game/world/districts
ln -sfn "$PWD/tests/data/usd/sponza_fixture.usda" \
  release/demo_game/world/sponza_fixture.usda
ln -sfn "$PWD/sponza/main_sponza/NewSponza_Main_USD_Zup_003.usda" \
  release/demo_game/world/districts/sponza.usda
```

The runner launches one client per renderer mode, avoiding a cross-mode
`vid_restart` while that recovery path is being repaired. Each process fixes
the output at 1280x720, disables TAA and post-film effects, loads the USDA
fixture with a fixed mesh budget, and emits a deterministic JPEG:

| Capture | Ownership under test |
| --- | --- |
| `forwardplus.jpg` | portable clustered Forward+ baseline |
| `deferred.jpg` | deferred G-buffer and clustered lighting |
| `wboit.jpg` | unified clustered opaque + WBOIT transparency |

Each checkpoint also prints `renderer_status` and
`renderer_capture_frame_contract`; the mode-specific checkpoints print
`cluster_status`, `deferred_status`, or `oit_status`. Preserve those logs with
the JPEGs when comparing a renderer change. A screenshot without a clean frame
contract is not a valid golden.

The repository-level contract is checked with:

```sh
bash tests/scripts/test_sponza_benchmark.sh
```

This test validates the benchmark wiring and capture names without requiring a
GPU. It does not claim a performance number. Record FPS, GPU, driver, build
profile, and commit separately when establishing a performance baseline.
