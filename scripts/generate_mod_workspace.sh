#!/usr/bin/env bash
# Generate VS Code launch/tasks for a mod (s&box SolutionGenerator analogue).
# Usage: ./scripts/generate_mod_workspace.sh <mod_dir> [engine_release_dir]
set -euo pipefail

MOD="${1:?mod directory required}"
ENGINE="${2:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/release}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MOD_ABS="$(cd "$MOD" && pwd)"

python3 - "$MOD_ABS" "$ENGINE" "$ROOT" <<'PY'
import json, pathlib, sys
mod, engine, root = sys.argv[1:4]
mod_path = pathlib.Path(mod)
ident = mod_path.name
startup = ""
scripts = []
proj = mod_path / "game.idproj"
if proj.is_file():
    d = json.loads(proj.read_text())
    ident = d.get("Ident", ident)
    startup = d.get("Metadata", {}).get("StartupMap", "")
    scripts = d.get("Scripts", [])

launch_args = ["+set", "fs_basepath", mod, "+set", "fs_game", ident, "+set", "com_scriptWatch", "1"]
if startup:
    launch_args += ["+map", startup]

vscode = mod_path / ".vscode"
vscode.mkdir(parents=True, exist_ok=True)

(vscode / "launch.json").write_text(json.dumps({
    "version": "0.2.0",
    "configurations": [{
        "name": f"idtech3 ({ident})",
        "type": "cppdbg",
        "request": "launch",
        "program": f"{engine}/idtech3",
        "args": launch_args,
        "cwd": mod,
    }],
}, indent=2) + "\n")

(vscode / "tasks.json").write_text(json.dumps({
    "version": "2.0.0",
    "tasks": [
        {"label": "bootstrap engine", "type": "shell", "command": f"{root}/scripts/bootstrap.sh engine", "options": {"cwd": root}},
        {"label": "watch scripts", "type": "shell", "command": f"{root}/scripts/watch_scripts.sh {mod}", "options": {"cwd": mod}},
    ],
}, indent=2) + "\n")

(vscode / "settings.json").write_text(json.dumps({
    "files.associations": {"*.idproj": "json"},
    "C_Cpp.default.includePath": [f"{root}/src", f"{root}/src/renderers/vulkan/shaders/glsl"],
}, indent=2) + "\n")

print(f"[workspace] wrote .vscode under {mod}")
PY
