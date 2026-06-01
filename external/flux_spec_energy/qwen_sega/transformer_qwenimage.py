# Copyright 2025 Qwen-Image Team, The HuggingFace Team. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Transformer adapted from Hugging Face diffusers (QwenImageTransformer2DModel) with SEGA.
# ---------------------------------------------------------------------------
# SEGA: Spectral-Energy Guided Attention for resolution extrapolation.
# ---------------------------------------------------------------------------

import functools
import math
from math import prod
from typing import Any, Dict, List, Optional, Tuple, Union

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from diffusers.configuration_utils import ConfigMixin, register_to_config
from diffusers.loaders import FromOriginalModelMixin, PeftAdapterMixin
from diffusers.utils import USE_PEFT_BACKEND, deprecate, logging, scale_lora_layers, unscale_lora_layers
from diffusers.utils.torch_utils import maybe_allow_in_graph
from diffusers.models._modeling_parallel import ContextParallelInput, ContextParallelOutput
from diffusers.models.attention import AttentionMixin, FeedForward
from diffusers.models.attention_dispatch import dispatch_attention_fn
from diffusers.models.attention_processor import Attention
from diffusers.models.cache_utils import CacheMixin
from diffusers.models.embeddings import TimestepEmbedding, Timesteps
from diffusers.models.modeling_outputs import Transformer2DModelOutput
from diffusers.models.modeling_utils import ModelMixin
from diffusers.models.normalization import AdaLayerNormContinuous, RMSNorm


logger = logging.get_logger(__name__)  # pylint: disable=invalid-name


def get_timestep_embedding(
    timesteps: torch.Tensor,
    embedding_dim: int,
    flip_sin_to_cos: bool = False,
    downscale_freq_shift: float = 1,
    scale: float = 1,
    max_period: int = 10000,
) -> torch.Tensor:
    """
    This matches the implementation in Denoising Diffusion Probabilistic Models: Create sinusoidal timestep embeddings.

    Args
        timesteps (torch.Tensor):
            a 1-D Tensor of N indices, one per batch element. These may be fractional.
        embedding_dim (int):
            the dimension of the output.
        flip_sin_to_cos (bool):
            Whether the embedding order should be `cos, sin` (if True) or `sin, cos` (if False)
        downscale_freq_shift (float):
            Controls the delta between frequencies between dimensions
        scale (float):
            Scaling factor applied to the embeddings.
        max_period (int):
            Controls the maximum frequency of the embeddings
    Returns
        torch.Tensor: an [N x dim] Tensor of positional embeddings.
    """
    assert len(timesteps.shape) == 1, "Timesteps should be a 1d-array"

    half_dim = embedding_dim // 2
    exponent = -math.log(max_period) * torch.arange(
        start=0, end=half_dim, dtype=torch.float32, device=timesteps.device
    )
    exponent = exponent / (half_dim - downscale_freq_shift)

    emb = torch.exp(exponent).to(timesteps.dtype)
    emb = timesteps[:, None].float() * emb[None, :]

    # scale embeddings
    emb = scale * emb

    # concat sine and cosine embeddings
    emb = torch.cat([torch.sin(emb), torch.cos(emb)], dim=-1)

    # flip sine and cosine embeddings
    if flip_sin_to_cos:
        emb = torch.cat([emb[:, half_dim:], emb[:, :half_dim]], dim=-1)

    # zero pad
    if embedding_dim % 2 == 1:
        emb = torch.nn.functional.pad(emb, (0, 1, 0, 0))
    return emb


def apply_rotary_emb_qwen(
    x: torch.Tensor,
    freqs_cis: Union[torch.Tensor, Tuple[torch.Tensor]],
    use_real: bool = True,
    use_real_unbind_dim: int = -1,
) -> Tuple[torch.Tensor, torch.Tensor]:
    """
    Apply rotary embeddings to input tensors using the given frequency tensor. This function applies rotary embeddings
    to the given query or key 'x' tensors using the provided frequency tensor 'freqs_cis'. The input tensors are
    reshaped as complex numbers, and the frequency tensor is reshaped for broadcasting compatibility. The resulting
    tensors contain rotary embeddings and are returned as real tensors.

    Args:
        x (`torch.Tensor`):
            Query or key tensor to apply rotary embeddings. [B, S, H, D] xk (torch.Tensor): Key tensor to apply
        freqs_cis (`Tuple[torch.Tensor]`): Precomputed frequency tensor for complex exponentials. ([S, D], [S, D],)

    Returns:
        Tuple[torch.Tensor, torch.Tensor]: Tuple of modified query tensor and key tensor with rotary embeddings.
    """
    if use_real:
        cos, sin = freqs_cis  # [S, D]
        cos = cos[None, None]
        sin = sin[None, None]
        cos, sin = cos.to(x.device), sin.to(x.device)

        if use_real_unbind_dim == -1:
            # Used for flux, cogvideox, hunyuan-dit
            x_real, x_imag = x.reshape(*x.shape[:-1], -1, 2).unbind(-1)  # [B, S, H, D//2]
            x_rotated = torch.stack([-x_imag, x_real], dim=-1).flatten(3)
        elif use_real_unbind_dim == -2:
            # Used for Stable Audio, OmniGen, CogView4 and Cosmos
            x_real, x_imag = x.reshape(*x.shape[:-1], 2, -1).unbind(-2)  # [B, S, H, D//2]
            x_rotated = torch.cat([-x_imag, x_real], dim=-1)
        else:
            raise ValueError(f"`use_real_unbind_dim={use_real_unbind_dim}` but should be -1 or -2.")

        out = (x.float() * cos + x_rotated.float() * sin).to(x.dtype)

        return out
    else:
        x_rotated = torch.view_as_complex(x.float().reshape(*x.shape[:-1], -1, 2))
        freqs_cis = freqs_cis.unsqueeze(1)
        x_out = torch.view_as_real(x_rotated * freqs_cis).flatten(3)

        return x_out.type_as(x)


def compute_text_seq_len_from_mask(
    encoder_hidden_states: torch.Tensor, encoder_hidden_states_mask: Optional[torch.Tensor]
) -> Tuple[int, Optional[torch.Tensor], Optional[torch.Tensor]]:
    """
    Compute text sequence length without assuming contiguous masks. Returns length for RoPE and a normalized bool mask.
    """
    batch_size, text_seq_len = encoder_hidden_states.shape[:2]
    if encoder_hidden_states_mask is None:
        return text_seq_len, None, None

    if encoder_hidden_states_mask.shape[:2] != (batch_size, text_seq_len):
        raise ValueError(
            f"`encoder_hidden_states_mask` shape {encoder_hidden_states_mask.shape} must match "
            f"(batch_size, text_seq_len)=({batch_size}, {text_seq_len})."
        )

    if encoder_hidden_states_mask.dtype != torch.bool:
        encoder_hidden_states_mask = encoder_hidden_states_mask.to(torch.bool)

    position_ids = torch.arange(text_seq_len, device=encoder_hidden_states.device, dtype=torch.long)
    active_positions = torch.where(encoder_hidden_states_mask, position_ids, position_ids.new_zeros(()))
    has_active = encoder_hidden_states_mask.any(dim=1)
    per_sample_len = torch.where(has_active, active_positions.max(dim=1).values + 1, torch.as_tensor(text_seq_len))
    return text_seq_len, per_sample_len, encoder_hidden_states_mask


# ---------------------------------------------------------------------------
# base_mscale — only power_res and log_res
# ---------------------------------------------------------------------------

def compute_base_mscale(
    target_res: int,
    training_res: int,
    formula: str = "power_res",
    coefficient: Optional[float] = None,
) -> float:
    s = max(float(target_res) / float(training_res), 1.0)
    if formula == "power_res":
        c = 0.1 if coefficient is None else float(coefficient)
        return s ** c
    if formula == "log_res":
        c = 0.1 if coefficient is None else float(coefficient)
        return 1.0 + c * math.log(s)
    raise ValueError(f"Unknown base_mscale formula: {formula!r}. Use 'power_res' or 'log_res'.")


# ---------------------------------------------------------------------------
# SEGA mscale
# ---------------------------------------------------------------------------

