#!/usr/bin/env bash
# Install a relocatable, project-oriented engine + project-authoring SDK.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${HOME}/.local"
DESTDIR=""
SDK_ONLY=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix) PREFIX="${2:?--prefix needs a path}"; shift 2 ;;
    --destdir) DESTDIR="${2:?--destdir needs a path}"; shift 2 ;;
    --sdk-only) SDK_ONLY=1; shift ;;
    -h|--help) echo "Usage: $0 [--prefix PATH] [--destdir PATH] [--sdk-only]"; exit 0 ;;
    *) echo "install_engine: unknown option: $1" >&2; exit 2 ;;
  esac
done

install_root="${DESTDIR}${PREFIX}"
bindir="$install_root/bin"
sdkdir="$install_root/share/idtech3/sdk"
appdir="$install_root/share/applications"
manifest="$install_root/share/idtech3/install-manifest.txt"
mkdir -p "$bindir" "$sdkdir/scripts" "$sdkdir/examples/templates" "$sdkdir/config" "$sdkdir/docs" "$appdir" "$(dirname "$manifest")"

files=()
put() { install -m "$1" "$2" "$3"; files+=("${3#$DESTDIR}"); }
put 0755 "$ROOT/scripts/idtech3-engine" "$bindir/idtech3-engine"
for script in create_starter_game.sh new_mod_from_template.sh generate_mod_workspace.sh asset_pipeline.sh validate_assets.sh compile_shaders.sh; do
  put 0755 "$ROOT/scripts/$script" "$sdkdir/scripts/$script"
done
cp -a "$ROOT/examples/templates/." "$sdkdir/examples/templates/"
cp -a "$ROOT/config/." "$sdkdir/config/"
put 0644 "$ROOT/docs/ENGINE_SDK_QUICKSTART.md" "$sdkdir/docs/ENGINE_SDK_QUICKSTART.md"

if [[ $SDK_ONLY -eq 0 ]]; then
  for binary in idtech3 idtech3_server; do
    [[ -x "$ROOT/release/$binary" ]] || { echo "install_engine: release/$binary missing; build first or use --sdk-only" >&2; exit 1; }
    put 0755 "$ROOT/release/$binary" "$bindir/$binary"
  done
  shopt -s nullglob
  # Renderer plugins deliberately stay beside the client, matching release archives
  # and the engine's default dynamic-library lookup contract.
  for plugin in "$ROOT"/release/idtech3_*.so; do put 0755 "$plugin" "$bindir/$(basename "$plugin")"; done
fi

cat > "$appdir/idtech3-editor.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=idTech3 Editor
Comment=Create and test an idTech3 game project
Exec=${PREFIX}/bin/idtech3-engine editor
Terminal=false
Categories=Development;Game;
EOF
files+=("$PREFIX/share/applications/idtech3-editor.desktop")
printf '%s\n' "${files[@]}" > "$manifest"
echo "Installed idTech3 Engine SDK in $install_root"
echo "Add $PREFIX/bin to PATH, then run: idtech3-engine doctor"
