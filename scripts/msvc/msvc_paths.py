#!/usr/bin/env python3
"""Map canonical repo-relative paths to MSVC vcxproj ClCompile paths (Phase 5d)."""
from __future__ import annotations

from pathlib import Path

# (resolved canonical prefix, vcxproj-relative directory from win32/msvc2017/)
_BRIDGE_RULES: list[tuple[str, str]] = [
    ("runtime/client/", r"..\..\client"),
    ("runtime/server/", r"..\..\server"),
    ("runtime/game/", r"..\..\game"),
    ("runtime/cgame/", r"..\..\cgame"),
    ("runtime/ui/", r"..\..\ui"),
    ("engine/core/", r"..\..\qcommon"),
    ("engine/asm/", r"..\..\asm"),
    ("modules/audio/", r"..\..\audio"),
    ("modules/physics/", r"..\..\..\physics"),
    ("modules/navigation/", r"..\..\..\navigation"),
    ("modules/world/", r"..\..\..\world"),
    ("modules/botlib/", r"..\..\..\botlib"),
    ("renderers/", r"..\..\..\renderers"),
    ("third_party/", r"..\..\..\external"),
    ("extensions/", r"..\..\..\extensions"),
]

# Legacy src/* shims (resolve same as canonical).
_SHIM_PREFIXES: list[tuple[str, str]] = [
    ("src/client/", "runtime/client/"),
    ("src/server/", "runtime/server/"),
    ("src/game/", "runtime/game/"),
    ("src/cgame/", "runtime/cgame/"),
    ("src/ui/", "runtime/ui/"),
    ("src/qcommon/", "engine/core/"),
    ("src/asm/", "engine/asm/"),
    ("src/audio/", "modules/audio/"),
    ("src/physics/", "modules/physics/"),
    ("src/navigation/", "modules/navigation/"),
    ("src/world/", "modules/world/"),
    ("src/botlib/", "modules/botlib/"),
    ("src/renderers/", "renderers/"),
    ("src/external/", "third_party/"),
    ("src/extensions/", "extensions/"),
]


def normalize_canonical(root: Path, rel: str) -> str:
    """Repo-relative POSIX path via physical layout (resolves symlinks)."""
    p = (root / rel).resolve()
    root_res = root.resolve()
    try:
        return p.relative_to(root_res).as_posix()
    except ValueError:
        return rel.replace("\\", "/")


def shim_to_canonical(rel: str) -> str:
    norm = rel.replace("\\", "/")
    for shim, canonical in _SHIM_PREFIXES:
        if norm.startswith(shim):
            return canonical + norm[len(shim) :]
    return norm


def canonical_to_vcxproj_rel(canonical_posix: str, *, depth3: bool = False) -> str | None:
    """Return vcxproj ClCompile path or None if unmappable."""
    canon = canonical_posix.replace("\\", "/")
    rules = sorted(_BRIDGE_RULES, key=lambda r: len(r[0]), reverse=True)
    for prefix, vcx_dir in rules:
        if not canon.startswith(prefix):
            continue
        tail = canon[len(prefix) :].replace("/", "\\")
        vcx_prefix = vcx_dir
        if depth3 and vcx_prefix.startswith(r"..\..") and not vcx_prefix.startswith(
            r"..\..\.."
        ):
            # vulkan.vcxproj uses ../../../renderers/...
            vcx_prefix = r"..\..\.." + vcx_prefix[5:]
        if tail:
            return f"{vcx_prefix}\\{tail}"
        return vcx_prefix
    return None


def manifest_entry_to_vcxproj(root: Path, manifest_rel: str, *, depth3: bool = False) -> str | None:
    canon = shim_to_canonical(normalize_canonical(root, manifest_rel))
    return canonical_to_vcxproj_rel(canon, depth3=depth3)
