#!/usr/bin/env python3
"""
idTech3 wrapper for Microsoft TRELLIS.2 image-to-3D (GLB export).

Requires a local TRELLIS.2 checkout (--repo) with conda env per upstream README.
Invoked by trellis_generate / cl_trellis_cmd from the engine client.
"""

from __future__ import annotations

import argparse
import os
import sys


def _fail(msg: str) -> None:
    print(f"TRELLIS wrapper: {msg}", file=sys.stderr)
    sys.exit(1)


def main() -> None:
    ap = argparse.ArgumentParser(description="TRELLIS.2 image-to-GLB for idTech3")
    ap.add_argument("--repo", required=True, help="Path to TRELLIS.2 git checkout")
    ap.add_argument("--image", required=True, help="Input image (PNG/JPEG/WebP, etc.)")
    ap.add_argument("--output", required=True, help="Output .glb path")
    ap.add_argument("--model", default="microsoft/TRELLIS.2-4B", help="Hugging Face model id")
    ap.add_argument("--decimation", type=int, default=500000, help="o_voxel decimation_target")
    ap.add_argument("--texture-size", type=int, default=2048, help="o_voxel texture_size")
    ap.add_argument("--remesh", action="store_true", default=True)
    ap.add_argument("--no-remesh", action="store_false", dest="remesh")
    args = ap.parse_args()

    repo = os.path.abspath(args.repo)
    if not os.path.isdir(repo):
        _fail(f"repo not found: {repo}")
    if not os.path.isfile(args.image):
        _fail(f"image not found: {args.image}")

    os.environ.setdefault("OPENCV_IO_ENABLE_OPENEXR", "1")
    os.environ.setdefault("PYTORCH_CUDA_ALLOC_CONF", "expandable_segments:True")

    sys.path.insert(0, repo)
    os.chdir(repo)

    try:
        import cv2
        import torch
        from PIL import Image
        from trellis2.pipelines import Trellis2ImageTo3DPipeline
        import o_voxel
    except ImportError as exc:
        _fail(
            f"import failed ({exc}). Activate the trellis2 conda env per TRELLIS.2/setup.sh"
        )

    if not torch.cuda.is_available():
        _fail("CUDA is not available — TRELLIS.2 requires an NVIDIA GPU (24GB+ recommended)")

    print(f"TRELLIS wrapper: loading pipeline {args.model} ...")
    pipeline = Trellis2ImageTo3DPipeline.from_pretrained(args.model)
    pipeline.cuda()

    image = Image.open(args.image).convert("RGB")
    print(f"TRELLIS wrapper: running image-to-3D on {args.image} ...")
    mesh = pipeline.run(image)[0]
    mesh.simplify(16777216)

    out_dir = os.path.dirname(os.path.abspath(args.output))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    print(f"TRELLIS wrapper: exporting GLB -> {args.output}")
    glb = o_voxel.postprocess.to_glb(
        vertices=mesh.vertices,
        faces=mesh.faces,
        attr_volume=mesh.attrs,
        coords=mesh.coords,
        attr_layout=mesh.layout,
        voxel_size=mesh.voxel_size,
        aabb=[[-0.5, -0.5, -0.5], [0.5, 0.5, 0.5]],
        decimation_target=args.decimation,
        texture_size=args.texture_size,
        remesh=args.remesh,
        remesh_band=1,
        remesh_project=0,
        verbose=True,
    )
    glb.export(args.output, extension_webp=True)
    print(f"TRELLIS wrapper: done ({args.output})")


if __name__ == "__main__":
    main()
