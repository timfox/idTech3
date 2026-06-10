#!/usr/bin/env python3
"""Extract frozen DaX patch-token embeddings from tiled WSIs."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import torch
from PIL import Image
from torchvision import transforms

from config import EvalConfig, MPP_AT_20X, PATCH_PX
from encoder import DaXEncoder, load_dax_weights


def load_image(path: Path, size: int) -> torch.Tensor:
    img = Image.open(path).convert("RGB")
    tfm = transforms.Compose(
        [
            transforms.Resize((size, size)),
            transforms.ToTensor(),
            transforms.Normalize(mean=(0.485, 0.456, 0.406), std=(0.229, 0.224, 0.225)),
        ]
    )
    return tfm(img).unsqueeze(0)


def main() -> None:
    parser = argparse.ArgumentParser(description="DaX WSI patch feature extraction")
    parser.add_argument("input", type=Path, nargs="?", help="patch directory or image list JSON")
    parser.add_argument("--output", type=Path, default=Path("dax_features.pt"))
    parser.add_argument("--checkpoint", type=str, default="")
    parser.add_argument("--weights", type=str, default="", help="huggingface:|url|path via load_dax_weights")
    parser.add_argument("--mag", type=float, default=20.0)
    parser.add_argument("--size", type=int, default=224, help="Resize for encoder input (patches at full WSI scale)")
    parser.add_argument("--patch-tokens", action="store_true", default=True, help="Export patch tokens (not CLS only)")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    cfg = EvalConfig(encoder_mag=args.mag, tile_mpp=MPP_AT_20X)
    print(f"[DaX extract] mag={cfg.encoder_mag}x mpp={cfg.tile_mpp} patch_store={PATCH_PX}px")

    if args.dry_run:
        print("[DaX extract] dry-run OK")
        return

    ckpt = args.checkpoint
    if args.weights:
        ckpt = load_dax_weights(args.weights) or ckpt

    model = DaXEncoder(init_checkpoint=ckpt or None, freeze=True)
    model.eval()

    if not args.input:
        raise SystemExit("Provide input patch directory or use --dry-run")

    paths: list[Path]
    if args.input.is_dir():
        paths = sorted(args.input.glob("*.png")) + sorted(args.input.glob("*.jpg"))
    elif args.input.suffix == ".json":
        paths = [Path(p) for p in json.loads(args.input.read_text())]
    else:
        paths = [args.input]

    feats = []
    for p in paths:
        x = load_image(p, args.size)
        with torch.no_grad():
            cls, patches = model.forward(x)
            feats.append(patches.cpu() if args.patch_tokens else cls.unsqueeze(1).cpu())

    out = {"paths": [str(p) for p in paths], "embeddings": torch.cat(feats, dim=0)}
    torch.save(out, args.output)
    print(f"[DaX extract] saved {len(paths)} slide tensors -> {args.output}")


if __name__ == "__main__":
    main()
