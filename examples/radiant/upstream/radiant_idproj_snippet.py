# Optional: merge into idTech3Radiant setup/data/tools/scripts/radiant/__init__.py
# Adds game.idproj helpers (s&box .sbproj analogue). Engine ships this for upstream PR.

def idproj_path() -> str:
    explicit = _env_path("RADIANT_IDPROJ_PATH", "")
    if explicit:
        return explicit
    gp = game_path()
    if gp:
        candidate = os.path.join(gp, "game.idproj")
        if os.path.isfile(candidate):
            return candidate
    return ""

def load_idproj() -> dict:
    path = idproj_path()
    if not path or not os.path.isfile(path):
        return {}
    try:
        import json
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}

def idproj_ident(default: str = "") -> str:
    data = load_idproj()
    return str(data.get("Ident", default or os.path.basename(game_path() or "")))

def startup_map() -> str:
    data = load_idproj()
    meta = data.get("Metadata") or {}
    return str(meta.get("StartupMap", ""))
