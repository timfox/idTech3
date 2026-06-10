#!/usr/bin/env python3
"""DaX Stage 2: multi-input-size + Gram-anchored dense refinement."""

from __future__ import annotations

import argparse
from pathlib import Path

from config import PretrainConfig, STAGE2_CROP_PAIRS


def main() -> None:
    parser = argparse.ArgumentParser(description="DaX Stage 2 pretraining scaffold")
    parser.add_argument("--checkpoint", type=Path, required=False, help="Stage 1 weights")
    parser.add_argument("--epochs", type=int, default=50)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    cfg = PretrainConfig(stage=2, epochs=args.epochs, gram_anchor=True)
    print(f"[DaX Stage 2] crop pairs {STAGE2_CROP_PAIRS}, gram_anchor={cfg.gram_anchor}")
    print(f"[DaX Stage 2] max global view up to 1536px @ 2x resolution setting")

    if args.dry_run:
        print("[DaX Stage 2] dry-run OK")
        return

    from encoder import DaXEncoder

    model = DaXEncoder(init_checkpoint=str(args.checkpoint) if args.checkpoint else None)
    print(f"[DaX Stage 2] encoder ready, params: {sum(p.numel() for p in model.parameters()):,}")

    if not args.checkpoint:
        raise SystemExit("Stage 2 requires --checkpoint from Stage 1 or use --dry-run")


if __name__ == "__main__":
    main()
