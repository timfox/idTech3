#!/usr/bin/env bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 id Tech 3 contributors
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="$REPO_ROOT/release"
CONTENT_DIR="$REPO_ROOT/content"

BUILD_TYPE="Release"
RENDERER="vulkan"
SKIP_BUILD=0
QUIET=0
CLEAN_BUILD=0

usage() {
  cat <<'EOF'
Usage: ./scripts/preflight_checks.sh [options]

Options:
  --build-type=<Release|Debug>   Build configuration to pass to compile_engine.sh (default: Release)
  --renderer=<vulkan|opengl>     Renderer backend (default: vulkan)
  --clean-build                  Clean the existing build directory before building
  --skip-build                   Skip the engine build, only run lint and license checks
  --quiet                        Silence the engine build output
  --help                         Show this message
EOF
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "preflight: missing required command '$1' in PATH"
    exit 1
  fi
}

parse_args() {
  while [ "$#" -gt 0 ]; do
    case "$1" in
      --build-type=*)
        BUILD_TYPE="${1#*=}"
        shift
        ;;
      --renderer=*)
        RENDERER="${1#*=}"
        shift
        ;;
      --clean-build)
        CLEAN_BUILD=1
        shift
        ;;
      --skip-build)
        SKIP_BUILD=1
        shift
        ;;
      --quiet)
        QUIET=1
        shift
        ;;
      --help|-h)
        usage
        exit 0
        ;;
      *)
        echo "preflight: unknown option '$1'"
        usage
        exit 1
        ;;
    esac
  done
}

run_git_diff_check() {
  echo "preflight: checking whitespace/format (git diff --check)"
  git -C "$REPO_ROOT" diff --check --quiet --
  echo "preflight: format/whitespace check passed"
}

run_static_analysis() {
  require_cmd cppcheck
  echo "preflight: running cppcheck static analysis"
  cppcheck \
    --enable=warning \
    --error-exitcode=1 \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    "$REPO_ROOT/src" "$REPO_ROOT/src/common" "$REPO_ROOT/src/renderers" "$REPO_ROOT/src/game" "$REPO_ROOT/src/qcommon"
  echo "preflight: cppcheck finished"
}

run_spdx_scan() {
  require_cmd python3
  echo "preflight: running SPDX/license check"
  python3 "$SCRIPT_DIR/spdx_scan.py" --path "$REPO_ROOT"
}

check_large_binaries() {
  local threshold="+100M"
  local found=()
  for target in "$RELEASE_DIR" "$CONTENT_DIR"; do
    if [ -d "$target" ]; then
      while IFS= read -r -d '' file; do
        found+=("$file")
      done < <(find "$target" -type f -size "$threshold" -print0)
    fi
  done

  if [ "${#found[@]}" -gt 0 ]; then
    echo "preflight: large binary threshold exceeded (files >100MB)"
    printf '  %s\n' "${found[@]}"
    exit 1
  fi
  echo "preflight: large file audit passed"
}

run_engine_build() {
  local args=("$BUILD_TYPE" "$RENDERER")
  if [ "$CLEAN_BUILD" -eq 1 ]; then
    args+=("clean")
  fi
  if [ "$QUIET" -eq 1 ]; then
    args+=("quiet")
  fi
  args+=("skip-idpak-check")

  echo "preflight: invoking ./scripts/compile_engine.sh ${args[*]}"
  "$SCRIPT_DIR/compile_engine.sh" "${args[@]}"
}

main() {
  parse_args "$@"

  require_cmd git
  require_cmd find

  run_git_diff_check
  run_static_analysis
  run_spdx_scan
  check_large_binaries

  if [ "$SKIP_BUILD" -eq 0 ]; then
    run_engine_build
  else
    echo "preflight: build step skipped (--skip-build)"
  fi

  echo "preflight: all checks passed"
}

main "$@"