@torch.no_grad()
def compute_sega_allocation(
    energy_profile: torch.Tensor,
    freqs: torch.Tensor,
    base_mscale: float,
    spread: float,
    alpha: float = 0.15,
    beta: float = 1.5,
    min_mscale: float = 1.0,
) -> torch.Tensor:
    """
    Spectral-Energy Guided Attention (SEGA): per-dim RoPE mscale (asymmetric band).

        z_k = standardise( log E[bin(k)] )
        s_k = tanh( beta * z_k ),  re-centred
        m_k = base_mscale * ( 1 - alpha * spread * s_k )

    High spectral-energy dims get lower m_k (sharpness-biased).
    """
    D_half = freqs.shape[0]
    eps = 1e-8

    if spread <= 0.0 or alpha <= 0.0:
        return torch.full((D_half,), float(base_mscale), device=freqs.device, dtype=torch.float32)

    # Map each RoPE dim to its FFT bin via log-period
    periods = 2.0 * math.pi / freqs.clamp(min=eps)
    log_periods = torch.log(periods)
    min_lp, max_lp = log_periods.min(), log_periods.max()
    if (max_lp - min_lp).item() > 1e-6:
        lp_norm = (log_periods - min_lp) / (max_lp - min_lp)  # 0=high-freq, 1=low-freq
    else:
        lp_norm = torch.zeros_like(log_periods)

    n_bins = energy_profile.shape[0]
    bin_pos = (1.0 - lp_norm) * (n_bins - 1)
    j_low = bin_pos.floor().long().clamp(0, n_bins - 1)
    j_high = (j_low + 1).clamp(0, n_bins - 1)
    frac = (bin_pos - j_low.to(bin_pos.dtype)).clamp(0.0, 1.0)

    E = energy_profile.to(freqs.device).clamp(min=eps)
    log_E = torch.log(E)
    raw = log_E[j_low] * (1.0 - frac) + log_E[j_high] * frac

    # Standardise + tanh + re-centre
    z = raw - raw.mean()
    z = z / z.std().clamp(min=eps)
    s = torch.tanh(float(beta) * z)
    s = s - s.mean()

    # direction = -1  =>  subtract
    m = float(base_mscale) * (1.0 - float(alpha) * float(spread) * s)
    return m.clamp(min=float(min_mscale)).to(torch.float32)


# ---------------------------------------------------------------------------
# Spectral helpers
# ---------------------------------------------------------------------------

@torch.no_grad()
def compute_spectral_energy_profile(hidden_states, height, width, n_bins):
    """
    Radial spectral energy profile of the spatial latent.

    Reshapes the 1-D token sequence back to a 2-D grid, computes the 2-D FFT
    power spectrum, then bins by radial frequency to produce a compact 1-D
    energy profile from lowest to highest spatial frequency.
    """
    B, S, C = hidden_states.shape
    n_spatial = min(S, height * width)

    spatial = hidden_states[:, :n_spatial].reshape(B, height, width, C)
    spatial_map = spatial.float().mean(dim=(0, -1))
    spatial_map = spatial_map - spatial_map.mean()

    power = torch.fft.fftshift(torch.fft.fft2(spatial_map)).abs().pow(2)

    cy, cx = height / 2.0, width / 2.0
    y = torch.arange(height, device=power.device, dtype=torch.float32) - cy
    x = torch.arange(width, device=power.device, dtype=torch.float32) - cx
    yy, xx = torch.meshgrid(y, x, indexing="ij")
    radius_norm = torch.sqrt(yy ** 2 + xx ** 2)
    radius_norm = radius_norm / (radius_norm.max() + 1e-8)

    bin_idx = (radius_norm * n_bins).long().clamp(0, n_bins - 1).flatten()
    flat_pw = power.flatten()

    energy_sum = torch.zeros(n_bins, device=power.device, dtype=torch.float32)
    energy_cnt = torch.zeros(n_bins, device=power.device, dtype=torch.float32)
    energy_sum.scatter_add_(0, bin_idx, flat_pw)
    energy_cnt.scatter_add_(0, bin_idx, torch.ones_like(flat_pw))

    return energy_sum / (energy_cnt + 1e-8)


@torch.no_grad()
def compute_axis_spectral_profiles(hidden_states, height, width, n_bins_h, n_bins_w):
    """
    Per-axis (height, width) 1-D spectral energy profiles.

    Instead of a single radial profile, computes separate 1-D FFT profiles
    along height and width axes for SEGA per-axis mscale.
    """
    B, S, C = hidden_states.shape
    n_spatial = min(S, height * width)
    spatial = hidden_states[:, :n_spatial].reshape(B, height, width, C)
    sm = spatial.float().mean(dim=(0, -1))
    sm = sm - sm.mean()

    def _axis_profile(sm, axis, n_bins, length):
        fft = torch.fft.fft(sm, dim=axis)
        power = fft.abs().pow(2).mean(dim=1 - axis)
        half = length // 2 + 1
        power = power[:half]
        freq_norm = torch.linspace(0.0, 1.0, half, device=power.device)
        bin_idx = (freq_norm * n_bins).long().clamp(0, n_bins - 1)
        energy_sum = torch.zeros(n_bins, device=power.device, dtype=torch.float32)
        energy_cnt = torch.zeros(n_bins, device=power.device, dtype=torch.float32)
        energy_sum.scatter_add_(0, bin_idx, power.float())
        energy_cnt.scatter_add_(0, bin_idx, torch.ones_like(power, dtype=torch.float32))
        return energy_sum / (energy_cnt + 1e-8)

    return (
        _axis_profile(sm, axis=0, n_bins=n_bins_h, length=height),
        _axis_profile(sm, axis=1, n_bins=n_bins_w, length=width),
    )


@torch.no_grad()
def compute_dynamic_spread(energy_profile, spread_min=0.0, spread_max=1.0, alpha=1.5):
    """
    Derive optimal mscale_spread from the latent's radial spectral energy.

    Uses spectral concentration (1 - spectral_flatness) to decide how much
    to differentiate between high-freq and low-freq RoPE mscale values.

    The alpha parameter applies a non-linear mapping:
      spread = spread_min + (spread_max - spread_min) * (1 - (1 - c)^alpha)
    where c = concentration.  alpha > 1 is more aggressive (higher spread
    for moderate concentration).
    """
    eps = 1e-8
    energy = energy_profile.clamp(min=eps)

    geo_mean = torch.exp(torch.log(energy).mean())
    arith_mean = energy.mean()
    flatness = (geo_mean / (arith_mean + eps)).clamp(0.0, 1.0)

    concentration = 1.0 - flatness.item()
    return spread_min + (spread_max - spread_min) * (1.0 - (1.0 - concentration) ** alpha)


# ---------------------------------------------------------------------------
# Embeddings
# ---------------------------------------------------------------------------

class QwenTimestepProjEmbeddings(nn.Module):
    def __init__(self, embedding_dim, use_additional_t_cond=False):
        super().__init__()

        self.time_proj = Timesteps(num_channels=256, flip_sin_to_cos=True, downscale_freq_shift=0, scale=1000)
        self.timestep_embedder = TimestepEmbedding(in_channels=256, time_embed_dim=embedding_dim)
        self.use_additional_t_cond = use_additional_t_cond
        if use_additional_t_cond:
            self.addition_t_embedding = nn.Embedding(2, embedding_dim)

    def forward(self, timestep, hidden_states, addition_t_cond=None):
        timesteps_proj = self.time_proj(timestep)
        timesteps_emb = self.timestep_embedder(timesteps_proj.to(dtype=hidden_states.dtype))  # (N, D)

        conditioning = timesteps_emb
        if self.use_additional_t_cond:
            if addition_t_cond is None:
                raise ValueError("When additional_t_cond is True, addition_t_cond must be provided.")
            addition_t_emb = self.addition_t_embedding(addition_t_cond)
            addition_t_emb = addition_t_emb.to(dtype=hidden_states.dtype)
            conditioning = conditioning + addition_t_emb

        return conditioning


# ---------------------------------------------------------------------------
# RoPE with SEGA per-dim mscale
# ---------------------------------------------------------------------------

