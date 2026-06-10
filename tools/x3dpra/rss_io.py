"""Load nodes, RSS measurements, and background baselines for x3DPRA."""

from __future__ import annotations

import csv
from pathlib import Path
from typing import Optional, Tuple

import numpy as np


def load_nodes_csv(path: Path) -> np.ndarray:
    """CSV columns: x,y,z (meters)."""
    rows = []
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append([float(row["x"]), float(row["y"]), float(row["z"])])
    return np.array(rows, dtype=np.float64)


def load_measurements_npz(path: Path) -> Tuple[np.ndarray, Optional[np.ndarray]]:
    """NPZ with delta_y_db or P_obj/P_bg arrays."""
    data = np.load(path)
    if "delta_y_db" in data:
        return np.asarray(data["delta_y_db"]).ravel(), None
    if "P_obj" in data and "P_bg" in data:
        p_obj = np.asarray(data["P_obj"]).ravel()
        p_bg = np.asarray(data["P_bg"]).ravel()
        return p_obj - p_bg, p_bg
    if "y" in data:
        return np.asarray(data["y"]).ravel(), None
    raise ValueError(f"{path}: expected delta_y_db, y, or P_obj/P_bg")


def background_subtract(
    p_obj: np.ndarray,
    p_bg: np.ndarray,
) -> np.ndarray:
    """Eq. 15: delta_P = P_obj - P_bg (dB domain)."""
    return np.asarray(p_obj).ravel() - np.asarray(p_bg).ravel()
