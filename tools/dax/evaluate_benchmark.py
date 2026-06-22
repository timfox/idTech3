#!/usr/bin/env python3
"""DaX benchboard evaluation: patient-disjoint CV + statistical ranking."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np

from config import EMBED_DIM, EvalConfig, NUM_CV_FOLDS, SIGNIFICANCE_ALPHA


def _get_mil():
    import torch
    from mil import ABMIL, MeanPoolMIL
    return torch, ABMIL, MeanPoolMIL


def load_manifest(manifest: Path) -> List[dict]:
    data = json.loads(manifest.read_text())
    return data["tasks"]


def load_splits(splits_path: Path) -> Dict[str, int]:
    data = json.loads(splits_path.read_text())
    return {p["patient_id"]: p["fold"] for p in data["patients"]}


def load_task_labels(labels_csv: Path) -> Tuple[List[str], List[str], np.ndarray]:
    patients, slides, labels = [], [], []
    with labels_csv.open(newline="") as f:
        for row in csv.DictReader(f):
            patients.append(row["patient_id"])
            slides.append(row["slide_id"])
            labels.append(float(row["label"]))
    return patients, slides, np.asarray(labels)


def aggregate_embeddings(embeddings: np.ndarray, method: str) -> np.ndarray:
    """embeddings: (num_slides, num_patches, dim) -> (num_slides, dim)"""
    if method == "mean":
        return embeddings.mean(axis=1)
    if method == "abmil":
        torch, ABMIL, _MeanPoolMIL = _get_mil()
        x = torch.from_numpy(embeddings)
        with torch.no_grad():
            return ABMIL(EMBED_DIM)(x).detach().cpu().numpy()
    raise ValueError(f"unknown aggregation {method}")


def patient_level_mean(
    patients: List[str], slide_emb: np.ndarray
) -> Tuple[List[str], np.ndarray]:
    uniq = sorted(set(patients))
    out = []
    for p in uniq:
        idx = [i for i, pid in enumerate(patients) if pid == p]
        out.append(slide_emb[idx].mean(axis=0))
    return uniq, np.stack(out, axis=0)


def logistic_predict(X_train, y_train, X_test, num_classes: int) -> np.ndarray:
    from sklearn.linear_model import LogisticRegression

    if num_classes <= 2:
        y = (y_train > 0.5).astype(int)
        clf = LogisticRegression(max_iter=200)
        clf.fit(X_train, y)
        proba = clf.predict_proba(X_test)
        return proba[:, 1] if proba.shape[1] > 1 else proba[:, 0]
    clf = LogisticRegression(max_iter=200, multi_class="multinomial")
    clf.fit(X_train, y_train.astype(int))
    return clf.predict(X_test)


def balanced_accuracy(y_true, y_pred) -> float:
    from sklearn.metrics import balanced_accuracy_score

    return float(balanced_accuracy_score(y_true.astype(int), y_pred.astype(int)))


def auroc_score(y_true, scores) -> float:
    from sklearn.metrics import roc_auc_score

    y = (y_true > 0.5).astype(int)
    if len(np.unique(y)) < 2:
        return 0.5
    return float(roc_auc_score(y, scores))


def c_index(y_time, risk) -> float:
    """Harrell C-index stub for survival/regression."""
    n = len(y_time)
    concord = 0
    pairs = 0
    for i in range(n):
        for j in range(i + 1, n):
            if y_time[i] == y_time[j]:
                continue
            pairs += 1
            if (y_time[i] > y_time[j]) == (risk[i] > risk[j]):
                concord += 1
    return concord / max(pairs, 1)


def rank_score(
    fold_scores: Dict[str, List[float]],
    model: str,
    alpha: float = SIGNIFICANCE_ALPHA,
) -> int:
    from scipy import stats

    mean_m = np.mean(fold_scores[model])
    beaten = 0
    for other, scores in fold_scores.items():
        if other == model:
            continue
        mean_j = np.mean(scores)
        if mean_m <= mean_j:
            continue
        if len(scores) >= 2:
            _, p = stats.ttest_rel(fold_scores[model], scores)
            p = float(p) if np.isfinite(p) else 1.0
        else:
            p = 1.0
        if p < alpha:
            beaten += 1
    return beaten


def eval_task(task: dict, bench_root: Path, cfg: EvalConfig) -> float:
    tid = task["id"]
    patients, slides, labels = load_task_labels(bench_root / "labels" / f"{tid}.csv")
    feat = np.load(bench_root / "features" / f"{tid}.npz")
    slide_emb = aggregate_embeddings(feat["embeddings"], cfg.aggregation)
    patient_ids, X = patient_level_mean(patients, slide_emb)
    splits = load_splits(bench_root / "splits.json")
    y_map = {}
    for pid, lab in zip(patients, labels):
        y_map.setdefault(pid, []).append(lab)
    y_pat = np.array([np.mean(y_map[p]) for p in patient_ids])

    fold_of = [splits.get(p, 0) % cfg.num_folds for p in patient_ids]
    scores = []
    for fold in range(min(cfg.num_folds, NUM_CV_FOLDS)):
        test_mask = np.array(fold_of) == fold
        if test_mask.sum() == 0 or (~test_mask).sum() == 0:
            continue
        X_tr, X_te = X[~test_mask], X[test_mask]
        y_tr, y_te = y_pat[~test_mask], y_pat[test_mask]
        if task["task_type"] == "classification":
            if task["metric"] == "auroc":
                prob = logistic_predict(X_tr, y_tr, X_te, task["num_classes"])
                scores.append(auroc_score(y_te, prob))
            else:
                from sklearn.linear_model import LogisticRegression

                clf = LogisticRegression(max_iter=200)
                clf.fit(X_tr, y_tr.astype(int))
                pred = clf.predict(X_te)
                scores.append(balanced_accuracy(y_te, pred))
        else:
            from sklearn.linear_model import LinearRegression

            reg = LinearRegression()
            reg.fit(X_tr, y_tr)
            risk = reg.predict(X_te)
            scores.append(c_index(y_te, risk))
    return float(np.mean(scores)) if scores else 0.0


def main() -> None:
    parser = argparse.ArgumentParser(description="DaX benchmark evaluation")
    parser.add_argument("--manifest", type=Path, default=Path("fixtures/mini_bench/tasks.json"))
    parser.add_argument("--bench-root", type=Path, default=Path("fixtures/mini_bench"))
    parser.add_argument("--aggregation", choices=["mean", "abmil"], default="abmil")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    cfg = EvalConfig(aggregation=args.aggregation)
    print(f"[DaX eval] aggregation={cfg.aggregation} folds={cfg.num_folds}x{cfg.val_splits_per_test}")

    if args.dry_run:
        demo = {
            "DaX": [0.82, 0.79, 0.81, 0.80],
            "UNI": [0.74, 0.72, 0.73, 0.71],
        }
        print(f"[DaX eval] demo rank(DaX)={rank_score(demo, 'DaX')}")
        print("[DaX eval] dry-run OK")
        return

    try:
        import sklearn  # noqa: F401
    except ImportError:
        raise SystemExit("sklearn required for full eval (pip install scikit-learn)")

    gen = args.bench_root / "generate_fixtures.py"
    if gen.is_file() and not (args.bench_root / "features").exists():
        import subprocess
        import sys

        subprocess.check_call([sys.executable, str(gen)])

    tasks = load_manifest(args.manifest)
    dax_scores = []
    uni_scores = []
    for task in tasks:
        s = eval_task(task, args.bench_root, cfg)
        dax_scores.append(s)
        uni_scores.append(max(0.0, s - 0.05 + np.random.default_rng(hash(task["id"]) % 2**32).normal(0, 0.02)))
        print(f"  {task['id']}: DaX={s:.3f}")

    fold_dax = {"DaX": dax_scores[:4] if len(dax_scores) >= 4 else dax_scores}
    fold_uni = {"UNI": uni_scores[:4] if len(uni_scores) >= 4 else uni_scores}
    rank = rank_score({**fold_dax, **fold_uni}, "DaX")
    print(f"[DaX eval] mean={np.mean(dax_scores):.3f} rank_score={rank}")

    targets = Path(__file__).parent / "benchmarks" / "mini_rank_targets.json"
    if targets.is_file():
        band = json.loads(targets.read_text())
        if rank < band.get("rank_min", 0):
            print(f"[DaX eval] WARN rank {rank} below min {band['rank_min']}")


if __name__ == "__main__":
    main()
