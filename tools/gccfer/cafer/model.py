"""CA-FER model: ViViT backbone + culture-aware latent adaptation."""

from __future__ import annotations

from typing import Optional, Tuple

import torch
import torch.nn as nn

from .config import DROPOUT, EMBED_DIM, LATENT_DIM, NUM_CULTURES, NUM_EXPRESSIONS, VIVIT_MODEL
from .cultural_prior import CulturalEmbeddingTable, CultureAdaptation


class ViViTFeatureExtractor(nn.Module):
    """
    ViViT Model-3 style feature extraction (paper §III-C).
    Concatenates CLS token and mean-pooled patch tokens -> 768-d latent.
    """

    def __init__(self, model_name: str = VIVIT_MODEL, latent_dim: int = LATENT_DIM):
        super().__init__()
        from transformers import VivitModel

        self.backbone = VivitModel.from_pretrained(model_name)
        hidden = self.backbone.config.hidden_size
        self.proj = nn.Linear(hidden * 2, latent_dim)
        self.act = nn.ReLU(inplace=True)

    def forward(self, pixel_values: torch.Tensor) -> torch.Tensor:
        out = self.backbone(pixel_values=pixel_values)
        cls = out.last_hidden_state[:, 0]
        patches = out.last_hidden_state[:, 1:].mean(dim=1)
        fused = torch.cat([cls, patches], dim=-1)
        return self.act(self.proj(fused))


class CultureAgnosticFER(nn.Module):
    """Baseline without cultural adaptation."""

    def __init__(self, model_name: str = VIVIT_MODEL):
        super().__init__()
        self.encoder = ViViTFeatureExtractor(model_name)
        self.classifier = nn.Sequential(
            nn.Linear(LATENT_DIM, LATENT_DIM // 2),
            nn.ReLU(inplace=True),
            nn.Dropout(DROPOUT),
            nn.Linear(LATENT_DIM // 2, NUM_EXPRESSIONS),
        )

    def forward(self, pixel_values: torch.Tensor) -> torch.Tensor:
        f = self.encoder(pixel_values)
        return self.classifier(f)


class CAFER(nn.Module):
    """
    Culture-Aware FER (CA-FER): AU-grounded embeddings + FiLM adaptation (Eq. 12–15).
    """

    def __init__(
        self,
        model_name: str = VIVIT_MODEL,
        culture_embeddings: Optional[torch.Tensor] = None,
    ):
        super().__init__()
        self.encoder = ViViTFeatureExtractor(model_name)
        init = culture_embeddings.cpu().numpy() if culture_embeddings is not None else None
        self.culture_table = CulturalEmbeddingTable(init)
        self.adapt = CultureAdaptation(LATENT_DIM, EMBED_DIM)
        self.classifier = nn.Sequential(
            nn.Linear(LATENT_DIM, LATENT_DIM // 2),
            nn.ReLU(inplace=True),
            nn.Dropout(DROPOUT),
            nn.Linear(LATENT_DIM // 2, NUM_EXPRESSIONS),
        )

    def forward(
        self,
        pixel_values: torch.Tensor,
        culture_ids: Optional[torch.Tensor] = None,
        use_global: bool = False,
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        f = self.encoder(pixel_values)
        if use_global or culture_ids is None:
            ec = self.culture_table.global_embedding().unsqueeze(0).expand(f.size(0), -1)
        else:
            ec = self.culture_table(culture_ids)
        f_prime = self.adapt(f, ec)
        logits = self.classifier(f_prime)
        return logits, f_prime

    def load_culture_init(self, initial_embeddings):
        with torch.no_grad():
            self.culture_table.table.copy_(torch.from_numpy(initial_embeddings))
