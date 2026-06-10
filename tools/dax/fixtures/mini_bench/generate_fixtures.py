#!/usr/bin/env python3
"""Generate synthetic mini-bench labels and feature NPZ files."""

from __future__ import annotations

import csv
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
TASKS = json.loads((ROOT / "tasks.json").read_text())["tasks"]
SPLITS = json.loads((ROOT / "splits.json").read_text())
PATIENTS = [p["patient_id"] for p in SPLITS["patients"]]
RNG = np.random.default_rng(42)
EMBED = 1024


def main() -> None:
    labels_dir = ROOT / "labels"
    feats_dir = ROOT / "features"
    labels_dir.mkdir(exist_ok=True)
    feats_dir.mkdir(exist_ok=True)

    for task in TASKS:
        tid = task["id"]
        rows = []
        slide_feats = []
        for pi, pid in enumerate(PATIENTS):
            for si in range(2):
                sid = f"{pid}_S{si}"
                if task["task_type"] == "regression":
                    label = float(RNG.uniform(0, 5))
                elif task["task_type"] == "survival":
                    label = float(RNG.exponential(2.0))
                else:
                    label = int(RNG.integers(0, task["num_classes"]))
                rows.append((pid, sid, label))
                slide_feats.append(RNG.standard_normal((8, EMBED)).astype(np.float32))

        with (labels_dir / f"{tid}.csv").open("w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["patient_id", "slide_id", "label"])
            w.writerows(rows)

        np.savez_compressed(
            feats_dir / f"{tid}.npz",
            slide_ids=[r[1] for r in rows],
            embeddings=np.stack(slide_feats, axis=0),
        )
    print(f"[mini_bench] generated {len(TASKS)} tasks under {ROOT}")


if __name__ == "__main__":
    main()
