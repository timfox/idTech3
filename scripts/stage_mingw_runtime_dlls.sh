#!/usr/bin/env bash
# Stage MinGW/MSYS2 runtime DLLs next to PE executables so a copied bin/ folder runs
# outside the MSYS2 shell (GitHub Actions artifacts, zip releases).
#
# Usage (from MINGW64 shell, after copying idtech3*.exe into bin/):
#   ./scripts/stage_mingw_runtime_dlls.sh [bin_dir]
#
# Requires: objdump from the same MinGW toolchain (usually on PATH in MINGW64).
set -euo pipefail

BIN_DIR="${1:-bin}"
PREFIX="${MINGW_PREFIX:-/mingw64}"
MINGW_BIN="$PREFIX/bin"

if [[ ! -d "$BIN_DIR" ]]; then
  echo "Error: directory not found: $BIN_DIR" >&2
  exit 1
fi

if ! command -v objdump &>/dev/null; then
  echo "Error: objdump not found (use MINGW64 shell / MinGW toolchain on PATH)" >&2
  exit 1
fi

shopt -s nullglob
# Avoid mapfile (not all MSYS2 bash builds ship with bash 4+ mapfile built-in)
PE_FILES=()
while IFS= read -r _pef; do
  [[ -n "$_pef" ]] && PE_FILES+=( "$_pef" )
done < <(find "$BIN_DIR" -maxdepth 1 \( -name '*.exe' -o -name '*.dll' \) -type f 2>/dev/null | sort -u)

if [[ ${#PE_FILES[@]} -eq 0 ]]; then
  echo "No .exe/.dll under $BIN_DIR - nothing to stage"
  exit 0
fi

stage_one() {
  local dll="$1"
  local src="$MINGW_BIN/$dll"
  local dst="$BIN_DIR/$dll"

  [[ -n "$dll" ]] || return 0
  [[ -f "$dst" ]] && return 0
  if [[ -f "$src" ]]; then
    cp -f "$src" "$dst"
    echo "  staged: $dll"
    return 0
  fi
  return 0
}

echo "Staging MinGW runtime DLLs into $BIN_DIR (from $MINGW_BIN) ..."
changed=1
round=0
while [[ "$changed" -eq 1 ]]; do
  changed=0
  round=$((round + 1))
  if [[ "$round" -gt 40 ]]; then
    echo "Error: too many dependency rounds (cycle or huge tree?)" >&2
    exit 1
  fi
  for f in "${PE_FILES[@]}"; do
    [[ -f "$f" ]] || continue
    while IFS= read -r dll; do
      [[ -z "$dll" ]] && continue
      dst="$BIN_DIR/$dll"
      if [[ -f "$MINGW_BIN/$dll" && ! -f "$dst" ]]; then
        stage_one "$dll"
        changed=1
      fi
    done < <(objdump -p "$f" 2>/dev/null | sed -n 's/.*DLL Name: \(.*\)/\1/p')
  done
  # Refresh PE list so newly staged DLLs are scanned next round
  PE_FILES=()
  while IFS= read -r _pef; do
    [[ -n "$_pef" ]] && PE_FILES+=( "$_pef" )
  done < <(find "$BIN_DIR" -maxdepth 1 \( -name '*.exe' -o -name '*.dll' \) -type f 2>/dev/null | sort -u)
done

echo "Done."
