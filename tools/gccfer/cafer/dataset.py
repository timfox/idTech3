"""GCC-FER dataset loader (16 frames, 224x224, ImageNet norm)."""

from __future__ import annotations

import csv
import os
from pathlib import Path
from typing import Callable, List, Optional, Tuple

import cv2
import numpy as np
import torch
from torch.utils.data import Dataset

from .config import CULTURES, EXPRESSIONS, IMAGENET_MEAN, IMAGENET_STD, INPUT_SIZE, NUM_FRAMES


def _uniform_sample_indices(total: int, num: int) -> List[int]:
    if total <= 0:
        return [0] * num
    if total == 1:
        return [0] * num
    return [int(round(i * (total - 1) / (num - 1))) for i in range(num)]


def load_video_frames(path: str, num_frames: int = NUM_FRAMES, size: int = INPUT_SIZE) -> np.ndarray:
    """Load uniformly sampled RGB frames from video file."""
    cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        raise FileNotFoundError(f"Cannot open video: {path}")

    total = int(cap.get(cv2.CAP_PROP_FRAME_COUNT)) or 1
    indices = _uniform_sample_indices(total, num_frames)
    frames = []
    for idx in indices:
        cap.set(cv2.CAP_PROP_POS_FRAMES, idx)
        ok, frame = cap.read()
        if not ok:
            if frames:
                frames.append(frames[-1].copy())
            else:
                frame = np.zeros((size, size, 3), dtype=np.uint8)
                frames.append(frame)
            continue
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        frame = cv2.resize(frame, (size, size), interpolation=cv2.INTER_LINEAR)
        frames.append(frame)
    cap.release()
    return np.stack(frames, axis=0)


def load_frame_directory(path: str, num_frames: int = NUM_FRAMES, size: int = INPUT_SIZE) -> np.ndarray:
    """Load frames from a directory of images sorted by name."""
    exts = {".jpg", ".jpeg", ".png", ".bmp"}
    files = sorted(
        [p for p in Path(path).iterdir() if p.suffix.lower() in exts],
        key=lambda p: p.name,
    )
    if not files:
        raise FileNotFoundError(f"No images in {path}")
    indices = _uniform_sample_indices(len(files), num_frames)
    frames = []
    for idx in indices:
        img = cv2.imread(str(files[idx]))
        if img is None:
            continue
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        img = cv2.resize(img, (size, size), interpolation=cv2.INTER_LINEAR)
        frames.append(img)
    while len(frames) < num_frames:
        frames.append(frames[-1].copy())
    return np.stack(frames[:num_frames], axis=0)


def normalize_frames(frames: np.ndarray) -> torch.Tensor:
    """(T,H,W,C) uint8 -> (T,C,H,W) float normalized."""
    x = frames.astype(np.float32) / 255.0
    x = np.transpose(x, (0, 3, 1, 2))
    mean = np.array(IMAGENET_MEAN, dtype=np.float32).reshape(1, 3, 1, 1)
    std = np.array(IMAGENET_STD, dtype=np.float32).reshape(1, 3, 1, 1)
    x = (x - mean) / std
    return torch.from_numpy(x)


class GccferManifestDataset(Dataset):
    """
    CSV manifest columns: path,culture,expression
    culture/expression are string labels from CULTURES / EXPRESSIONS.
    """

    def __init__(self, manifest_csv: str, transform: Optional[Callable] = None):
        self.rows: List[Tuple[str, int, int]] = []
        with open(manifest_csv, newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                path = row["path"].strip()
                culture = CULTURES.index(row["culture"].strip().lower())
                expr = EXPRESSIONS.index(row["expression"].strip().lower())
                self.rows.append((path, culture, expr))
        self.transform = transform

    def __len__(self) -> int:
        return len(self.rows)

    def __getitem__(self, idx: int):
        path, culture, expr = self.rows[idx]
        if path.startswith("fixture:"):
            rng = np.random.default_rng(idx)
            frames = (rng.random((NUM_FRAMES, INPUT_SIZE, INPUT_SIZE, 3)) * 255).astype(np.uint8)
        elif os.path.isdir(path):
            frames = load_frame_directory(path)
        else:
            frames = load_video_frames(path)
        pixel_values = normalize_frames(frames)
        if self.transform:
            pixel_values = self.transform(pixel_values)
        return {
            "pixel_values": pixel_values,
            "culture_id": culture,
            "label": expr,
        }


def collate_batch(batch):
    pixels = torch.stack([b["pixel_values"] for b in batch], dim=0)
    cultures = torch.tensor([b["culture_id"] for b in batch], dtype=torch.long)
    labels = torch.tensor([b["label"] for b in batch], dtype=torch.long)
    return pixels, cultures, labels
