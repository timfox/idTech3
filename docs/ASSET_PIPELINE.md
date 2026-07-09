# Asset Pipeline

The engine now has a simple convention-first asset pipeline entrypoint:

```bash
./scripts/asset_pipeline.sh [mod_dir]
```

Default source input is `examples/demo_game/mod`. The script turns a loose source tree into:

1. a **cooked stage directory**
2. **validation logs**
3. a generated **hot reload cfg**
4. a **shipping `.pk3`**
5. a copy under `release/<mod>/`

## Predictable outputs

For mod name `<mod>` the outputs live under:

```text
build/asset-pipeline/<mod>/
├── hot_reload.cfg
├── logs/
├── manifest.txt
├── package/<mod>.pk3
└── stage/
```

This is the stable “source asset -> cooked asset -> validation -> hot reload -> shipping package” path:

- **Source**: loose files under the mod tree, plus optional extras from `asset_pipeline.conf`
- **Cooked**: staged files under `build/asset-pipeline/<mod>/stage/`
- **Validation**: `scripts/validate_assets.sh` runs on both source and staged trees
- **Hot reload**: generated `hot_reload.cfg`
- **Shipping**: zipped `.pk3` copied into `release/<mod>/`

## Optional project config

If present, the pipeline loads either:

- `<mod>/asset_pipeline.conf`
- or a sibling config such as `examples/demo_game/asset_pipeline.conf`

Supported variables:

```bash
ASSET_PIPELINE_MOD_NAME="idtech3_demo"
ASSET_PIPELINE_EXTRA_DIRS=("examples/demo_game/bootstrap_media/gfx")
ASSET_PIPELINE_EXTRA_FILES=("fonts/Inter_28pt-Regular.ttf:fonts/Inter_28pt-Regular.ttf")
ASSET_PIPELINE_HOT_RELOAD_COMMANDS=("exec demo_features.cfg")
```

This keeps setup minimal for content that lives slightly outside the mod folder.

## Demo example

```bash
./scripts/asset_pipeline.sh examples/demo_game/mod --skip-shaders
```

That pipeline:

- stages the demo mod tree
- merges `bootstrap_media/gfx` and `bootstrap_media/fonts`
- adds the repo Inter font and `loc/en.loc`
- validates both source and cooked outputs
- writes `build/asset-pipeline/idtech3_demo/hot_reload.cfg`
- emits `build/asset-pipeline/idtech3_demo/package/idtech3_demo.pk3`
- copies the same package to `release/idtech3_demo/idtech3_demo.pk3`

## Hot reload

The generated `hot_reload.cfg` is intentionally simple and conservative:

- `vid_restart keep_window`
- `reloadTtf`
- `script_reload`
- `js_reload`
- `py_reload`
- `cs_reload`

Project-specific reload commands from `asset_pipeline.conf` are appended at the end.

## Validation and regression coverage

- `scripts/validate_assets.sh` now works with both source and staged trees
- `tests/scripts/test_asset_pipeline.sh` verifies the demo path end to end

Run it directly:

```bash
./tests/scripts/test_asset_pipeline.sh
```
