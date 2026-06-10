#!/usr/bin/env python3
"""Evaluate frozen CA-FER checkpoint on DFEW-style manifest (Table IV)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import torch
from torch.utils.data import DataLoader

from cafer.config import NUM_EXPRESSIONS
from cafer.dataset import GccferManifestDataset, collate_batch
from cafer.model import CAFER
from metrics import unweighted_average_recall, weighted_average_recall

# Static C benchmarks from src/gccfer (Table IV) for tolerance check
C_DFEW_CAFER_UAR = 63.93
TOLERANCE = 5.0


def main() -> None:
    parser = argparse.ArgumentParser(description="DFEW evaluation")
    parser.add_argument("--manifest", required=True, help="DFEW-style CSV manifest")
    parser.add_argument("--checkpoint", required=True, help="CA-FER checkpoint")
    parser.add_argument("--device", default="auto")
    args = parser.parse_args()

    device = torch.device("cuda" if args.device == "auto" and torch.cuda.is_available() else "cpu")
    if args.device != "auto":
        device = torch.device(args.device)

    ds = GccferManifestDataset(args.manifest)
    loader = DataLoader(ds, batch_size=4, shuffle=False, collate_fn=collate_batch)
    model = CAFER().to(device)
    state = torch.load(args.checkpoint, map_location=device)
    model.load_state_dict(state, strict=False)
    model.eval()

    y_true, y_pred = [], []
    with torch.no_grad():
        for pixels, cultures, labels in loader:
            pixels = pixels.to(device)
            cultures = cultures.to(device)
            logits, _ = model(pixels, cultures, use_global=False)
            y_true.extend(labels.tolist())
            y_pred.extend(logits.argmax(dim=-1).cpu().tolist())

    uar = unweighted_average_recall(y_true, y_pred, NUM_EXPRESSIONS)
    war = weighted_average_recall(y_true, y_pred, NUM_EXPRESSIONS)
    print(f"[DFEW eval] UAR={uar:.2f}% WAR={war:.2f}%")
    if abs(uar - C_DFEW_CAFER_UAR) > TOLERANCE and len(ds) > 100:
        print(f"[DFEW eval] WARN UAR differs from C benchmark {C_DFEW_CAFER_UAR} by >{TOLERANCE}")


if __name__ == "__main__":
    main()
