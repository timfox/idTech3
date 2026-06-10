"""Multiple-instance learning heads for WSI-level prediction."""

from __future__ import annotations

import torch
import torch.nn as nn


class MeanPoolMIL(nn.Module):
    def forward(self, patch_emb: torch.Tensor) -> torch.Tensor:
        return patch_emb.mean(dim=1)


class ABMIL(nn.Module):
    """Attention-based MIL (Ilse et al., ICML 2018)."""

    def __init__(self, embed_dim: int, hidden: int = 256) -> None:
        super().__init__()
        self.attention = nn.Sequential(
            nn.Linear(embed_dim, hidden),
            nn.Tanh(),
            nn.Linear(hidden, 1),
        )

    def forward(self, patch_emb: torch.Tensor) -> torch.Tensor:
        attn = self.attention(patch_emb)
        weights = torch.softmax(attn, dim=1)
        return (patch_emb * weights).sum(dim=1)
