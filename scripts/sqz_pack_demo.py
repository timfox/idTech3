#!/usr/bin/env python3
"""Pack a minimal SqueezeMe demo .sqz blob (procedural-compatible layout)."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

SQZ_MAGIC = b"SQZ1"
SQZ_VERSION = 1
GCS = 64
CELLS = GCS * GCS
CHANNELS = 16
POSE_DIM = 128
POSE_BASIS = 64
JOINTS = 12
FLAGS_LINEAR = 1
FLAGS_GCS = 2


def main() -> int:
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "examples/demo_game/mod/sqz/demo.sqz")
    out.parent.mkdir(parents=True, exist_ok=True)

    pose_mean = [0.0] * POSE_DIM
    pose_basis = [0.0] * (POSE_DIM * POSE_BASIS)
    for i in range(min(POSE_DIM, POSE_BASIS)):
        pose_basis[i * POSE_BASIS + i] = 1.0

    correctives = [0.0] * ((POSE_BASIS + 1) * CELLS * CHANNELS)
    template = [0.0] * (CELLS * CHANNELS)
    mask = bytearray(CELLS)
    weights = [0.0] * (CELLS * 24)
    bind = [0.0] * (CELLS * 3)

    for cy in range(GCS):
        for cx in range(GCS):
            idx = cy * GCS + cx
            u = (cx + 0.5) / GCS
            v = (cy + 0.5) / GCS
            dx = u - 0.5
            dy = v - 0.25
            body = 1.0 - (dx * dx * 4.0 + dy * dy * 1.5)
            bind[idx * 3 + 0] = dx * 0.6
            bind[idx * 3 + 1] = dy * 1.75
            bind[idx * 3 + 2] = 0.0
            mask[idx] = 1 if body > 0.15 else 0
            weights[idx * 24] = 1.0
            base = idx * CHANNELS
            template[base + 8] = 4.0
            template[base + 9] = 3.0
            template[base + 10] = 5.0
            template[base + 11] = 2.5
            template[base + 12] = 0.65
            template[base + 13] = 0.5 + 0.4 * u
            template[base + 14] = 0.55 + 0.35 * v
            template[base + 15] = 0.6

    hdr = struct.pack(
        "<4s7I",
        SQZ_MAGIC,
        SQZ_VERSION,
        FLAGS_LINEAR | FLAGS_GCS,
        JOINTS,
        1024,
        GCS,
        POSE_DIM,
        POSE_BASIS,
    )
    blob = hdr
    blob += struct.pack(f"<{POSE_DIM}f", *pose_mean)
    blob += struct.pack(f"<{POSE_DIM * POSE_BASIS}f", *pose_basis)
    blob += struct.pack(f"<{len(correctives)}f", *correctives)
    blob += struct.pack(f"<{len(template)}f", *template)
    blob += bytes(mask)
    blob += struct.pack(f"<{len(weights)}f", *weights)
    blob += struct.pack(f"<{len(bind)}f", *bind)

    out.write_bytes(blob)
    print(f"wrote {out} ({len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
