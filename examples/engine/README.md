# Engine examples — local build and validation

Run all commands from the **repository root** unless noted.

## Prerequisites

- Linux: see [docs/DEVELOPMENT_SETUP.md](../../docs/DEVELOPMENT_SETUP.md) (CMake, Ninja, compilers, `glslang-tools` for full shader validation in smoke tests).
- Windows / macOS: use the same scripts where applicable, or follow CI job steps in `.github/workflows/build.yml`.

## Build

```bash
# Vulkan + OpenGL Release (artifacts under release/)
./scripts/compile_engine.sh vulkan
./scripts/compile_engine.sh opengl

# Debug Vulkan
./scripts/compile_engine.sh vulkan debug
```

## Quick checks

```bash
./scripts/smoke_test.sh release
cd build-vk-Release && ctest --output-on-failure
./scripts/evidence_status.sh
```

`evidence_status.sh` prints Tier A–D gaps (build dirs, `GAME_BASE`, FINDINGS placeholder) — see [docs/PRODUCTION_CERTIFICATION.md](../../docs/PRODUCTION_CERTIFICATION.md).

## Full local CI parity + production orchestrator

```bash
# Same stack as validate_ci_build + optional OpenGL + full ctest (+ maps if GAME_BASE set)
./scripts/production_readiness.sh
```

With optional environment file:

```bash
cp examples/engine/local_validation.env.example local_validation.env
# Edit paths, then:
set -a && source ./local_validation.env && set +a && ./scripts/production_readiness.sh
```

## Clean rebuild

```bash
./scripts/compile_engine.sh clean vulkan
```
