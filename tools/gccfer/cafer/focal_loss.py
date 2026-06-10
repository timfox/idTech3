"""Focal loss with label smoothing (paper §IV)."""

from __future__ import annotations

import torch
import torch.nn.functional as F


def focal_loss(
    logits: torch.Tensor,
    targets: torch.Tensor,
    gamma: float = 2.0,
    alpha: float = 1.0,
    label_smoothing: float = 0.1,
) -> torch.Tensor:
    """Multi-class focal loss with optional label smoothing."""
    num_classes = logits.size(-1)
    log_probs = F.log_softmax(logits, dim=-1)
    probs = log_probs.exp()

    if label_smoothing > 0.0:
        with torch.no_grad():
            smooth = torch.full_like(log_probs, label_smoothing / num_classes)
            smooth.scatter_(1, targets.unsqueeze(1), 1.0 - label_smoothing)
        ce = -(smooth * log_probs).sum(dim=-1)
        pt = (probs * smooth).sum(dim=-1)
    else:
        ce = F.nll_loss(log_probs, targets, reduction="none")
        pt = probs.gather(1, targets.unsqueeze(1)).squeeze(1)

    return (alpha * (1.0 - pt).pow(gamma) * ce).mean()
