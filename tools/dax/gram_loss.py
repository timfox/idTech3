"""Gram-anchored dense consistency (Stage 2)."""

from __future__ import annotations

import torch
import torch.nn.functional as F


def gram_matrix(tokens: torch.Tensor) -> torch.Tensor:
    """Pairwise token similarity [B, T, T]."""
    tokens = F.normalize(tokens, dim=-1)
    return tokens @ tokens.transpose(-1, -2)


def gram_anchor_loss(
    student_tokens: torch.Tensor,
    anchor_tokens: torch.Tensor,
    mask: torch.Tensor | None = None,
) -> torch.Tensor:
    """Frobenius norm between student and anchored teacher Gram matrices."""
    g_s = gram_matrix(student_tokens)
    g_a = gram_matrix(anchor_tokens.detach())
    diff = (g_s - g_a) ** 2
    if mask is not None:
        diff = diff * mask.unsqueeze(-1)
    return diff.mean()
