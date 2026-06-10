"""Continuous magnification sampling and pathology augmentations (Stage 1)."""

from __future__ import annotations

import random
from typing import Tuple

import torch
import torchvision.transforms as T
import torchvision.transforms.functional as TF

from config import ANCHOR_MAGNIFICATIONS, PretrainConfig


def sample_continuous_magnification(cfg: PretrainConfig) -> float:
    lo, hi = cfg.anchor_mags[0], cfg.anchor_mags[-1]
    return random.uniform(lo, hi)


def nearest_anchor(mag: float, cfg: PretrainConfig) -> float:
    return min(cfg.anchor_mags, key=lambda a: abs(a - mag))


def pathology_augment(image: torch.Tensor) -> torch.Tensor:
    """Rotation-agnostic + stain/acquisition robust augmentations."""
    angle = random.uniform(0.0, 360.0)
    image = TF.rotate(image, angle)
    jitter = T.ColorJitter(brightness=0.4, contrast=0.4, saturation=0.3, hue=0.05)
    image = jitter(image)
    if random.random() < 0.3:
        image = TF.gaussian_blur(image, kernel_size=5, sigma=(0.1, 2.0))
    return image


def cross_scale_crop_pair(
    patch: torch.Tensor,
    global_scale: float = 1.0,
    local_scale: float = 0.25,
) -> Tuple[torch.Tensor, torch.Tensor]:
    """Global + local views from same anchor region."""
    _, h, w = patch.shape
    gh = max(32, int(h * global_scale))
    gw = max(32, int(w * global_scale))
    lh = max(32, int(h * local_scale))
    lw = max(32, int(w * local_scale))
    top = random.randint(0, max(0, h - gh))
    left = random.randint(0, max(0, w - gw))
    global_view = TF.crop(patch, top, left, gh, gw)
    lt = random.randint(0, max(0, gh - lh))
    ll = random.randint(0, max(0, gw - lw))
    local_view = TF.crop(global_view, lt, ll, lh, lw)
    return global_view, local_view
