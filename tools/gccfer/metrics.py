"""GCC-FER metrics: UAR and WAR per culture and global."""

from __future__ import annotations

from collections import defaultdict
from typing import Dict, Iterable, List, Sequence, Tuple

import numpy as np


def _per_class_recall(y_true: np.ndarray, y_pred: np.ndarray, num_classes: int) -> np.ndarray:
    recalls = np.zeros(num_classes, dtype=np.float64)
    for c in range(num_classes):
        mask = y_true == c
        if mask.sum() == 0:
            recalls[c] = 0.0
        else:
            recalls[c] = (y_pred[mask] == c).mean()
    return recalls


def unweighted_average_recall(
    y_true: Sequence[int],
    y_pred: Sequence[int],
    num_classes: int,
) -> float:
    recalls = _per_class_recall(np.asarray(y_true), np.asarray(y_pred), num_classes)
    present = [r for i, r in enumerate(recalls) if np.any(np.asarray(y_true) == i)]
    return float(np.mean(present)) * 100.0 if present else 0.0


def weighted_average_recall(
    y_true: Sequence[int],
    y_pred: Sequence[int],
    num_classes: int,
) -> float:
    y_true = np.asarray(y_true)
    y_pred = np.asarray(y_pred)
    recalls = _per_class_recall(y_true, y_pred, num_classes)
    weights = np.array([(y_true == c).sum() for c in range(num_classes)], dtype=np.float64)
    total = weights.sum()
    if total <= 0:
        return 0.0
    return float(np.dot(recalls, weights / total)) * 100.0


def metrics_by_culture(
    y_true: Sequence[int],
    y_pred: Sequence[int],
    cultures: Sequence[int],
    num_classes: int,
    num_cultures: int,
) -> Dict[int, Tuple[float, float]]:
    out: Dict[int, Tuple[float, float]] = {}
    y_true = np.asarray(y_true)
    y_pred = np.asarray(y_pred)
    cultures = np.asarray(cultures)
    for c in range(num_cultures):
        mask = cultures == c
        if mask.sum() == 0:
            out[c] = (0.0, 0.0)
            continue
        out[c] = (
            unweighted_average_recall(y_true[mask], y_pred[mask], num_classes),
            weighted_average_recall(y_true[mask], y_pred[mask], num_classes),
        )
    return out
