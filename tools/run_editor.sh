#!/usr/bin/env bash
set -euo pipefail

# Launch the modern Qt editor by default. Pass --gtk to run the legacy GtkRadiant.
# Binaries are expected under build/radiant/install after running tools/compile_editor.sh.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
INSTALL_DIR="${ROOT_DIR}/build/radiant/install"

QT_BIN="${INSTALL_DIR}/radiant_qt"
GTK_BIN="${INSTALL_DIR}/radiant"

# Provide a default renderer path for the Qt viewport if available.
if [[ -z "${ENGINE_RENDERER_VK_LIB:-}" ]]; then
  CANDIDATES=(
    "${ROOT_DIR}/build/idtech3_vulkan_x86_64.so"
    "${ROOT_DIR}/release/idtech3_vulkan_x86_64.so"
    "${ROOT_DIR}/build/idtech3_vulkan.so"
    "${ROOT_DIR}/release/idtech3_vulkan.so"
  )
  for c in "${CANDIDATES[@]}"; do
    if [[ -f "$c" ]]; then
      export ENGINE_RENDERER_VK_LIB="$c"
      break
    fi
  done
fi

if [[ "${1:-}" == "--gtk" ]]; then
  shift
  if [[ ! -x "${GTK_BIN}" ]]; then
    echo "GtkRadiant binary not found at ${GTK_BIN}."
    echo "Rebuild with: tools/compile_editor.sh --gtk"
    exit 127
  fi
  exec "${GTK_BIN}" "$@"
fi

if [[ ! -x "${QT_BIN}" ]]; then
  echo "Qt editor binary not found at ${QT_BIN}."
  echo "Rebuild with: tools/compile_editor.sh"
  exit 127
fi

exec "${QT_BIN}" "$@"
