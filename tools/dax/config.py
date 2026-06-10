"""DaX pathology foundation model configuration (Zhao et al., arXiv:2606.06983)."""

from dataclasses import dataclass, field
from typing import List, Tuple

ANCHOR_MAGNIFICATIONS: Tuple[float, ...] = (2.5, 5.0, 10.0, 20.0)
PATCH_PX = 1920
MPP_AT_20X = 0.5
OVERLAP_PX = 640
TISSUE_RATIO_MIN = 2.0 / 3.0

STAGE2_CROP_PAIRS: Tuple[Tuple[int, int], ...] = (
    (512, 224),
    (384, 168),
    (768, 336),
)

EMBED_DIM = 1024
NUM_BENCHMARK_TASKS = 161
NUM_BENCHMARK_DATASETS = 44
NUM_PATIENTS = 28182
NUM_SLIDES = 34394
PRETRAIN_WSIS = 104569
NUM_CV_FOLDS = 20
SIGNIFICANCE_ALPHA = 0.05


@dataclass
class PretrainConfig:
    stage: int = 1
    backbone: str = "vit_large_patch16_224"
    init_weights: str = "dinov3"
    anchor_mags: Tuple[float, ...] = ANCHOR_MAGNIFICATIONS
    patch_px: int = PATCH_PX
    mpp_at_20x: float = MPP_AT_20X
    overlap_px: int = OVERLAP_PX
    tissue_ratio_min: float = TISSUE_RATIO_MIN
    stage2_crop_pairs: Tuple[Tuple[int, int], ...] = STAGE2_CROP_PAIRS
    gram_anchor: bool = False
    lr: float = 1e-4
    batch_size: int = 8
    epochs: int = 100


@dataclass
class EvalConfig:
    encoder_mag: float = 20.0
    tile_mpp: float = MPP_AT_20X
    aggregation: str = "mean"  # mean | abmil
    num_folds: int = 5
    val_splits_per_test: int = 4
    alpha: float = SIGNIFICANCE_ALPHA
    metrics: List[str] = field(default_factory=lambda: ["auroc", "balanced_accuracy", "c_index"])
