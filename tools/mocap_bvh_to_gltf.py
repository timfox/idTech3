#!/usr/bin/env python3
"""BVH -> glTF animation clip stub (offline ingest). Requires pygltflib for full export."""
import argparse
import sys

def main() -> int:
    p = argparse.ArgumentParser(description="Convert BVH mocap to glTF animation (v1 scaffold)")
    p.add_argument("input_bvh")
    p.add_argument("output_gltf")
    p.add_argument("--fps", type=float, default=30.0)
    args = p.parse_args()
    print(f"[mocap] BVH ingest: {args.input_bvh} -> {args.output_gltf} @ {args.fps} fps")
    print("[mocap] Install pygltflib and extend this script for production clips.", file=sys.stderr)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
