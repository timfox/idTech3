"""DaX ViT-L encoder with DINOv3 initialization (feature extraction)."""

from __future__ import annotations

from pathlib import Path
from typing import Optional, Tuple

import torch
import torch.nn as nn
import torch.nn.functional as F

from config import EMBED_DIM


def load_dax_weights(source: str = "") -> str | None:
    """
    Load public DaX weights from huggingface | url | path.
    Returns resolved checkpoint path or None when unavailable.
    """
    if not source or source in ("none", "dry-run"):
        return None
    p = Path(source)
    if p.is_file():
        return str(p)
    if source.startswith("http://") or source.startswith("https://"):
        raise FileNotFoundError(
            f"DaX weights URL not cached: {source}. Download manually to a local path."
        )
    if source.startswith("huggingface:"):
        raise FileNotFoundError(
            f"DaX HuggingFace weights not published yet: {source}. "
            "Use --features NPZ until benchboard release."
        )
    raise FileNotFoundError(f"Unknown DaX weight source: {source}")


class DaXEncoder(nn.Module):
    """ViT-L/16 pathology encoder. Loads timm backbone; optional DINOv3 init path."""

    def __init__(
        self,
        backbone: str = "vit_large_patch16_224",
        embed_dim: int = EMBED_DIM,
        freeze: bool = False,
        init_checkpoint: Optional[str] = None,
    ) -> None:
        super().__init__()
        try:
            import timm
        except ImportError as exc:
            raise ImportError("pip install timm for DaX encoder") from exc

        self.backbone = timm.create_model(
            backbone,
            pretrained=False,
            num_classes=0,
            global_pool="token",
        )
        feat_dim = self.backbone.num_features
        self.proj = nn.Identity() if feat_dim == embed_dim else nn.Linear(feat_dim, embed_dim)
        self.embed_dim = embed_dim

        if init_checkpoint:
            state = torch.load(init_checkpoint, map_location="cpu", weights_only=True)
            if "model" in state:
                state = state["model"]
            missing, unexpected = self.backbone.load_state_dict(state, strict=False)
            print(f"[DaX] loaded checkpoint missing={len(missing)} unexpected={len(unexpected)}")

        if freeze:
            for p in self.parameters():
                p.requires_grad = False

    def forward(self, x: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor]:
        """Return (cls_token, patch_tokens)."""
        tokens = self.backbone.forward_features(x)
        if tokens.ndim == 2:
            cls = self.proj(tokens)
            return cls, cls.unsqueeze(1)
        cls = self.proj(tokens[:, 0])
        patches = self.proj(tokens[:, 1:])
        return cls, patches

    @torch.inference_mode()
    def encode_patches(self, x: torch.Tensor) -> torch.Tensor:
        cls, _ = self.forward(x)
        return F.normalize(cls, dim=-1)
