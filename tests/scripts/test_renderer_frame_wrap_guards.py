#!/usr/bin/env python3
"""Regression guards for renderer model frame wrapping.

These source-level checks protect the crash fix that made MD3/MDR/IQM
front-end frame wrapping validate the model frame count before modulo and
before frame-indexed culling/drawing work.  The renderer copies are duplicated
between src/renderers and src/platform/renderers, so keep both trees covered.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def strip_comments_and_strings(source: str) -> str:
    """Return source with comments/strings replaced by spaces for brace scans."""

    out: list[str] = []
    i = 0
    state = "code"
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""

        if state == "code":
            if ch == "/" and nxt == "/":
                out.extend("  ")
                i += 2
                state = "line_comment"
                continue
            if ch == "/" and nxt == "*":
                out.extend("  ")
                i += 2
                state = "block_comment"
                continue
            if ch == '"':
                out.append(" ")
                i += 1
                state = "string"
                continue
            if ch == "'":
                out.append(" ")
                i += 1
                state = "char"
                continue
            out.append(ch)
            i += 1
            continue

        if state == "line_comment":
            out.append("\n" if ch == "\n" else " ")
            i += 1
            if ch == "\n":
                state = "code"
            continue

        if state == "block_comment":
            if ch == "*" and nxt == "/":
                out.extend("  ")
                i += 2
                state = "code"
            else:
                out.append("\n" if ch == "\n" else " ")
                i += 1
            continue

        if state == "string":
            if ch == "\\" and nxt:
                out.extend("  ")
                i += 2
            else:
                out.append("\n" if ch == "\n" else " ")
                i += 1
                if ch == '"':
                    state = "code"
            continue

        if state == "char":
            if ch == "\\" and nxt:
                out.extend("  ")
                i += 2
            else:
                out.append("\n" if ch == "\n" else " ")
                i += 1
                if ch == "'":
                    state = "code"
            continue

    return "".join(out)


def read_source(path: Path) -> str:
    if not path.is_file():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8")


def function_body(source: str, name: str, path: Path) -> str:
    scrubbed = strip_comments_and_strings(source)
    signature = re.compile(
        rf"(?m)^[ \t]*(?:static[ \t]+)?[A-Za-z_][A-Za-z0-9_ \t\*]*[ \t]+"
        rf"{re.escape(name)}[ \t]*\([^;{{]*\)[ \t]*\{{"
    )
    match = signature.search(scrubbed)
    if not match:
        fail(f"{path}: could not find function {name}")

    open_brace = scrubbed.find("{", match.start(), match.end())
    depth = 0
    for idx in range(open_brace, len(scrubbed)):
        if scrubbed[idx] == "{":
            depth += 1
        elif scrubbed[idx] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace : idx + 1]

    fail(f"{path}: function {name} body is unterminated")


def compact(source: str) -> str:
    return re.sub(r"\s+", " ", strip_comments_and_strings(source)).strip()


def require_regex(body: str, pattern: str, context: str) -> None:
    if not re.search(pattern, compact(body)):
        fail(f"{context}: expected pattern {pattern!r}")


def require_order(body: str, needles: list[str], context: str) -> None:
    text = compact(body)
    last = -1
    for needle in needles:
        pos = text.find(needle)
        if pos < 0:
            fail(f"{context}: missing {needle!r}")
        if pos <= last:
            fail(f"{context}: {needle!r} appears out of order")
        last = pos


def check_md3(path: Path) -> None:
    body = function_body(read_source(path), "R_AddMD3Surfaces", path)
    context = f"{path}: R_AddMD3Surfaces"

    require_regex(
        body,
        r"if \( !tr\.currentModel->md3\[0\] \|\| tr\.currentModel->md3\[0\]->numFrames < 1 \) \{ return; \}",
        context,
    )
    require_regex(
        body,
        r"if \( ent->e\.renderfx & RF_WRAP_FRAMES \) \{ const int nf = tr\.currentModel->md3\[0\]->numFrames; "
        r"ent->e\.frame %= nf; ent->e\.oldframe %= nf; \}",
        context,
    )
    require_regex(
        body,
        r"ent->e\.frame = 0; ent->e\.oldframe = 0;",
        context,
    )
    require_order(
        body,
        [
            "numFrames < 1",
            "RF_WRAP_FRAMES",
            "ent->e.frame %= nf;",
            "ent->e.oldframe %= nf;",
            "ent->e.frame >= tr.currentModel->md3[0]->numFrames",
            "ent->e.frame = 0;",
            "ent->e.oldframe = 0;",
            "lod = R_ComputeLOD( ent );",
            "cull = R_CullModel( header, ent, bounds );",
        ],
        context,
    )


def check_mdr(path: Path) -> None:
    body = function_body(read_source(path), "R_MDRAddAnimSurfaces", path)
    context = f"{path}: R_MDRAddAnimSurfaces"

    require_regex(
        body,
        r"if \( !header \|\| header->numFrames < 1 \) \{ return; \}",
        context,
    )
    require_regex(
        body,
        r"if \( ent->e\.renderfx & RF_WRAP_FRAMES \) \{ const int nf = \(int\)header->numFrames; "
        r"ent->e\.frame %= nf; ent->e\.oldframe %= nf; \}",
        context,
    )
    require_regex(
        body,
        r"ent->e\.frame = 0; ent->e\.oldframe = 0;",
        context,
    )
    require_order(
        body,
        [
            "header->numFrames < 1",
            "RF_WRAP_FRAMES",
            "ent->e.frame %= nf;",
            "ent->e.oldframe %= nf;",
            "ent->e.frame >= header->numFrames",
            "ent->e.frame = 0;",
            "ent->e.oldframe = 0;",
            "cull = R_MDRCullModel (header, ent);",
        ],
        context,
    )


def check_iqm_frontend(path: Path) -> None:
    body = function_body(read_source(path), "R_AddIQMSurfaces", path)
    context = f"{path}: R_AddIQMSurfaces"

    require_regex(
        body,
        r"if \( ent->e\.renderfx & RF_WRAP_FRAMES && data->num_frames > 0 \) \{ "
        r"ent->e\.frame %= data->num_frames; ent->e\.oldframe %= data->num_frames; \}",
        context,
    )
    require_regex(
        body,
        r"ent->e\.frame = 0; ent->e\.oldframe = 0;",
        context,
    )
    require_order(
        body,
        [
            "RF_WRAP_FRAMES && data->num_frames > 0",
            "ent->e.frame %= data->num_frames;",
            "ent->e.oldframe %= data->num_frames;",
            "ent->e.frame >= data->num_frames",
            "ent->e.frame = 0;",
            "ent->e.oldframe = 0;",
            "cull = R_CullIQM ( data, ent );",
        ],
        context,
    )


def check_iqm_backend(path: Path) -> None:
    body = function_body(read_source(path), "RB_IQMSurfaceAnim", path)
    context = f"{path}: RB_IQMSurfaceAnim"

    require_regex(
        body,
        r"int frame = data->num_frames \? backEnd\.currentEntity->e\.frame % data->num_frames : 0;",
        context,
    )
    require_regex(
        body,
        r"int oldframe = data->num_frames \? backEnd\.currentEntity->e\.oldframe % data->num_frames : 0;",
        context,
    )
    require_order(
        body,
        [
            "data->num_frames ? backEnd.currentEntity->e.frame % data->num_frames : 0;",
            "data->num_frames ? backEnd.currentEntity->e.oldframe % data->num_frames : 0;",
            "ComputePoseMats( data, frame, oldframe",
        ],
        context,
    )


def main() -> None:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]

    renderer_roots = [
        root / "src/renderers",
        root / "src/platform/renderers",
    ]
    for renderer_root in renderer_roots:
        for renderer in ("opengl", "vulkan"):
            check_md3(renderer_root / renderer / "tr_mesh.c")
            check_mdr(renderer_root / renderer / "tr_animation.c")
            iqm_path = renderer_root / renderer / "tr_model_iqm.c"
            check_iqm_frontend(iqm_path)
            check_iqm_backend(iqm_path)

    print("PASS: test_renderer_frame_wrap_guards")


if __name__ == "__main__":
    main()
