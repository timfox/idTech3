#!/usr/bin/env python3
"""DaX Stage 1: pathology-specific representation learning."""

from __future__ import annotations

import argparse
from pathlib import Path

from config import PretrainConfig


def main() -> None:
    parser = argparse.ArgumentParser(description="DaX Stage 1 pretraining scaffold")
    parser.add_argument("--data", type=Path, help="patch manifest or WSI list")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--init", type=str, default="", help="DINOv3 init checkpoint")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    cfg = PretrainConfig(stage=1, epochs=args.epochs, gram_anchor=False)
    print(f"[DaX Stage 1] continuous mag {cfg.anchor_mags}, patch {cfg.patch_px}px")
    print(f"[DaX Stage 1] cross-scale views + pathology augmentations")

    if args.dry_run:
        print("[DaX Stage 1] dry-run OK (install torch+timm and provide --data to train)")
        return

    from encoder import DaXEncoder

    model = DaXEncoder(init_checkpoint=args.init or None)
    print(f"[DaX Stage 1] encoder params: {sum(p.numel() for p in model.parameters()):,}")

    if not args.data:
        raise SystemExit("Provide --data for training or use --dry-run")

    print(f"[DaX Stage 1] training on {args.data} — implement dataloader hook for your WSI patch store")


if __name__ == "__main__":
    main()
