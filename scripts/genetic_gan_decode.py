#!/usr/bin/env python3
"""
idTech3 wrapper: genome latent JSON -> body mesh (GLB).

The engine writes a genome vector (see docs/GENETIC_GAN.md). This script either:
  1. Delegates to $REPO/decode_genome.py when cl_geneticGanRepo is set, or
  2. Emits a minimal placeholder GLB scaled by the first latent gene (no GPU).
"""

from __future__ import annotations

import argparse
import json
import os
import struct
import subprocess
import sys


def _fail(msg: str) -> None:
    print(f"GeneticGAN wrapper: {msg}", file=sys.stderr)
    sys.exit(1)


def _write_minimal_glb(path: str, scale: float) -> None:
    """Single-triangle GLB placeholder (glTF 2.0) for pipeline smoke tests."""
    s = max(0.25, min(2.0, scale))
    # positions: scaled triangle on XZ plane
    positions = [
        -0.5 * s, 0.0, -0.5 * s,
        0.5 * s, 0.0, -0.5 * s,
        0.0, 0.0, 0.5 * s,
    ]
    bin_blob = struct.pack("<9f", *positions)
    gltf = {
        "asset": {"version": "2.0", "generator": "idtech3-genetic-gan-stub"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{
            "primitives": [{
                "attributes": {"POSITION": 0},
                "mode": 4,
            }]
        }],
        "accessors": [{
            "bufferView": 0,
            "componentType": 5126,
            "count": 3,
            "type": "VEC3",
            "min": [-0.5 * s, 0.0, -0.5 * s],
            "max": [0.5 * s, 0.0, 0.5 * s],
        }],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": len(bin_blob)}],
        "buffers": [{"byteLength": len(bin_blob)}],
    }
    json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    json_pad = (4 - (len(json_bytes) % 4)) % 4
    json_bytes += b" " * json_pad
    bin_pad = (4 - (len(bin_blob) % 4)) % 4
    bin_blob += b"\x00" * bin_pad

    total = 12 + 8 + len(json_bytes) + 8 + len(bin_blob)
    header = struct.pack("<4sII", b"glTF", 2, total)
    json_chunk = struct.pack("<I4s", len(json_bytes), b"JSON") + json_bytes
    bin_chunk = struct.pack("<I4s", len(bin_blob), b"BIN\x00") + bin_blob

    out_dir = os.path.dirname(os.path.abspath(path))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(path, "wb") as f:
        f.write(header + json_chunk + bin_chunk)


def main() -> None:
    ap = argparse.ArgumentParser(description="Genome latent -> GLB for idTech3")
    ap.add_argument("--repo", default="", help="Optional GAN checkout (decode_genome.py)")
    ap.add_argument("--genome", required=True, help="Genome JSON from engine")
    ap.add_argument("--output", required=True, help="Output .glb path")
    ap.add_argument("--slot", type=int, default=0, help="Genome slot id")
    args = ap.parse_args()

    if not os.path.isfile(args.genome):
        _fail(f"genome not found: {args.genome}")

    with open(args.genome, encoding="utf-8") as f:
        genome = json.load(f)

    genes = genome.get("genes") or []
    scale = float(genes[0]) if genes else 1.0

    repo = os.path.abspath(args.repo) if args.repo else ""
    custom = os.path.join(repo, "decode_genome.py") if repo else ""
    if custom and os.path.isfile(custom):
        print(f"GeneticGAN wrapper: delegating to {custom}")
        subprocess.check_call(
            [sys.executable, custom, args.genome, args.output, str(args.slot)],
            cwd=repo or None,
        )
        if not os.path.isfile(args.output):
            _fail("decode_genome.py did not write output")
        print(f"GeneticGAN wrapper: wrote {args.output}")
        return

    print(f"GeneticGAN wrapper: stub GLB (scale={scale:.3f}) -> {args.output}")
    _write_minimal_glb(args.output, scale)
    print(f"GeneticGAN wrapper: wrote placeholder mesh for slot {args.slot}")


if __name__ == "__main__":
    main()
