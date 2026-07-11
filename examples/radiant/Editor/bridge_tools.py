#!/usr/bin/env python3
"""
Editor-only helpers (s&box Editor/Assembly.cs analogue).
Run from idTech3Radiant Python Script Editor or: python3 Editor/bridge_tools.py

Requires RADIANT_* env vars (set automatically by Radiant) or a mod with game.idproj.
"""
from __future__ import annotations

import json
import os
import pathlib
import subprocess
import sys

# Allow `import radiant` when run from mod Editor/ folder
_scripts = os.environ.get("RADIANT_APP_PATH", "")
if _scripts:
    _p = os.path.join(_scripts, "scripts")
    if _p not in sys.path:
        sys.path.insert(0, _p)

try:
    import radiant  # type: ignore
except ImportError:
    radiant = None  # type: ignore


def _game_path() -> pathlib.Path:
    if radiant:
        p = radiant.game_path()
        if p:
            return pathlib.Path(p)
    env = os.environ.get("RADIANT_GAME_PATH") or os.environ.get("IDTECH3_MOD_PATH", "")
    if env:
        return pathlib.Path(env)
    return pathlib.Path(".")


def load_idproj() -> dict:
    """Load game.idproj from mod root (s&box .sbproj analogue)."""
    path = _game_path() / "game.idproj"
    if not path.is_file():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def idproj_ident() -> str:
    data = load_idproj()
    return str(data.get("Ident", _game_path().name))


def startup_map() -> str:
    data = load_idproj()
    meta = data.get("Metadata") or {}
    return str(meta.get("StartupMap", ""))


def engine_binary() -> pathlib.Path:
    data = load_idproj()
    rad = data.get("Radiant") or {}
    if rad.get("EngineBinary"):
        return pathlib.Path(str(rad["EngineBinary"]))
    if radiant:
        ep = radiant.engine_path()
        if ep:
            return pathlib.Path(ep) / "idtech3"
    release = os.environ.get("IDTECH3_ENGINE_RELEASE", "")
    if release:
        return pathlib.Path(release) / "idtech3"
    return pathlib.Path("idtech3")


def launch_engine(map_name: str | None = None) -> int:
    """Launch idtech3 with fs_game from game.idproj (+ optional +map)."""
    ident = idproj_ident()
    game = _game_path()
    exe = engine_binary()
    if not exe.is_file():
        print(f"[bridge] engine binary not found: {exe}", file=sys.stderr)
        return 1
    args = [
        str(exe),
        "+set", "fs_basepath", str(game),
        "+set", "fs_game", ident,
        "+set", "r_studio_tools", "1",
        "+set", "com_scriptWatch", "1",
    ]
    m = map_name or startup_map() or (radiant.current_map().replace(".map", "") if radiant else "")
    if m:
        args += ["+map", m.replace(".bsp", "").replace(".map", "")]
    print("[bridge] exec:", " ".join(args))
    return subprocess.call(args, cwd=str(game))


def import_studio_export(cfg_path: pathlib.Path | None = None) -> str:
    """
    Read studio_exportents.cfg (from in-engine Studio Entities panel) and return
    a Radiant-pasteable entity block string.
    """
    game = _game_path()
    path = cfg_path or (game / "studio_exportents.cfg")
    if not path.is_file():
        return f"// not found: {path}\n"
    text = path.read_text(encoding="utf-8", errors="replace").strip()
    if not text.startswith("{"):
        return f"// invalid snippet in {path}\n"
    return text + "\n"


def paint_sidecar_path(map_name: str | None = None) -> pathlib.Path:
    """Path to maps/<map>.paint next to the mod (material-blend weight sidecar)."""
    game = _game_path()
    m = map_name or startup_map() or "unknown"
    m = m.replace(".bsp", "").replace(".map", "")
    if "/" in m:
        m = m.split("/")[-1]
    return game / "maps" / f"{m}.paint"


def export_paint_sidecar(dest: pathlib.Path | None = None, map_name: str | None = None) -> pathlib.Path | None:
    """
    Copy maps/<map>.paint from the mod into dest (default: alongside current .map).
    Engine Studio paint_save writes the sidecar; Radiant only relocates/archives it.
    """
    src = paint_sidecar_path(map_name)
    if not src.is_file():
        print(f"[bridge] paint sidecar not found: {src}", file=sys.stderr)
        return None
    out = dest or (src.parent / (src.stem + "_export.paint"))
    out.write_bytes(src.read_bytes())
    print(f"[bridge] exported paint: {src} -> {out}")
    return out


def import_paint_sidecar(src: pathlib.Path, map_name: str | None = None) -> pathlib.Path | None:
    """Install a .paint file as maps/<map>.paint for the engine to load on map start."""
    if not src.is_file():
        print(f"[bridge] import source missing: {src}", file=sys.stderr)
        return None
    dest = paint_sidecar_path(map_name)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(src.read_bytes())
    print(f"[bridge] imported paint: {src} -> {dest}")
    print("[bridge] note: q3map2 vertex light can clobber BSP colors; .paint is source of truth for materialBlend weights")
    return dest


def main() -> int:
    print("=== idTech3 Radiant bridge (Source-2 EditorScripts pattern) ===")
    print("  game_path:", _game_path())
    print("  ident:", idproj_ident())
    print("  startup_map:", startup_map() or "(unset)")
    print("  engine:", engine_binary())
    print("  paint_sidecar:", paint_sidecar_path())
    proj = load_idproj()
    if proj:
        scripts = proj.get("EditorScripts") or []
        print("  EditorScripts:", scripts)
    snippet = import_studio_export()
    if snippet.startswith("{"):
        print("\n--- studio_exportents.cfg (paste into Radiant) ---")
        print(snippet)
    if len(sys.argv) > 1 and sys.argv[1] == "export-paint":
        export_paint_sidecar(pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else None)
    elif len(sys.argv) > 2 and sys.argv[1] == "import-paint":
        import_paint_sidecar(pathlib.Path(sys.argv[2]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
