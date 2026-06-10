#!/usr/bin/env bash
# s&box-style staged bootstrap (see https://github.com/timfox/Source-2 Bootstrap.bat).
# Usage: ./scripts/bootstrap.sh [all|engine|shaders|content] [compile_engine.sh args...]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE="${1:-all}"
shift || true

run_engine() {
  echo "[bootstrap] stage=engine (Vulkan Release)"
  bash "${ROOT}/scripts/compile_engine.sh" vulkan "$@"
}

run_shaders() {
  echo "[bootstrap] stage=shaders"
  if [[ -x "${ROOT}/scripts/compile_shaders.sh" ]]; then
    bash "${ROOT}/scripts/compile_shaders.sh"
  else
    echo "[bootstrap] compile_shaders.sh missing; run engine build first"
    exit 1
  fi
}

run_content() {
  echo "[bootstrap] stage=content (demo pk3 + release staging)"
  bash "${ROOT}/scripts/compile_engine.sh" vulkan demo "$@"
  if [[ -d "${ROOT}/release/demo_game" ]]; then
    echo "[bootstrap] demo_game staged under release/demo_game/"
  fi
}

run_radiant() {
  echo "[bootstrap] stage=radiant (gamepack into demo mod if present)"
  local demo="${ROOT}/release/demo_game/mod"
  if [[ -d "$demo" ]]; then
    bash "${ROOT}/scripts/install_radiant_gamepack.sh" "$demo" "${ROOT}/release"
  else
    echo "[bootstrap] no release/demo_game/mod — skip (run: new_mod_from_template + install_radiant_gamepack)"
  fi
}

case "$STAGE" in
  engine)
    run_engine "$@"
    ;;
  shaders)
    run_shaders
    ;;
  content)
    run_content "$@"
    ;;
  radiant)
    run_radiant
    ;;
  all)
    run_engine "$@"
    run_shaders
    run_content "$@"
    run_radiant
    ;;
  help|-h|--help)
    cat <<'EOF'
Usage: ./scripts/bootstrap.sh [all|engine|shaders|content|radiant] [extra compile_engine args]

  engine   — build Vulkan client/server/renderer (default: vulkan Release)
  shaders  — regenerate Vulkan SPIR-V (compile_shaders.sh)
  content  — build engine + idtech3_demo.pk3 into release/demo_game/
  radiant  — install examples/radiant gamepack into demo mod (if present)
  all      — engine, shaders, content, radiant (default)

Mirrors s&box Bootstrap.bat: build → build-shaders → build-content.
EOF
    ;;
  *)
    echo "Unknown stage: $STAGE (use all|engine|shaders|content|radiant)" >&2
    exit 1
    ;;
esac

echo "[bootstrap] OK ($STAGE)"