class QwenEmbedRope(nn.Module):
    def __init__(self, theta: int, axes_dim: List[int], scale_rope=False,
                 mscale_alpha: float = 0.15, mscale_beta: float = 1.5, mscale_min: float = 1.0,
                 base_mscale_formula: str = "power_res", base_mscale_coefficient: Optional[float] = None,
                 training_res_pixels: Optional[int] = None):
        super().__init__()
        self.theta = theta
        self.axes_dim = axes_dim
        self.mscale_alpha = mscale_alpha
        self.mscale_beta = mscale_beta
        self.mscale_min = mscale_min
        self.base_mscale_formula = base_mscale_formula
        self.base_mscale_coefficient = base_mscale_coefficient
        self.training_res_pixels = training_res_pixels
        self._current_ntk_factor_h = 1.0
        self._current_ntk_factor_w = 1.0

        self._build_freqs(ntk_factor_h=1.0, ntk_factor_w=1.0)

        # DO NOT USING REGISTER BUFFER HERE, IT WILL CAUSE COMPLEX NUMBERS LOSE ITS IMAGINARY PART
        self.scale_rope = scale_rope

    def _build_freqs(self, ntk_factor_h: float, ntk_factor_w: float):
        """(Re)build pos/neg frequency tensors for the given per-axis NTK factors."""
        pos_index = torch.arange(4096)
        neg_index = torch.arange(4096).flip(0) * -1 - 1
        self.pos_freqs = torch.cat(
            [
                self.rope_params(pos_index, self.axes_dim[0], self.theta),
                self.rope_params(pos_index, self.axes_dim[1], self.theta, ntk_factor=ntk_factor_h),
                self.rope_params(pos_index, self.axes_dim[2], self.theta, ntk_factor=ntk_factor_w),
            ],
            dim=1,
        )
        self.neg_freqs = torch.cat(
            [
                self.rope_params(neg_index, self.axes_dim[0], self.theta),
                self.rope_params(neg_index, self.axes_dim[1], self.theta, ntk_factor=ntk_factor_h),
                self.rope_params(neg_index, self.axes_dim[2], self.theta, ntk_factor=ntk_factor_w),
            ],
            dim=1,
        )

        if ntk_factor_h > 1.0:
            scaled_theta_h = self.theta * ntk_factor_h
            spatial_dim = self.axes_dim[1]
            self._inv_freqs_h = 1.0 / torch.pow(
                scaled_theta_h, torch.arange(0, spatial_dim, 2).float().div(spatial_dim)
            )
        if ntk_factor_w > 1.0:
            scaled_theta_w = self.theta * ntk_factor_w
            spatial_dim = self.axes_dim[2]
            self._inv_freqs_w = 1.0 / torch.pow(
                scaled_theta_w, torch.arange(0, spatial_dim, 2).float().div(spatial_dim)
            )

        self._current_ntk_factor_h = ntk_factor_h
        self._current_ntk_factor_w = ntk_factor_w
        if hasattr(self, '_compute_video_freqs'):
            self._compute_video_freqs.cache_clear()

    def _ensure_ntk_factor(self, ntk_factor_h: float, ntk_factor_w: float):
        """Rebuild frequencies if per-axis NTK factors changed."""
        if (abs(ntk_factor_h - self._current_ntk_factor_h) > 1e-8 or
                abs(ntk_factor_w - self._current_ntk_factor_w) > 1e-8):
            self._build_freqs(ntk_factor_h, ntk_factor_w)

    @staticmethod
    def rope_params(index, dim, theta=10000, ntk_factor=1.0):
        assert dim % 2 == 0
        scaled_theta = theta * ntk_factor
        freqs = torch.outer(index, 1.0 / torch.pow(scaled_theta, torch.arange(0, dim, 2).to(torch.float32).div(dim)))
        freqs = torch.polar(torch.ones_like(freqs), freqs)
        return freqs

    @torch.no_grad()
    def compute_mscale(self, ntk_factor_h, ntk_factor_w, mscale_spread,
                       energy_profile_h=None, energy_profile_w=None,
                       target_res_h=None, target_res_w=None, device=None):
        """
        Compute per-frequency-dimension mscale with SEGA (Spectral-Energy Guided Attention).

        Uses per-axis spectral energy profiles to set per-dim scaling
        factors via the SEGA formula. Returns None if no scaling needed,
        otherwise [D_total/2] real tensor.
        """
        if (ntk_factor_h <= 1.0 and ntk_factor_w <= 1.0) or mscale_spread is None:
            return None

        frame_half_dim = self.axes_dim[0] // 2

        # Height axis SEGA
        if ntk_factor_h > 1.0 and hasattr(self, '_inv_freqs_h'):
            base_ms_h = compute_base_mscale(
                target_res_h or self.training_res_pixels,
                self.training_res_pixels,
                self.base_mscale_formula,
                self.base_mscale_coefficient,
            )
            use_sega_h = (
                energy_profile_h is not None
                and float(mscale_spread) > 0.0
                and base_ms_h > 1.0 + 1e-8
            )
            if use_sega_h:
                inv_f = self._inv_freqs_h.to(device) if device is not None else self._inv_freqs_h
                mscale_h = compute_sega_allocation(
                    energy_profile_h, inv_f,
                    base_ms_h, float(mscale_spread),
                    float(self.mscale_alpha), float(self.mscale_beta), float(self.mscale_min),
                )
            else:
                dev = device or self._inv_freqs_h.device
                mscale_h = torch.full((self.axes_dim[1] // 2,), base_ms_h,
                                       device=dev, dtype=torch.float32)
        else:
            mscale_h = None

        # Width axis SEGA
        if ntk_factor_w > 1.0 and hasattr(self, '_inv_freqs_w'):
            base_ms_w = compute_base_mscale(
                target_res_w or self.training_res_pixels,
                self.training_res_pixels,
                self.base_mscale_formula,
                self.base_mscale_coefficient,
            )
            use_sega_w = (
                energy_profile_w is not None
                and float(mscale_spread) > 0.0
                and base_ms_w > 1.0 + 1e-8
            )
            if use_sega_w:
                inv_f = self._inv_freqs_w.to(device) if device is not None else self._inv_freqs_w
                mscale_w = compute_sega_allocation(
                    energy_profile_w, inv_f,
                    base_ms_w, float(mscale_spread),
                    float(self.mscale_alpha), float(self.mscale_beta), float(self.mscale_min),
                )
            else:
                dev = device or self._inv_freqs_w.device
                mscale_w = torch.full((self.axes_dim[2] // 2,), base_ms_w,
                                       device=dev, dtype=torch.float32)
        else:
            mscale_w = None

        if mscale_h is None and mscale_w is None:
            return None

        dev = device or (mscale_h.device if mscale_h is not None else mscale_w.device)
        if mscale_h is None:
            mscale_h = torch.ones(self.axes_dim[1] // 2, device=dev)
        if mscale_w is None:
            mscale_w = torch.ones(self.axes_dim[2] // 2, device=dev)

        mscale_h = mscale_w

        mscale_full = torch.cat([
            torch.ones(frame_half_dim, device=dev),
            mscale_h,
            mscale_w,
        ])
        vals = [f"{v:.4f}" for v in mscale_full.tolist()]
        # print(
        #     f"[SEGA] mscale (per-dim, {mscale_full.shape[0]} dims): "
        #     f"min={mscale_full.min().item():.4f}  "
        #     f"mean={mscale_full.mean().item():.4f}  "
        #     f"max={mscale_full.max().item():.4f}\n"
        #     f"[SEGA] mscale frame dims  0–7   (16/2 = 8 dims): {vals[:8]}\n"
        #     f"[SEGA] mscale height dims 8–35  (56/2 = 28 dims): {vals[8:36]}\n"
        #     f"[SEGA] mscale width dims 36–63  (56/2 = 28 dims): {vals[36:]}"
        # )
   
        return mscale_full

    def forward(
        self,
        video_fhw: Union[Tuple[int, int, int], List[Tuple[int, int, int]]],
        txt_seq_lens: Optional[List[int]] = None,
        device: torch.device = None,
        max_txt_seq_len: Optional[Union[int, torch.Tensor]] = None,
        ntk_factor_h: float = 1.0,
        ntk_factor_w: float = 1.0,
        mscale_spread: Optional[float] = None,
        energy_profile_h: Optional[torch.Tensor] = None,
        energy_profile_w: Optional[torch.Tensor] = None,
        target_res_h: Optional[int] = None,
        target_res_w: Optional[int] = None,
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        """
        Args:
            video_fhw: [frame, height, width] shape(s).
            txt_seq_lens: **Deprecated**. Use `max_txt_seq_len` instead.
            device: Device for RoPE computation.
            max_txt_seq_len: Maximum text sequence length for RoPE.
            ntk_factor_h: Dynamic NTK scaling factor for height axis.
            ntk_factor_w: Dynamic NTK scaling factor for width axis.
            mscale_spread: Dynamic spread for per-frequency mscale. None = no mscale.
            energy_profile_h: Per-axis spectral energy profile for height (from SEGA).
            energy_profile_w: Per-axis spectral energy profile for width (from SEGA).
            target_res_h: Target pixel resolution for height (for base_mscale).
            target_res_w: Target pixel resolution for width (for base_mscale).
        """
        if txt_seq_lens is not None:
            deprecate(
                "txt_seq_lens",
                "0.39.0",
                "Passing `txt_seq_lens` is deprecated and will be removed in version 0.39.0. "
                "Please use `max_txt_seq_len` instead. "
                "The new parameter accepts a single int or tensor value representing the maximum text sequence length.",
                standard_warn=False,
            )
            if max_txt_seq_len is None:
                max_txt_seq_len = max(txt_seq_lens) if isinstance(txt_seq_lens, list) else txt_seq_lens

        if max_txt_seq_len is None:
            raise ValueError("Either `max_txt_seq_len` or `txt_seq_lens` (deprecated) must be provided.")

        self._ensure_ntk_factor(ntk_factor_h, ntk_factor_w)

        if isinstance(video_fhw, list) and len(video_fhw) > 1:
            first_fhw = video_fhw[0]
            if not all(fhw == first_fhw for fhw in video_fhw):
                logger.warning(
                    "Batch inference with variable-sized images is not currently supported in QwenEmbedRope. "
                    "All images in the batch should have the same dimensions (frame, height, width). "
                    f"Detected sizes: {video_fhw}. Using the first image's dimensions {first_fhw} "
                    "for RoPE computation, which may lead to incorrect results for other images in the batch."
                )

        if isinstance(video_fhw, list):
            video_fhw = video_fhw[0]
        if not isinstance(video_fhw, list):
            video_fhw = [video_fhw]

        vid_freqs = []
        max_vid_index = 0
        for idx, fhw in enumerate(video_fhw):
            frame, height, width = fhw
            video_freq = self._compute_video_freqs(frame, height, width, idx, device)
            vid_freqs.append(video_freq)

            if self.scale_rope:
                max_vid_index = max(height // 2, width // 2, max_vid_index)
            else:
                max_vid_index = max(height, width, max_vid_index)

        max_txt_seq_len_int = int(max_txt_seq_len)
        txt_freqs = self.pos_freqs.to(device)[max_vid_index : max_vid_index + max_txt_seq_len_int, ...]
        vid_freqs = torch.cat(vid_freqs, dim=0)

        mscale = self.compute_mscale(
            ntk_factor_h, ntk_factor_w, mscale_spread,
            energy_profile_h, energy_profile_w,
            target_res_h, target_res_w, device,
        )
        if mscale is not None:
            vid_freqs = vid_freqs * mscale
            txt_freqs = txt_freqs * mscale

        return vid_freqs, txt_freqs

    @functools.lru_cache(maxsize=128)
    def _compute_video_freqs(
        self, frame: int, height: int, width: int, idx: int = 0, device: torch.device = None
    ) -> torch.Tensor:
        seq_lens = frame * height * width
        pos_freqs = self.pos_freqs.to(device) if device is not None else self.pos_freqs
        neg_freqs = self.neg_freqs.to(device) if device is not None else self.neg_freqs

        freqs_pos = pos_freqs.split([x // 2 for x in self.axes_dim], dim=1)
        freqs_neg = neg_freqs.split([x // 2 for x in self.axes_dim], dim=1)

        freqs_frame = freqs_pos[0][idx : idx + frame].view(frame, 1, 1, -1).expand(frame, height, width, -1)
        if self.scale_rope:
            freqs_height = torch.cat([freqs_neg[1][-(height - height // 2) :], freqs_pos[1][: height // 2]], dim=0)
            freqs_height = freqs_height.view(1, height, 1, -1).expand(frame, height, width, -1)
            freqs_width = torch.cat([freqs_neg[2][-(width - width // 2) :], freqs_pos[2][: width // 2]], dim=0)
            freqs_width = freqs_width.view(1, 1, width, -1).expand(frame, height, width, -1)
        else:
            freqs_height = freqs_pos[1][:height].view(1, height, 1, -1).expand(frame, height, width, -1)
            freqs_width = freqs_pos[2][:width].view(1, 1, width, -1).expand(frame, height, width, -1)

        freqs = torch.cat([freqs_frame, freqs_height, freqs_width], dim=-1).reshape(seq_lens, -1)
        return freqs.clone().contiguous()


class QwenEmbedLayer3DRope(nn.Module):
    def __init__(self, theta: int, axes_dim: List[int], scale_rope=False,
                 mscale_alpha: float = 0.15, mscale_beta: float = 1.5, mscale_min: float = 1.0,
                 base_mscale_formula: str = "power_res", base_mscale_coefficient: Optional[float] = None,
                 training_res_pixels: Optional[int] = None):
        super().__init__()
        self.theta = theta
        self.axes_dim = axes_dim
        self.mscale_alpha = mscale_alpha
        self.mscale_beta = mscale_beta
        self.mscale_min = mscale_min
        self.base_mscale_formula = base_mscale_formula
        self.base_mscale_coefficient = base_mscale_coefficient
        self.training_res_pixels = training_res_pixels
        self._current_ntk_factor_h = 1.0
        self._current_ntk_factor_w = 1.0

        self._build_freqs(ntk_factor_h=1.0, ntk_factor_w=1.0)
        self.scale_rope = scale_rope

    def _build_freqs(self, ntk_factor_h: float, ntk_factor_w: float):
        """(Re)build pos/neg frequency tensors for the given per-axis NTK factors."""
        pos_index = torch.arange(4096)
        neg_index = torch.arange(4096).flip(0) * -1 - 1
        self.pos_freqs = torch.cat(
            [
                self.rope_params(pos_index, self.axes_dim[0], self.theta),
                self.rope_params(pos_index, self.axes_dim[1], self.theta, ntk_factor=ntk_factor_h),
                self.rope_params(pos_index, self.axes_dim[2], self.theta, ntk_factor=ntk_factor_w),
            ],
            dim=1,
        )
        self.neg_freqs = torch.cat(
            [
                self.rope_params(neg_index, self.axes_dim[0], self.theta),
                self.rope_params(neg_index, self.axes_dim[1], self.theta, ntk_factor=ntk_factor_h),
                self.rope_params(neg_index, self.axes_dim[2], self.theta, ntk_factor=ntk_factor_w),
            ],
            dim=1,
        )

        if ntk_factor_h > 1.0:
            scaled_theta_h = self.theta * ntk_factor_h
            spatial_dim = self.axes_dim[1]
            self._inv_freqs_h = 1.0 / torch.pow(
                scaled_theta_h, torch.arange(0, spatial_dim, 2).float().div(spatial_dim)
            )
        if ntk_factor_w > 1.0:
            scaled_theta_w = self.theta * ntk_factor_w
            spatial_dim = self.axes_dim[2]
            self._inv_freqs_w = 1.0 / torch.pow(
                scaled_theta_w, torch.arange(0, spatial_dim, 2).float().div(spatial_dim)
            )

        self._current_ntk_factor_h = ntk_factor_h
        self._current_ntk_factor_w = ntk_factor_w
        if hasattr(self, '_compute_video_freqs'):
            self._compute_video_freqs.cache_clear()
        if hasattr(self, '_compute_condition_freqs'):
            self._compute_condition_freqs.cache_clear()

    def _ensure_ntk_factor(self, ntk_factor_h: float, ntk_factor_w: float):
        """Rebuild frequencies if per-axis NTK factors changed."""
        if (abs(ntk_factor_h - self._current_ntk_factor_h) > 1e-8 or
                abs(ntk_factor_w - self._current_ntk_factor_w) > 1e-8):
            self._build_freqs(ntk_factor_h, ntk_factor_w)

    @staticmethod
    def rope_params(index, dim, theta=10000, ntk_factor=1.0):
        assert dim % 2 == 0
        scaled_theta = theta * ntk_factor
        freqs = torch.outer(index, 1.0 / torch.pow(scaled_theta, torch.arange(0, dim, 2).to(torch.float32).div(dim)))
        freqs = torch.polar(torch.ones_like(freqs), freqs)
        return freqs

    @torch.no_grad()
    def compute_mscale(self, ntk_factor_h, ntk_factor_w, mscale_spread,
                       energy_profile_h=None, energy_profile_w=None,
                       target_res_h=None, target_res_w=None, device=None):
        """
        Compute per-frequency-dimension mscale with SEGA (Spectral-Energy Guided Attention).
        Returns None if no scaling needed, otherwise [D_total/2] real tensor.
        """
        if (ntk_factor_h <= 1.0 and ntk_factor_w <= 1.0) or mscale_spread is None:
            return None

        frame_half_dim = self.axes_dim[0] // 2

        # Height axis SEGA
        if ntk_factor_h > 1.0 and hasattr(self, '_inv_freqs_h'):
            base_ms_h = compute_base_mscale(
                target_res_h or self.training_res_pixels,
                self.training_res_pixels,
                self.base_mscale_formula,
                self.base_mscale_coefficient,
            )
            use_sega_h = (
                energy_profile_h is not None
                and float(mscale_spread) > 0.0
                and base_ms_h > 1.0 + 1e-8
            )
            if use_sega_h:
                inv_f = self._inv_freqs_h.to(device) if device is not None else self._inv_freqs_h
                mscale_h = compute_sega_allocation(
                    energy_profile_h, inv_f,
                    base_ms_h, float(mscale_spread),
                    float(self.mscale_alpha), float(self.mscale_beta), float(self.mscale_min),
                )
            else:
                dev = device or self._inv_freqs_h.device
                mscale_h = torch.full((self.axes_dim[1] // 2,), base_ms_h,
                                       device=dev, dtype=torch.float32)
        else:
            mscale_h = None

        # Width axis SEGA
        if ntk_factor_w > 1.0 and hasattr(self, '_inv_freqs_w'):
            base_ms_w = compute_base_mscale(
                target_res_w or self.training_res_pixels,
                self.training_res_pixels,
                self.base_mscale_formula,
                self.base_mscale_coefficient,
            )
            use_sega_w = (
                energy_profile_w is not None
                and float(mscale_spread) > 0.0
                and base_ms_w > 1.0 + 1e-8
            )
            if use_sega_w:
                inv_f = self._inv_freqs_w.to(device) if device is not None else self._inv_freqs_w
                mscale_w = compute_sega_allocation(
                    energy_profile_w, inv_f,
                    base_ms_w, float(mscale_spread),
                    float(self.mscale_alpha), float(self.mscale_beta), float(self.mscale_min),
                )
            else:
                dev = device or self._inv_freqs_w.device
                mscale_w = torch.full((self.axes_dim[2] // 2,), base_ms_w,
                                       device=dev, dtype=torch.float32)
        else:
            mscale_w = None

        if mscale_h is None and mscale_w is None:
            return None

        dev = device or (mscale_h.device if mscale_h is not None else mscale_w.device)
        if mscale_h is None:
            mscale_h = torch.ones(self.axes_dim[1] // 2, device=dev)
        if mscale_w is None:
            mscale_w = torch.ones(self.axes_dim[2] // 2, device=dev)

        mscale_full = torch.cat([
            torch.ones(frame_half_dim, device=dev),
            mscale_h,
            mscale_w,
        ])
        vals = [f"{v:.4f}" for v in mscale_full.tolist()]
        # print(
        #     f"[SEGA] mscale (per-dim, {mscale_full.shape[0]} dims): "
        #     f"min={mscale_full.min().item():.4f}  "
        #     f"mean={mscale_full.mean().item():.4f}  "
        #     f"max={mscale_full.max().item():.4f}\n"
        #     f"[SEGA] mscale values: {vals}"
        # )
        return mscale_full

    def forward(
        self,
        video_fhw: Union[Tuple[int, int, int], List[Tuple[int, int, int]]],
        max_txt_seq_len: Union[int, torch.Tensor],
        device: torch.device = None,
        ntk_factor_h: float = 1.0,
        ntk_factor_w: float = 1.0,
        mscale_spread: Optional[float] = None,
        energy_profile_h: Optional[torch.Tensor] = None,
        energy_profile_w: Optional[torch.Tensor] = None,
        target_res_h: Optional[int] = None,
        target_res_w: Optional[int] = None,
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        """
        Args:
            video_fhw: [frame, height, width] shape(s) or list of layer structures.
            max_txt_seq_len: Maximum text sequence length for RoPE.
            device: Device for RoPE computation.
            ntk_factor_h: Dynamic NTK scaling factor for height axis.
            ntk_factor_w: Dynamic NTK scaling factor for width axis.
            mscale_spread: Dynamic spread for per-frequency mscale. None = no mscale.
            energy_profile_h: Per-axis spectral energy profile for height (from SEGA).
            energy_profile_w: Per-axis spectral energy profile for width (from SEGA).
            target_res_h: Target pixel resolution for height (for base_mscale).
            target_res_w: Target pixel resolution for width (for base_mscale).
        """
        self._ensure_ntk_factor(ntk_factor_h, ntk_factor_w)

        if isinstance(video_fhw, list) and len(video_fhw) > 1:
            first_entry = video_fhw[0]
            if not all(entry == first_entry for entry in video_fhw):
                logger.warning(
                    "Batch inference with variable-sized images is not currently supported in QwenEmbedLayer3DRope. "
                    "All images in the batch should have the same layer structure. "
                    f"Detected sizes: {video_fhw}. Using the first image's layer structure {first_entry} "
                    "for RoPE computation, which may lead to incorrect results for other images in the batch."
                )

        if isinstance(video_fhw, list):
            video_fhw = video_fhw[0]
        if not isinstance(video_fhw, list):
            video_fhw = [video_fhw]

        vid_freqs = []
        max_vid_index = 0
        layer_num = len(video_fhw) - 1
        for idx, fhw in enumerate(video_fhw):
            frame, height, width = fhw
            if idx != layer_num:
                video_freq = self._compute_video_freqs(frame, height, width, idx, device)
            else:
                video_freq = self._compute_condition_freqs(frame, height, width, device)
            vid_freqs.append(video_freq)

            if self.scale_rope:
                max_vid_index = max(height // 2, width // 2, max_vid_index)
            else:
                max_vid_index = max(height, width, max_vid_index)

        max_vid_index = max(max_vid_index, layer_num)
        max_txt_seq_len_int = int(max_txt_seq_len)
        txt_freqs = self.pos_freqs.to(device)[max_vid_index : max_vid_index + max_txt_seq_len_int, ...]
        vid_freqs = torch.cat(vid_freqs, dim=0)

        mscale = self.compute_mscale(
            ntk_factor_h, ntk_factor_w, mscale_spread,
            energy_profile_h, energy_profile_w,
            target_res_h, target_res_w, device,
        )
        if mscale is not None:
            vid_freqs = vid_freqs * mscale
            txt_freqs = txt_freqs * mscale

        return vid_freqs, txt_freqs

    @functools.lru_cache(maxsize=None)
    def _compute_video_freqs(self, frame, height, width, idx=0, device: torch.device = None):
        seq_lens = frame * height * width
        pos_freqs = self.pos_freqs.to(device) if device is not None else self.pos_freqs
        neg_freqs = self.neg_freqs.to(device) if device is not None else self.neg_freqs

        freqs_pos = pos_freqs.split([x // 2 for x in self.axes_dim], dim=1)
        freqs_neg = neg_freqs.split([x // 2 for x in self.axes_dim], dim=1)

        freqs_frame = freqs_pos[0][idx : idx + frame].view(frame, 1, 1, -1).expand(frame, height, width, -1)
        if self.scale_rope:
            freqs_height = torch.cat([freqs_neg[1][-(height - height // 2) :], freqs_pos[1][: height // 2]], dim=0)
            freqs_height = freqs_height.view(1, height, 1, -1).expand(frame, height, width, -1)
            freqs_width = torch.cat([freqs_neg[2][-(width - width // 2) :], freqs_pos[2][: width // 2]], dim=0)
            freqs_width = freqs_width.view(1, 1, width, -1).expand(frame, height, width, -1)
        else:
            freqs_height = freqs_pos[1][:height].view(1, height, 1, -1).expand(frame, height, width, -1)
            freqs_width = freqs_pos[2][:width].view(1, 1, width, -1).expand(frame, height, width, -1)

        freqs = torch.cat([freqs_frame, freqs_height, freqs_width], dim=-1).reshape(seq_lens, -1)
        return freqs.clone().contiguous()

    @functools.lru_cache(maxsize=None)
    def _compute_condition_freqs(self, frame, height, width, device: torch.device = None):
        seq_lens = frame * height * width
        pos_freqs = self.pos_freqs.to(device) if device is not None else self.pos_freqs
        neg_freqs = self.neg_freqs.to(device) if device is not None else self.neg_freqs

        freqs_pos = pos_freqs.split([x // 2 for x in self.axes_dim], dim=1)
        freqs_neg = neg_freqs.split([x // 2 for x in self.axes_dim], dim=1)

        freqs_frame = freqs_neg[0][-1:].view(frame, 1, 1, -1).expand(frame, height, width, -1)
        if self.scale_rope:
            freqs_height = torch.cat([freqs_neg[1][-(height - height // 2) :], freqs_pos[1][: height // 2]], dim=0)
            freqs_height = freqs_height.view(1, height, 1, -1).expand(frame, height, width, -1)
            freqs_width = torch.cat([freqs_neg[2][-(width - width // 2) :], freqs_pos[2][: width // 2]], dim=0)
            freqs_width = freqs_width.view(1, 1, width, -1).expand(frame, height, width, -1)
        else:
            freqs_height = freqs_pos[1][:height].view(1, height, 1, -1).expand(frame, height, width, -1)
            freqs_width = freqs_pos[2][:width].view(1, 1, width, -1).expand(frame, height, width, -1)

        freqs = torch.cat([freqs_frame, freqs_height, freqs_width], dim=-1).reshape(seq_lens, -1)
        return freqs.clone().contiguous()


# ---------------------------------------------------------------------------
# Attention
# ---------------------------------------------------------------------------

class QwenDoubleStreamAttnProcessor2_0:
    """
    Attention processor for Qwen double-stream architecture, matching DoubleStreamLayerMegatron logic. This processor
    implements joint attention computation where text and image streams are processed together.
    """

    _attention_backend = None
    _parallel_config = None

    def __init__(self):
        if not hasattr(F, "scaled_dot_product_attention"):
            raise ImportError(
                "QwenDoubleStreamAttnProcessor2_0 requires PyTorch 2.0, to use it, please upgrade PyTorch to 2.0."
            )

    def __call__(
        self,
        attn: Attention,
        hidden_states: torch.FloatTensor,  # Image stream
        encoder_hidden_states: torch.FloatTensor = None,  # Text stream
        encoder_hidden_states_mask: torch.FloatTensor = None,
        attention_mask: Optional[torch.FloatTensor] = None,
        image_rotary_emb: Optional[torch.Tensor] = None,
    ) -> torch.FloatTensor:
        if encoder_hidden_states is None:
            raise ValueError("QwenDoubleStreamAttnProcessor2_0 requires encoder_hidden_states (text stream)")

        seq_txt = encoder_hidden_states.shape[1]

        # Compute QKV for image stream (sample projections)
        img_query = attn.to_q(hidden_states)
        img_key = attn.to_k(hidden_states)
        img_value = attn.to_v(hidden_states)

        # Compute QKV for text stream (context projections)
        txt_query = attn.add_q_proj(encoder_hidden_states)
        txt_key = attn.add_k_proj(encoder_hidden_states)
        txt_value = attn.add_v_proj(encoder_hidden_states)

        # Reshape for multi-head attention
        img_query = img_query.unflatten(-1, (attn.heads, -1))
        img_key = img_key.unflatten(-1, (attn.heads, -1))
        img_value = img_value.unflatten(-1, (attn.heads, -1))

        txt_query = txt_query.unflatten(-1, (attn.heads, -1))
        txt_key = txt_key.unflatten(-1, (attn.heads, -1))
        txt_value = txt_value.unflatten(-1, (attn.heads, -1))

        # Apply QK normalization
        if attn.norm_q is not None:
            img_query = attn.norm_q(img_query)
        if attn.norm_k is not None:
            img_key = attn.norm_k(img_key)
        if attn.norm_added_q is not None:
            txt_query = attn.norm_added_q(txt_query)
        if attn.norm_added_k is not None:
            txt_key = attn.norm_added_k(txt_key)

        # Apply RoPE
        if image_rotary_emb is not None:
            img_freqs, txt_freqs = image_rotary_emb
            img_query = apply_rotary_emb_qwen(img_query, img_freqs, use_real=False)
            img_key = apply_rotary_emb_qwen(img_key, img_freqs, use_real=False)
            txt_query = apply_rotary_emb_qwen(txt_query, txt_freqs, use_real=False)
            txt_key = apply_rotary_emb_qwen(txt_key, txt_freqs, use_real=False)

        # Concatenate for joint attention
        # Order: [text, image]
        joint_query = torch.cat([txt_query, img_query], dim=1)
        joint_key = torch.cat([txt_key, img_key], dim=1)
        joint_value = torch.cat([txt_value, img_value], dim=1)

        joint_hidden_states = dispatch_attention_fn(
            joint_query,
            joint_key,
            joint_value,
            attn_mask=attention_mask,
            dropout_p=0.0,
            is_causal=False,
            backend=self._attention_backend,
            parallel_config=self._parallel_config,
        )

        # Reshape back
        joint_hidden_states = joint_hidden_states.flatten(2, 3)
        joint_hidden_states = joint_hidden_states.to(joint_query.dtype)

        # Split attention outputs back
        txt_attn_output = joint_hidden_states[:, :seq_txt, :]  # Text part
        img_attn_output = joint_hidden_states[:, seq_txt:, :]  # Image part

        # Apply output projections
        img_attn_output = attn.to_out[0](img_attn_output)
        if len(attn.to_out) > 1:
            img_attn_output = attn.to_out[1](img_attn_output)  # dropout

        txt_attn_output = attn.to_add_out(txt_attn_output)

        return img_attn_output, txt_attn_output


# ---------------------------------------------------------------------------
# Transformer block
# ---------------------------------------------------------------------------

@maybe_allow_in_graph
class QwenImageTransformerBlock(nn.Module):
    def __init__(
        self,
        dim: int,
        num_attention_heads: int,
        attention_head_dim: int,
        qk_norm: str = "rms_norm",
        eps: float = 1e-6,
        zero_cond_t: bool = False,
    ):
        super().__init__()

        self.dim = dim
        self.num_attention_heads = num_attention_heads
        self.attention_head_dim = attention_head_dim

        # Image processing modules
        self.img_mod = nn.Sequential(
            nn.SiLU(),
            nn.Linear(dim, 6 * dim, bias=True),  # For scale, shift, gate for norm1 and norm2
        )
        self.img_norm1 = nn.LayerNorm(dim, elementwise_affine=False, eps=eps)
        self.attn = Attention(
            query_dim=dim,
            cross_attention_dim=None,  # Enable cross attention for joint computation
            added_kv_proj_dim=dim,  # Enable added KV projections for text stream
            dim_head=attention_head_dim,
            heads=num_attention_heads,
            out_dim=dim,
            context_pre_only=False,
            bias=True,
            processor=QwenDoubleStreamAttnProcessor2_0(),
            qk_norm=qk_norm,
            eps=eps,
        )
        self.img_norm2 = nn.LayerNorm(dim, elementwise_affine=False, eps=eps)
        self.img_mlp = FeedForward(dim=dim, dim_out=dim, activation_fn="gelu-approximate")

        # Text processing modules
        self.txt_mod = nn.Sequential(
            nn.SiLU(),
            nn.Linear(dim, 6 * dim, bias=True),  # For scale, shift, gate for norm1 and norm2
        )
        self.txt_norm1 = nn.LayerNorm(dim, elementwise_affine=False, eps=eps)
        # Text doesn't need separate attention - it's handled by img_attn joint computation
        self.txt_norm2 = nn.LayerNorm(dim, elementwise_affine=False, eps=eps)
        self.txt_mlp = FeedForward(dim=dim, dim_out=dim, activation_fn="gelu-approximate")

        self.zero_cond_t = zero_cond_t

    def _modulate(self, x, mod_params, index=None):
        """Apply modulation to input tensor"""
        # x: b l d, shift: b d, scale: b d, gate: b d
        shift, scale, gate = mod_params.chunk(3, dim=-1)

        if index is not None:
            # Assuming mod_params batch dim is 2*actual_batch (chunked into 2 parts)
            # So shift, scale, gate have shape [2*actual_batch, d]
            actual_batch = shift.size(0) // 2
            shift_0, shift_1 = shift[:actual_batch], shift[actual_batch:]  # each: [actual_batch, d]
            scale_0, scale_1 = scale[:actual_batch], scale[actual_batch:]
            gate_0, gate_1 = gate[:actual_batch], gate[actual_batch:]

            # index: [b, l] where b is actual batch size
            # Expand to [b, l, 1] to match feature dimension
            index_expanded = index.unsqueeze(-1)  # [b, l, 1]

            # Expand chunks to [b, 1, d] then broadcast to [b, l, d]
            shift_0_exp = shift_0.unsqueeze(1)  # [b, 1, d]
            shift_1_exp = shift_1.unsqueeze(1)  # [b, 1, d]
            scale_0_exp = scale_0.unsqueeze(1)
            scale_1_exp = scale_1.unsqueeze(1)
            gate_0_exp = gate_0.unsqueeze(1)
            gate_1_exp = gate_1.unsqueeze(1)

            # Use torch.where to select based on index
            shift_result = torch.where(index_expanded == 0, shift_0_exp, shift_1_exp)
            scale_result = torch.where(index_expanded == 0, scale_0_exp, scale_1_exp)
            gate_result = torch.where(index_expanded == 0, gate_0_exp, gate_1_exp)
        else:
            shift_result = shift.unsqueeze(1)
            scale_result = scale.unsqueeze(1)
            gate_result = gate.unsqueeze(1)

        return x * (1 + scale_result) + shift_result, gate_result

    def forward(
        self,
        hidden_states: torch.Tensor,
        encoder_hidden_states: torch.Tensor,
        encoder_hidden_states_mask: torch.Tensor,
        temb: torch.Tensor,
        image_rotary_emb: Optional[Tuple[torch.Tensor, torch.Tensor]] = None,
        joint_attention_kwargs: Optional[Dict[str, Any]] = None,
        modulate_index: Optional[List[int]] = None,
    ) -> Tuple[torch.Tensor, torch.Tensor]:
        # Get modulation parameters for both streams
        img_mod_params = self.img_mod(temb)  # [B, 6*dim]

        if self.zero_cond_t:
            temb = torch.chunk(temb, 2, dim=0)[0]
        txt_mod_params = self.txt_mod(temb)  # [B, 6*dim]

        # Split modulation parameters for norm1 and norm2
        img_mod1, img_mod2 = img_mod_params.chunk(2, dim=-1)  # Each [B, 3*dim]
        txt_mod1, txt_mod2 = txt_mod_params.chunk(2, dim=-1)  # Each [B, 3*dim]

        # Process image stream - norm1 + modulation
        img_normed = self.img_norm1(hidden_states)
        img_modulated, img_gate1 = self._modulate(img_normed, img_mod1, modulate_index)

        # Process text stream - norm1 + modulation
        txt_normed = self.txt_norm1(encoder_hidden_states)
        txt_modulated, txt_gate1 = self._modulate(txt_normed, txt_mod1)

        # Use QwenAttnProcessor2_0 for joint attention computation
        # This directly implements the DoubleStreamLayerMegatron logic:
        # 1. Computes QKV for both streams
        # 2. Applies QK normalization and RoPE
        # 3. Concatenates and runs joint attention
        # 4. Splits results back to separate streams
        joint_attention_kwargs = joint_attention_kwargs or {}
        attn_output = self.attn(
            hidden_states=img_modulated,  # Image stream (will be processed as "sample")
            encoder_hidden_states=txt_modulated,  # Text stream (will be processed as "context")
            encoder_hidden_states_mask=encoder_hidden_states_mask,
            image_rotary_emb=image_rotary_emb,
            **joint_attention_kwargs,
        )

        # QwenAttnProcessor2_0 returns (img_output, txt_output) when encoder_hidden_states is provided
        img_attn_output, txt_attn_output = attn_output

        # Apply attention gates and add residual (like in Megatron)
        hidden_states = hidden_states + img_gate1 * img_attn_output
        encoder_hidden_states = encoder_hidden_states + txt_gate1 * txt_attn_output

        # Process image stream - norm2 + MLP
        img_normed2 = self.img_norm2(hidden_states)
        img_modulated2, img_gate2 = self._modulate(img_normed2, img_mod2, modulate_index)
        img_mlp_output = self.img_mlp(img_modulated2)
        hidden_states = hidden_states + img_gate2 * img_mlp_output

        # Process text stream - norm2 + MLP
        txt_normed2 = self.txt_norm2(encoder_hidden_states)
        txt_modulated2, txt_gate2 = self._modulate(txt_normed2, txt_mod2)
        txt_mlp_output = self.txt_mlp(txt_modulated2)
        encoder_hidden_states = encoder_hidden_states + txt_gate2 * txt_mlp_output

        # Clip to prevent overflow for fp16
        if encoder_hidden_states.dtype == torch.float16:
            encoder_hidden_states = encoder_hidden_states.clip(-65504, 65504)
        if hidden_states.dtype == torch.float16:
            hidden_states = hidden_states.clip(-65504, 65504)

        return encoder_hidden_states, hidden_states


# ---------------------------------------------------------------------------
# Transformer
# ---------------------------------------------------------------------------

class QwenImageTransformer2DModel(
    ModelMixin, ConfigMixin, PeftAdapterMixin, FromOriginalModelMixin, CacheMixin, AttentionMixin
):
    """
    The Transformer model introduced in Qwen, with SEGA (Spectral-Energy
    Guided Attention) for dynamic per-frequency RoPE mscale.
    """

    _supports_gradient_checkpointing = True
    _no_split_modules = ["QwenImageTransformerBlock"]
    _skip_layerwise_casting_patterns = ["pos_embed", "norm"]
    _repeated_blocks = ["QwenImageTransformerBlock"]
    # Make CP plan compatible with https://github.com/huggingface/diffusers/pull/12702
    _cp_plan = {
        "transformer_blocks.0": {
            "hidden_states": ContextParallelInput(split_dim=1, expected_dims=3, split_output=False),
            "encoder_hidden_states": ContextParallelInput(split_dim=1, expected_dims=3, split_output=False),
        },
        "transformer_blocks.*": {
            "modulate_index": ContextParallelInput(split_dim=1, expected_dims=2, split_output=False),
        },
        "pos_embed": {
            0: ContextParallelInput(split_dim=0, expected_dims=2, split_output=True),
            1: ContextParallelInput(split_dim=0, expected_dims=2, split_output=True),
        },
        "proj_out": ContextParallelOutput(gather_dim=1, expected_dims=3),
    }

    @register_to_config
    def __init__(
        self,
        patch_size: int = 2,
        in_channels: int = 64,
        out_channels: Optional[int] = 16,
        num_layers: int = 60,
        attention_head_dim: int = 128,
        num_attention_heads: int = 24,
        joint_attention_dim: int = 3584,
        guidance_embeds: bool = False,
        axes_dims_rope: Tuple[int, int, int] = (16, 56, 56),
        zero_cond_t: bool = False,
        use_additional_t_cond: bool = False,
        use_layer3d_rope: bool = False,
        training_resolution: int = 1024,
        spread_min: float = 0.0,
        spread_max: float = 1.0,
        spread_alpha: float = 1.5,
        mscale_alpha: float = 0.15,
        mscale_beta: float = 1.5,
        mscale_min: float = 1.0,
        base_mscale_formula: str = "power_res",
        base_mscale_coefficient: float = 0.08,
    ):
        """
        SEGA — Spectral-Energy Guided Attention with Dynamic NTK.

        NTK factor is computed dynamically in forward() based on actual
        target_resolution, with per-axis scaling and a damping term:
            s = target_resolution / training_resolution
            ntk = s^(2d/(d-2)) / (1 + log(s))

        Uses per-step FFT spectral analysis of the latent to dynamically
        determine per-axis SEGA mscale via:
            z_k = standardise( log E[bin(k)] )
            m_k = base_mscale * (1 - alpha * spread * tanh(beta * z_k))

        Parameters:
            training_resolution: Training resolution in pixels.
            spread_min: Minimum spread when spectrum is flat (noise).
            spread_max: Maximum spread when spectrum is concentrated (structure).
            spread_alpha: Non-linear mapping exponent for spread schedule.
            mscale_alpha: SEGA alpha controlling mscale amplitude.
            mscale_beta: SEGA beta controlling tanh sharpness.
            mscale_min: Floor for per-frequency mscale values.
            base_mscale_formula: 'power_res' or 'log_res' for base mscale.
            base_mscale_coefficient: Coefficient for base mscale formula (default 0.08).
        """
        super().__init__()
        self.out_channels = out_channels or in_channels
        self.inner_dim = num_attention_heads * attention_head_dim

        self.spread_min = spread_min
        self.spread_max = spread_max
        self.spread_alpha = spread_alpha

        self.training_resolution = training_resolution
        self._rope_spatial_dim = axes_dims_rope[1]

        if not use_layer3d_rope:
            self.pos_embed = QwenEmbedRope(
                theta=10000,
                axes_dim=list(axes_dims_rope),
                scale_rope=True,
                mscale_alpha=mscale_alpha,
                mscale_beta=mscale_beta,
                mscale_min=mscale_min,
                base_mscale_formula=base_mscale_formula,
                base_mscale_coefficient=base_mscale_coefficient,
                training_res_pixels=training_resolution,
            )
        else:
            self.pos_embed = QwenEmbedLayer3DRope(
                theta=10000,
                axes_dim=list(axes_dims_rope),
                scale_rope=True,
                mscale_alpha=mscale_alpha,
                mscale_beta=mscale_beta,
                mscale_min=mscale_min,
                base_mscale_formula=base_mscale_formula,
                base_mscale_coefficient=base_mscale_coefficient,
                training_res_pixels=training_resolution,
            )

        self.time_text_embed = QwenTimestepProjEmbeddings(
            embedding_dim=self.inner_dim, use_additional_t_cond=use_additional_t_cond
        )

        self.txt_norm = RMSNorm(joint_attention_dim, eps=1e-6)

        self.img_in = nn.Linear(in_channels, self.inner_dim)
        self.txt_in = nn.Linear(joint_attention_dim, self.inner_dim)

        self.transformer_blocks = nn.ModuleList(
            [
                QwenImageTransformerBlock(
                    dim=self.inner_dim,
                    num_attention_heads=num_attention_heads,
                    attention_head_dim=attention_head_dim,
                    zero_cond_t=zero_cond_t,
                )
                for _ in range(num_layers)
            ]
        )

        self.norm_out = AdaLayerNormContinuous(self.inner_dim, self.inner_dim, elementwise_affine=False, eps=1e-6)
        self.proj_out = nn.Linear(self.inner_dim, patch_size * patch_size * self.out_channels, bias=True)

        self.gradient_checkpointing = False
        self.zero_cond_t = zero_cond_t

    def forward(
        self,
        hidden_states: torch.Tensor,
        encoder_hidden_states: torch.Tensor = None,
        encoder_hidden_states_mask: torch.Tensor = None,
        timestep: torch.LongTensor = None,
        img_shapes: Optional[List[Tuple[int, int, int]]] = None,
        txt_seq_lens: Optional[List[int]] = None,
        guidance: torch.Tensor = None,
        attention_kwargs: Optional[Dict[str, Any]] = None,
        controlnet_block_samples=None,
        additional_t_cond=None,
        return_dict: bool = True,
        target_resolution: Optional[int] = None,
    ) -> Union[torch.Tensor, Transformer2DModelOutput]:
        """
        The [`QwenTransformer2DModel`] forward method.

        Args:
            hidden_states (`torch.Tensor` of shape `(batch_size, image_sequence_length, in_channels)`):
                Input `hidden_states`.
            encoder_hidden_states (`torch.Tensor` of shape `(batch_size, text_sequence_length, joint_attention_dim)`):
                Conditional embeddings (embeddings computed from the input conditions such as prompts) to use.
            encoder_hidden_states_mask (`torch.Tensor` of shape `(batch_size, text_sequence_length)`, *optional*):
                Mask for the encoder hidden states.
            timestep ( `torch.LongTensor`):
                Used to indicate denoising step.
            img_shapes (`List[Tuple[int, int, int]]`, *optional*):
                Image shapes for RoPE computation.
            txt_seq_lens (`List[int]`, *optional*, **Deprecated**):
                Deprecated parameter. Use `encoder_hidden_states_mask` instead.
            guidance (`torch.Tensor`, *optional*):
                Guidance tensor for conditional generation.
            attention_kwargs (`dict`, *optional*):
                A kwargs dictionary passed along to the `AttentionProcessor`.
            controlnet_block_samples (*optional*):
                ControlNet block samples to add to the transformer blocks.
            return_dict (`bool`, *optional*, defaults to `True`):
                Whether or not to return a [`~models.transformer_2d.Transformer2DModelOutput`].

        Returns:
            If `return_dict` is True, an [`~models.transformer_2d.Transformer2DModelOutput`] is returned, otherwise a
            `tuple` where the first element is the sample tensor.
        """
        if txt_seq_lens is not None:
            deprecate(
                "txt_seq_lens",
                "0.39.0",
                "Passing `txt_seq_lens` is deprecated and will be removed in version 0.39.0. "
                "Please use `encoder_hidden_states_mask` instead. "
                "The mask-based approach is more flexible and supports variable-length sequences.",
                standard_warn=False,
            )
        if attention_kwargs is not None:
            attention_kwargs = attention_kwargs.copy()
            lora_scale = attention_kwargs.pop("scale", 1.0)
        else:
            lora_scale = 1.0

        if USE_PEFT_BACKEND:
            # weight the lora layers by setting `lora_scale` for each PEFT layer
            scale_lora_layers(self, lora_scale)
        else:
            if attention_kwargs is not None and attention_kwargs.get("scale", None) is not None:
                logger.warning(
                    "Passing `scale` via `joint_attention_kwargs` when not using the PEFT backend is ineffective."
                )

        hidden_states = self.img_in(hidden_states)

        timestep = timestep.to(hidden_states.dtype)

        if self.zero_cond_t:
            timestep = torch.cat([timestep, timestep * 0], dim=0)
            modulate_index = torch.tensor(
                [[0] * prod(sample[0]) + [1] * sum([prod(s) for s in sample[1:]]) for sample in img_shapes],
                device=timestep.device,
                dtype=torch.int,
            )
        else:
            modulate_index = None

        encoder_hidden_states = self.txt_norm(encoder_hidden_states)
        encoder_hidden_states = self.txt_in(encoder_hidden_states)

        # Use the encoder_hidden_states sequence length for RoPE computation and normalize mask
        text_seq_len, _, encoder_hidden_states_mask = compute_text_seq_len_from_mask(
            encoder_hidden_states, encoder_hidden_states_mask
        )

        if guidance is not None:
            guidance = guidance.to(hidden_states.dtype) * 1000

        temb = (
            self.time_text_embed(timestep, hidden_states, additional_t_cond)
            if guidance is None
            else self.time_text_embed(timestep, guidance, hidden_states, additional_t_cond)
        )

        # ================================================================
        # Compute per-axis NTK factors from actual height/width
        #
        # Fixed: uses d/(d-2) exponent (not 2d/(d-2)).  The old doubled
        # exponent treated the 2-D area ratio s^2 as a 1-D context
        # scaling, but RoPE operates independently on each spatial axis,
        # so each axis only needs scaling for its own extrapolation ratio.
        #
        # Fixed: per-axis scaling instead of max(h,w) for both axes.
        # Using max(h,w) over-scales the shorter dimension, causing the
        # vertical stretching artifact on landscape / non-square images.
        # ================================================================
        if img_shapes is not None:
            _shapes = img_shapes[0] if isinstance(img_shapes, list) else img_shapes
            if isinstance(_shapes, (list, tuple)) and isinstance(_shapes[0], (list, tuple)):
                _shapes = _shapes[0]
            _, img_h, img_w = _shapes
        else:
            img_h = img_w = 0

        d = self._rope_spatial_dim
        if target_resolution is not None:
            # User-specified uniform resolution: apply same scale to both axes
            s = target_resolution / self.training_resolution
            if s > 1.0:
                ntk = s ** (2 * d / (d - 2)) / (1 + math.log(s))
                ntk_factor_h = ntk_factor_w = ntk
            else:
                ntk_factor_h = ntk_factor_w = 1.0
        elif img_h > 0 and img_w > 0:
            # Per-axis scaling from actual image dimensions
            pixel_h = img_h * self.config.patch_size * 8
            pixel_w = img_w * self.config.patch_size * 8
            s_h = pixel_h / self.training_resolution
            s_w = pixel_w / self.training_resolution
            ntk_factor_w = s_w ** (2 * d / (d - 2)) / (1 + 0.1 * math.log(s_w)) if s_w > 1.0 else 1.0
            ntk_factor_h = s_h ** (2 * d / (d - 2)) / (1 + 0.1 * math.log(s_h)) if s_h > 1.0 else 1.0

            # If axes differ, scale the excess of the bigger-axis NTK factor by the axis ratio
            # and use it as the smaller-axis NTK factor:
            #   ntk_small = 1 + (small/big) * (ntk_big - 1)
            big = max(float(s_h), float(s_w))
            small = min(float(s_h), float(s_w))
            if big > 1.0 and small > 0.0 and abs(s_h - s_w) > 1e-8:
                ntk_big = max(float(ntk_factor_h), float(ntk_factor_w))
                ntk_small = 1.0 + (small / big) * max(0.0, (ntk_big - 1.0))
                if s_h < s_w:
                    ntk_factor_h = ntk_small
                    ntk_factor_w = ntk_big
                else:
                    ntk_factor_h = ntk_big
                    ntk_factor_w = ntk_small
        else:
            ntk_factor_h = ntk_factor_w = 1.0

        # ================================================================
        # Per-axis target resolutions for base_mscale computation
        # ================================================================
        if target_resolution is not None:
            _target_res_h = target_resolution
            _target_res_w = target_resolution
        elif img_h > 0 and img_w > 0:
            _target_res_h = img_h * self.config.patch_size * 8
            _target_res_w = img_w * self.config.patch_size * 8
        else:
            _target_res_h = _target_res_w = None

        # ================================================================
        # SEGA: FFT-Adaptive Spread with per-axis spectral profiles
        # ================================================================
        if max(ntk_factor_h, ntk_factor_w) > 1.0 and img_h > 0 and img_w > 0:
            # Per-axis spectral profiles for SEGA mscale
            n_bins_h = max(img_h // 2, 8)
            n_bins_w = max(img_w // 2, 8)
            energy_profile_h, energy_profile_w = compute_axis_spectral_profiles(
                hidden_states, img_h, img_w, n_bins_h, n_bins_w,
            )
            # Isotropic profile for spread schedule
            iso_profile = compute_spectral_energy_profile(
                hidden_states, img_h, img_w, n_bins=max(img_h, img_w) // 2,
            )
            dynamic_spread = compute_dynamic_spread(
                iso_profile, spread_min=self.spread_min,
                spread_max=self.spread_max, alpha=self.spread_alpha,
            )

        else:
            dynamic_spread = None
            energy_profile_h = energy_profile_w = None

        image_rotary_emb = self.pos_embed(
            img_shapes, max_txt_seq_len=text_seq_len, device=hidden_states.device,
            ntk_factor_h=ntk_factor_h, ntk_factor_w=ntk_factor_w,
            mscale_spread=dynamic_spread,
            energy_profile_h=energy_profile_h, energy_profile_w=energy_profile_w,
            target_res_h=_target_res_h, target_res_w=_target_res_w,
        )

        # Construct joint attention mask once to avoid reconstructing in every block
        # This eliminates 60 GPU syncs during training while maintaining torch.compile compatibility
        block_attention_kwargs = attention_kwargs.copy() if attention_kwargs is not None else {}
        if encoder_hidden_states_mask is not None:
            # Build joint mask: [text_mask, all_ones_for_image]
            batch_size, image_seq_len = hidden_states.shape[:2]
            image_mask = torch.ones((batch_size, image_seq_len), dtype=torch.bool, device=hidden_states.device)
            joint_attention_mask = torch.cat([encoder_hidden_states_mask, image_mask], dim=1)
            block_attention_kwargs["attention_mask"] = joint_attention_mask

        for index_block, block in enumerate(self.transformer_blocks):
            if torch.is_grad_enabled() and self.gradient_checkpointing:
                encoder_hidden_states, hidden_states = self._gradient_checkpointing_func(
                    block,
                    hidden_states,
                    encoder_hidden_states,
                    None,  # Don't pass encoder_hidden_states_mask (using attention_mask instead)
                    temb,
                    image_rotary_emb,
                    block_attention_kwargs,
                    modulate_index,
                )

            else:
                encoder_hidden_states, hidden_states = block(
                    hidden_states=hidden_states,
                    encoder_hidden_states=encoder_hidden_states,
                    encoder_hidden_states_mask=None,  # Don't pass (using attention_mask instead)
                    temb=temb,
                    image_rotary_emb=image_rotary_emb,
                    joint_attention_kwargs=block_attention_kwargs,
                    modulate_index=modulate_index,
                )

            # controlnet residual
            if controlnet_block_samples is not None:
                interval_control = len(self.transformer_blocks) / len(controlnet_block_samples)
                interval_control = int(np.ceil(interval_control))
                hidden_states = hidden_states + controlnet_block_samples[index_block // interval_control]

        if self.zero_cond_t:
            temb = temb.chunk(2, dim=0)[0]
        # Use only the image part (hidden_states) from the dual-stream blocks
        hidden_states = self.norm_out(hidden_states, temb)
        output = self.proj_out(hidden_states)

        if USE_PEFT_BACKEND:
            # remove `lora_scale` from each PEFT layer
            unscale_lora_layers(self, lora_scale)

        if not return_dict:
            return (output,)

        return Transformer2DModelOutput(sample=output)
