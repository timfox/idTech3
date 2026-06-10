"""Behaviorally grounded cultural prior modeling (Algorithm 1, Phase 1)."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Optional

import numpy as np
import torch
import torch.nn as nn

from .config import AU_STATS, EMBED_DIM, NUM_AUS, NUM_CULTURES


def au_stats_from_sequence(au_matrix: np.ndarray) -> np.ndarray:
    """
    Compute 80-dim AU feature vector [mean, std, max, freq] from (T, Na) matrix.
    """
    if au_matrix.ndim != 2:
        raise ValueError("au_matrix must be (T, Na)")
    t = au_matrix.shape[0]
    mean = au_matrix.mean(axis=0)
    std = au_matrix.std(axis=0)
    maxv = au_matrix.max(axis=0)
    freq = (au_matrix >= 0.1).sum(axis=0) / float(t)
    return np.concatenate([mean, std, maxv, freq], axis=0)


def minmax_normalize_profiles(profiles: np.ndarray) -> np.ndarray:
    """Min-max normalize culture AU mean profiles along AU dimension."""
    mu_min = profiles.min(axis=0)
    mu_max = profiles.max(axis=0)
    denom = np.maximum(mu_max - mu_min, 1e-8)
    return (profiles - mu_min) / denom


@dataclass
class CulturePriorBundle:
    profiles: np.ndarray  # (C, Na) mean AU activations
    normalized: np.ndarray  # (C, Na)
    initial_embeddings: np.ndarray  # (C, de)


def build_culture_priors(
    au_means_by_culture: Dict[int, np.ndarray],
    proj: Optional[nn.Linear] = None,
) -> CulturePriorBundle:
    """
    Build normalized AU profiles and projected initial embeddings e_c^(0).
    """
    profiles = np.stack([au_means_by_culture[c] for c in range(NUM_CULTURES)], axis=0)
    normalized = minmax_normalize_profiles(profiles)

    if proj is None:
        proj = nn.Linear(NUM_AUS, EMBED_DIM, bias=False)
        nn.init.xavier_uniform_(proj.weight)

    with torch.no_grad():
        t = torch.from_numpy(normalized.astype(np.float32))
        emb = proj(t).cpu().numpy()

    return CulturePriorBundle(profiles=profiles, normalized=normalized, initial_embeddings=emb)


class CulturalEmbeddingTable(nn.Module):
    """Trainable culture embeddings initialized from AU-grounded priors."""

    def __init__(self, initial: Optional[np.ndarray] = None):
        super().__init__()
        if initial is None:
            initial = np.random.randn(NUM_CULTURES, EMBED_DIM).astype(np.float32) * 0.01
        self.table = nn.Parameter(torch.from_numpy(initial.copy()))

    def forward(self, culture_ids: torch.Tensor) -> torch.Tensor:
        return self.table[culture_ids]

    def global_embedding(self) -> torch.Tensor:
        return self.table.mean(dim=0)


class CultureAdaptation(nn.Module):
    """Generate FiLM parameters a_c, b_c and adapt latent f (Eq. 12)."""

    def __init__(self, latent_dim: int = 768, embed_dim: int = EMBED_DIM):
        super().__init__()
        self.wa = nn.Linear(embed_dim, latent_dim, bias=False)
        self.wb = nn.Linear(embed_dim, latent_dim, bias=False)
        nn.init.xavier_uniform_(self.wa.weight, gain=0.02)
        nn.init.xavier_uniform_(self.wb.weight, gain=0.01)

    def forward(self, latent: torch.Tensor, culture_emb: torch.Tensor) -> torch.Tensor:
        a = self.wa(culture_emb)
        b = self.wb(culture_emb)
        return a * latent + b
