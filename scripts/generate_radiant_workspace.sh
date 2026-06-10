#!/usr/bin/env bash
# VS Code workspace for mod + idTech3Radiant (s&box SolutionGenerator + Hammer workflow).
# Usage: ./scripts/generate_radiant_workspace.sh <mod_dir> [engine_release] [radiant_path]
set -euo pipefail

MOD="${1:?mod directory required}"
ENGINE="${2:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/release}"
RADIANT="${3:-${RADIANT_PATH:-}}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MOD_ABS="$(cd "$MOD" && pwd)"

python3 - "$MOD_ABS" "$ENGINE" "$RADIANT" "$ROOT" <<'PY'
import json, pathlib, sys
mod, engine, radiant, root = sys.argv[1:5]
mod_path = pathlib.Path(mod)
ident = mod_path.name
startup = ""
editor_scripts = []
proj = mod_path / "game.idproj"
if proj.is_file():
    d = json.loads(proj.read_text())
    ident = d.get("Ident", ident)
    startup = (d.get("Metadata") or {}).get("StartupMap", "")
    editor_scripts = d.get("EditorScripts") or []

launch_args = ["+set", "fs_basepath", mod, "+set", "fs_game", ident,
               "+set", "r_studio_tools", "1", "+set", "com_scriptWatch", "1"]
if startup:
    launch_args += ["+map", startup]

vscode = mod_path / ".vscode"
vscode.mkdir(parents=True, exist_ok=True)

configs = [{
    "name": f"idtech3 ({ident})",
    "type": "cppdbg",
    "request": "launch",
    "program": f"{engine}/idtech3",
    "args": launch_args,
    "cwd": mod,
}]
if radiant:
    configs.append({
        "name": f"idTech3Radiant ({ident})",
        "type": "cppdbg",
        "request": "launch",
        "program": f"{radiant}/radiant.x86_64" if pathlib.Path(f"{radiant}/radiant.x86_64").exists() else f"{radiant}/radiant",
        "args": ["-game", "idtech3"],
        "cwd": mod,
    })

(vscode / "launch.json").write_text(json.dumps({"version": "0.2.0", "configurations": configs}, indent=2) + "\n")

tasks = [
    {"label": "bootstrap engine", "type": "shell", "command": f"{root}/scripts/bootstrap.sh engine", "options": {"cwd": root}},
    {"label": "install radiant gamepack", "type": "shell", "command": f"{root}/scripts/install_radiant_gamepack.sh {mod} {engine}", "options": {"cwd": root}},
    {"label": "watch studio export", "type": "shell", "command": f"python3 Editor/watch_studio_export.py", "options": {"cwd": mod}},
]
for script in editor_scripts:
    tasks.append({
        "label": f"radiant: {pathlib.Path(script).name}",
        "type": "shell",
        "command": f"python3 {script}",
        "options": {"cwd": mod},
    })

(vscode / "tasks.json").write_text(json.dumps({"version": "2.0.0", "tasks": tasks}, indent=2) + "\n")

(vscode / "settings.json").write_text(json.dumps({
    "files.associations": {"*.idproj": "json", "*.def": "cpp", "*.map": "xml"},
    "python.analysis.extraPaths": ["Editor"],
}, indent=2) + "\n")

print(f"[radiant-workspace] wrote .vscode under {mod}")
PY
