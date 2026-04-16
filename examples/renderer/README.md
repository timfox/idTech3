# Renderer examples - regression and confidence

Requires a **real game base** (VMs, assets, and optionally the regression pack). Paths below are placeholders.

## Environment template

```bash
cp examples/renderer/game_base.env.example game_base.env
# Edit GAME_BASE, then:
set -a && source ./game_base.env && set +a
```

## File and shader contract (headless-friendly)

```bash
./scripts/renderer_regression_check.sh
```

With content tree:

```bash
GAME_BASE=/absolute/path/to/base ./scripts/renderer_regression_check.sh
```

Optional BSP requirements: edit uncommented lines in `docs/samples/renderer_regression/OPTIONAL_GAME_ASSETS.txt` when you want the check to fail if maps are missing.

## Map load sanity (dedicated server)

```bash
GAME_BASE=/absolute/path/to/base ./scripts/renderer_regression_maps.sh
```

## Manual GPU pass

Follow the **Renderer proof loop** in [docs/RENDERER_CONFIDENCE.md](../../docs/RENDERER_CONFIDENCE.md) and scene specs under [docs/samples/renderer_regression/scenes/](../../docs/samples/renderer_regression/scenes/).
