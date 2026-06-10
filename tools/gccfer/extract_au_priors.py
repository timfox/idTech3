#!/usr/bin/env python3
"""Extract AU-grounded cultural priors from GCC-FER manifest (Algorithm 1 Phase 1)."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

import numpy as np

from cafer.config import NUM_AUS, NUM_CULTURES
from cafer.cultural_prior import au_stats_from_sequence, build_culture_priors


def synthetic_au_from_video_path(_path: str, seed: int = 0) -> np.ndarray:
    """Placeholder AU sequence when Py-Feat is unavailable."""
    rng = np.random.default_rng(seed)
    return rng.random((16, NUM_AUS)).astype(np.float32) * 0.5


def try_pyfeat_au(path: str) -> np.ndarray | None:
    try:
        from feat import Detector  # py-feat
    except ImportError:
        return None
    detector = Detector()
    # Py-Feat video API varies; fall back to synthetic if unsupported
    try:
        result = detector.detect_video(path, batch_size=16)
        aus = result.aus.values[:, :NUM_AUS]
        return aus.astype(np.float32)
    except Exception:
        return None


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract AU cultural priors for CA-FER")
    parser.add_argument("--manifest", required=True, help="CSV manifest path,culture,expression")
    parser.add_argument("--output", default="cultural_priors.json", help="Output JSON path")
    parser.add_argument("--max-per-culture", type=int, default=200, help="Cap samples per culture")
    parser.add_argument("--synthetic-au", action="store_true", help="Use synthetic AU (CI fixture)")
    args = parser.parse_args()

    from cafer.dataset import GccferManifestDataset

    ds = GccferManifestDataset(args.manifest)
    buckets: dict[int, list[np.ndarray]] = defaultdict(list)

    for i in range(len(ds)):
        path, culture, _ = ds.rows[i]
        if len(buckets[culture]) >= args.max_per_culture:
            continue
        au = try_pyfeat_au(path)
        if au is None:
            if not args.synthetic_au:
                raise SystemExit(
                    f"Py-Feat unavailable for {path}; install py-feat or pass --synthetic-au"
                )
            au = synthetic_au_from_video_path(path, seed=i)
        stats = au_stats_from_sequence(au)
        mean_au = stats[:NUM_AUS]
        buckets[culture].append(mean_au)

    au_means = {}
    for c in range(NUM_CULTURES):
        if buckets[c]:
            au_means[c] = np.stack(buckets[c], axis=0).mean(axis=0)
        else:
            au_means[c] = np.zeros(NUM_AUS, dtype=np.float32)

    bundle = build_culture_priors(au_means)
    out = {
        "profiles": bundle.profiles.tolist(),
        "normalized": bundle.normalized.tolist(),
        "initial_embeddings": bundle.initial_embeddings.tolist(),
    }
    Path(args.output).write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(f"Wrote cultural priors to {args.output}")


if __name__ == "__main__":
    main()
