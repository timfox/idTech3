#!/usr/bin/env python3
"""Train GCC-FER ablation baselines (Table III)."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description="GCC-FER baseline ablations")
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--output", default="checkpoints/ablations")
    parser.add_argument("--epochs", type=int, default=5)
    parser.add_argument(
        "--baseline",
        choices=["random_embed", "one_hot", "separate_heads", "au_concat", "cafer"],
        default="cafer",
    )
    args = parser.parse_args()

    out = Path(args.output)
    out.mkdir(parents=True, exist_ok=True)
    cmd = [
        sys.executable,
        str(Path(__file__).parent / "train_cafer.py"),
        "--manifest",
        args.manifest,
        "--output",
        str(out / args.baseline),
        "--epochs",
        str(args.epochs),
        "--folds",
        "2",
    ]
    if args.baseline in ("random_embed", "one_hot", "separate_heads", "au_concat"):
        cmd.append("--baseline")
    print(f"[baselines] running {args.baseline}: {' '.join(cmd)}")
    subprocess.check_call(cmd)


if __name__ == "__main__":
    main()
