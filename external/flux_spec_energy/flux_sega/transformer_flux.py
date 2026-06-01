# Copyright 2025 Black Forest Labs, The HuggingFace Team and The InstantX Team. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
#
# Transformer adapted from Hugging Face diffusers (FluxTransformer2DModel) with SEGA.
# ---------------------------------------------------------------------------
# SEGA: Spectral-Energy Guided Attention for resolution extrapolation.
# ---------------------------------------------------------------------------

import inspect
from typing import Any, Dict, List, Optional, Tuple, Union

import math

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from diffusers.configuration_utils import ConfigMixin, register_to_config
from diffusers.loaders import FluxTransformer2DLoadersMixin, FromOriginalModelMixin, PeftAdapterMixin
from diffusers.utils import USE_PEFT_BACKEND, logging, scale_lora_layers, unscale_lora_layers
from diffusers.utils.torch_utils import maybe_allow_in_graph
from diffusers.models.attention import AttentionMixin, AttentionModuleMixin, FeedForward
from diffusers.models.attention_dispatch import dispatch_attention_fn
from diffusers.models.cache_utils import CacheMixin
from diffusers.models.embeddings import (
    CombinedTimestepGuidanceTextProjEmbeddings,
    CombinedTimestepTextProjEmbeddings,
)
from diffusers.models.modeling_outputs import Transformer2DModelOutput
from diffusers.models.modeling_utils import ModelMixin
from diffusers.models.normalization import AdaLayerNormContinuous, AdaLayerNormZero, AdaLayerNormZeroSingle


logger = logging.get_logger(__name__)


# ---------------------------------------------------------------------------
# RoPE helpers
# ---------------------------------------------------------------------------

def apply_rotary_emb(
    x: torch.Tensor,
    freqs_cis: Union[torch.Tensor, Tuple[torch.Tensor]],
    use_real: bool = True,
    use_real_unbind_dim: int = -1,
    sequence_dim: int = 2,
) -> torch.Tensor:
    if use_real:
        cos, sin = freqs_cis
        if sequence_dim == 2:
            cos = cos[None, None, :, :]
            sin = sin[None, None, :, :]
        elif sequence_dim == 1:
            cos = cos[None, :, None, :]
            sin = sin[None, :, None, :]
        else:
            raise ValueError(f"`sequence_dim={sequence_dim}` but should be 1 or 2.")
        cos, sin = cos.to(x.device), sin.to(x.device)
        if use_real_unbind_dim == -1:
            x_real, x_imag = x.reshape(*x.shape[:-1], -1, 2).unbind(-1)
            x_rotated = torch.stack([-x_imag, x_real], dim=-1).flatten(3)
        elif use_real_unbind_dim == -2:
            x_real, x_imag = x.reshape(*x.shape[:-1], 2, -1).unbind(-2)
            x_rotated = torch.cat([-x_imag, x_real], dim=-1)
        else:
            raise ValueError(f"`use_real_unbind_dim={use_real_unbind_dim}` but should be -1 or -2.")
        return (x.float() * cos + x_rotated.float() * sin).to(x.dtype)
    else:
        x_rotated = torch.view_as_complex(x.float().reshape(*x.shape[:-1], -1, 2))
        freqs_cis = freqs_cis.unsqueeze(2)
        x_out = torch.view_as_real(x_rotated * freqs_cis).flatten(3)
        return x_out.type_as(x)


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
# 1-D RoPE embedding with SEGA per-dim mscale
# ---------------------------------------------------------------------------

def get_1d_rotary_pos_embed(
    dim: int,
    pos: Union[np.ndarray, int],
    theta: float = 10000.0,
    use_real: bool = True,
    ntk_factor: float = 1.0,
    repeat_interleave_real: bool = True,
    freqs_dtype=torch.float32,
    energy_profile: Optional[torch.Tensor] = None,
    mscale_spread: Optional[float] = None,
    mscale_alpha: float = 0.15,
    mscale_beta: float = 1.5,
    mscale_min: float = 1.0,
    base_mscale_formula: str = "power_res",
    base_mscale_coefficient: Optional[float] = None,
    target_res: Optional[int] = None,
    training_res: Optional[int] = None,
):
    assert dim % 2 == 0

    if isinstance(pos, int):
        pos = torch.arange(pos)
    if isinstance(pos, np.ndarray):
        pos = torch.from_numpy(pos)

    dim_indices = torch.arange(0, dim, 2, dtype=freqs_dtype, device=pos.device)
    inv_freq = 1.0 / (theta ** (dim_indices / dim))

    effective_mscale: Union[float, torch.Tensor] = 1.0

    if ntk_factor > 1.0:
        scaled_theta = theta * ntk_factor
        freqs = 1.0 / (scaled_theta ** (dim_indices / dim))

        base_ms = compute_base_mscale(
            target_res=target_res,
            training_res=training_res,
            formula=base_mscale_formula,
            coefficient=base_mscale_coefficient,
        )

        use_sega = (
            energy_profile is not None
            and mscale_spread is not None
            and float(mscale_spread) > 0.0
            and base_ms > 1.0 + 1e-8
        )

        if use_sega:
            effective_mscale = compute_sega_allocation(
                energy_profile=energy_profile,
                freqs=freqs,
                base_mscale=base_ms,
                spread=float(mscale_spread),
                alpha=float(mscale_alpha),
                beta=float(mscale_beta),
                min_mscale=float(mscale_min),
            )
        else:
            effective_mscale = base_ms
    else:
        freqs = inv_freq

    freqs = torch.outer(pos.float(), freqs)

    if freqs.device.type == "npu":
        freqs = freqs.float()

    if isinstance(effective_mscale, torch.Tensor):
        if use_real and repeat_interleave_real:
            ms = effective_mscale.repeat_interleave(2)
            freqs_cos = freqs.cos().repeat_interleave(2, dim=1, output_size=freqs.shape[1] * 2).float() * ms
            freqs_sin = freqs.sin().repeat_interleave(2, dim=1, output_size=freqs.shape[1] * 2).float() * ms
        elif use_real:
            ms = torch.cat([effective_mscale, effective_mscale], dim=-1)
            freqs_cos = torch.cat([freqs.cos(), freqs.cos()], dim=-1).float() * ms
            freqs_sin = torch.cat([freqs.sin(), freqs.sin()], dim=-1).float() * ms
        else:
            freqs_cis = torch.polar(effective_mscale.unsqueeze(0).expand_as(freqs), freqs)
            return freqs_cis
    else:
        if use_real and repeat_interleave_real:
            freqs_cos = freqs.cos().repeat_interleave(2, dim=1, output_size=freqs.shape[1] * 2).float() * effective_mscale
            freqs_sin = freqs.sin().repeat_interleave(2, dim=1, output_size=freqs.shape[1] * 2).float() * effective_mscale
        elif use_real:
            freqs_cos = torch.cat([freqs.cos(), freqs.cos()], dim=-1).float() * effective_mscale
            freqs_sin = torch.cat([freqs.sin(), freqs.sin()], dim=-1).float() * effective_mscale
        else:
            freqs_cis = torch.polar(torch.ones_like(freqs) * effective_mscale, freqs)
            return freqs_cis

    return freqs_cos, freqs_sin


# ---------------------------------------------------------------------------
# Spectral helpers
# ---------------------------------------------------------------------------

@torch.no_grad()
def compute_spectral_energy_profile(hidden_states, height, width, n_bins):
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
    eps = 1e-8
    energy = energy_profile.clamp(min=eps)
    geo_mean = torch.exp(torch.log(energy).mean())
    arith_mean = energy.mean()
    flatness = (geo_mean / (arith_mean + eps)).clamp(0.0, 1.0)
    concentration = 1.0 - flatness.item()
    return spread_min + (spread_max - spread_min) * (1.0 - (1.0 - concentration) ** alpha)


# ---------------------------------------------------------------------------
# Attention
# ---------------------------------------------------------------------------

def _get_projections(attn, hidden_states, encoder_hidden_states=None):
    query = attn.to_q(hidden_states)
    key = attn.to_k(hidden_states)
    value = attn.to_v(hidden_states)
    encoder_query = encoder_key = encoder_value = None
    if encoder_hidden_states is not None and attn.added_kv_proj_dim is not None:
        encoder_query = attn.add_q_proj(encoder_hidden_states)
        encoder_key = attn.add_k_proj(encoder_hidden_states)
        encoder_value = attn.add_v_proj(encoder_hidden_states)
    return query, key, value, encoder_query, encoder_key, encoder_value


def _get_fused_projections(attn, hidden_states, encoder_hidden_states=None):
    query, key, value = attn.to_qkv(hidden_states).chunk(3, dim=-1)
    encoder_query = encoder_key = encoder_value = None
    if encoder_hidden_states is not None and hasattr(attn, "to_added_qkv"):
        encoder_query, encoder_key, encoder_value = attn.to_added_qkv(encoder_hidden_states).chunk(3, dim=-1)
    return query, key, value, encoder_query, encoder_key, encoder_value


def _get_qkv_projections(attn, hidden_states, encoder_hidden_states=None):
    if attn.fused_projections:
        return _get_fused_projections(attn, hidden_states, encoder_hidden_states)
    return _get_projections(attn, hidden_states, encoder_hidden_states)


class FluxAttnProcessor:
    _attention_backend = None

    def __init__(self):
        pass

    def __call__(self, attn, hidden_states, encoder_hidden_states=None, attention_mask=None, image_rotary_emb=None):
        query, key, value, encoder_query, encoder_key, encoder_value = _get_qkv_projections(
            attn, hidden_states, encoder_hidden_states
        )
        query = query.unflatten(-1, (attn.heads, -1))
        key = key.unflatten(-1, (attn.heads, -1))
        value = value.unflatten(-1, (attn.heads, -1))
        query = attn.norm_q(query)
        key = attn.norm_k(key)

        if attn.added_kv_proj_dim is not None:
            encoder_query = encoder_query.unflatten(-1, (attn.heads, -1))
            encoder_key = encoder_key.unflatten(-1, (attn.heads, -1))
            encoder_value = encoder_value.unflatten(-1, (attn.heads, -1))
            encoder_query = attn.norm_added_q(encoder_query)
            encoder_key = attn.norm_added_k(encoder_key)
            query = torch.cat([encoder_query, query], dim=1)
            key = torch.cat([encoder_key, key], dim=1)
            value = torch.cat([encoder_value, value], dim=1)

        if image_rotary_emb is not None:
            query = apply_rotary_emb(query, image_rotary_emb, sequence_dim=1)
            key = apply_rotary_emb(key, image_rotary_emb, sequence_dim=1)

        hidden_states = dispatch_attention_fn(
            query, key, value, attn_mask=attention_mask, backend=self._attention_backend)
        hidden_states = hidden_states.flatten(2, 3).to(query.dtype)

        if encoder_hidden_states is not None:
            encoder_hidden_states, hidden_states = hidden_states.split_with_sizes(
                [encoder_hidden_states.shape[1], hidden_states.shape[1] - encoder_hidden_states.shape[1]], dim=1
            )
            hidden_states = attn.to_out[0](hidden_states)
            hidden_states = attn.to_out[1](hidden_states)
            encoder_hidden_states = attn.to_add_out(encoder_hidden_states)
            return hidden_states, encoder_hidden_states
        return hidden_states


class FluxAttention(torch.nn.Module, AttentionModuleMixin):
    _default_processor_cls = FluxAttnProcessor
    _available_processors = [FluxAttnProcessor]

    def __init__(
        self, query_dim, heads=8, dim_head=64, dropout=0.0, bias=False,
        added_kv_proj_dim=None, added_proj_bias=True, out_bias=True,
        eps=1e-5, out_dim=None, context_pre_only=None, pre_only=False,
        elementwise_affine=True, processor=None,
    ):
        super().__init__()
        self.head_dim = dim_head
        self.inner_dim = out_dim if out_dim is not None else dim_head * heads
        self.query_dim = query_dim
        self.use_bias = bias
        self.dropout = dropout
        self.out_dim = out_dim if out_dim is not None else query_dim
        self.context_pre_only = context_pre_only
        self.pre_only = pre_only
        self.heads = out_dim // dim_head if out_dim is not None else heads
        self.added_kv_proj_dim = added_kv_proj_dim
        self.added_proj_bias = added_proj_bias

        self.norm_q = torch.nn.RMSNorm(dim_head, eps=eps, elementwise_affine=elementwise_affine)
        self.norm_k = torch.nn.RMSNorm(dim_head, eps=eps, elementwise_affine=elementwise_affine)
        self.to_q = torch.nn.Linear(query_dim, self.inner_dim, bias=bias)
        self.to_k = torch.nn.Linear(query_dim, self.inner_dim, bias=bias)
        self.to_v = torch.nn.Linear(query_dim, self.inner_dim, bias=bias)

        if not self.pre_only:
            self.to_out = torch.nn.ModuleList([
                torch.nn.Linear(self.inner_dim, self.out_dim, bias=out_bias),
                torch.nn.Dropout(dropout),
            ])

        if added_kv_proj_dim is not None:
            self.norm_added_q = torch.nn.RMSNorm(dim_head, eps=eps)
            self.norm_added_k = torch.nn.RMSNorm(dim_head, eps=eps)
            self.add_q_proj = torch.nn.Linear(added_kv_proj_dim, self.inner_dim, bias=added_proj_bias)
            self.add_k_proj = torch.nn.Linear(added_kv_proj_dim, self.inner_dim, bias=added_proj_bias)
            self.add_v_proj = torch.nn.Linear(added_kv_proj_dim, self.inner_dim, bias=added_proj_bias)
            self.to_add_out = torch.nn.Linear(self.inner_dim, query_dim, bias=out_bias)

        if processor is None:
            processor = self._default_processor_cls()
        self.set_processor(processor)

    def forward(self, hidden_states, encoder_hidden_states=None, attention_mask=None, image_rotary_emb=None, **kwargs):
        attn_parameters = set(inspect.signature(self.processor.__call__).parameters.keys())
        kwargs = {k: w for k, w in kwargs.items() if k in attn_parameters}
        return self.processor(self, hidden_states, encoder_hidden_states, attention_mask, image_rotary_emb, **kwargs)


# ---------------------------------------------------------------------------
# Transformer blocks
# ---------------------------------------------------------------------------

@maybe_allow_in_graph
class FluxSingleTransformerBlock(nn.Module):
    def __init__(self, dim, num_attention_heads, attention_head_dim, mlp_ratio=4.0):
        super().__init__()
        self.mlp_hidden_dim = int(dim * mlp_ratio)
        self.norm = AdaLayerNormZeroSingle(dim)
        self.proj_mlp = nn.Linear(dim, self.mlp_hidden_dim)
        self.act_mlp = nn.GELU(approximate="tanh")
        self.proj_out = nn.Linear(dim + self.mlp_hidden_dim, dim)
        self.attn = FluxAttention(
            query_dim=dim, dim_head=attention_head_dim, heads=num_attention_heads,
            out_dim=dim, bias=True, processor=FluxAttnProcessor(), eps=1e-6, pre_only=True,
        )

    def forward(self, hidden_states, encoder_hidden_states, temb, image_rotary_emb=None, joint_attention_kwargs=None):
        text_seq_len = encoder_hidden_states.shape[1]
        hidden_states = torch.cat([encoder_hidden_states, hidden_states], dim=1)
        residual = hidden_states
        norm_hidden_states, gate = self.norm(hidden_states, emb=temb)
        mlp_hidden_states = self.act_mlp(self.proj_mlp(norm_hidden_states))
        attn_output = self.attn(hidden_states=norm_hidden_states, image_rotary_emb=image_rotary_emb, **(joint_attention_kwargs or {}))
        hidden_states = gate.unsqueeze(1) * self.proj_out(torch.cat([attn_output, mlp_hidden_states], dim=2))
        hidden_states = residual + hidden_states
        if hidden_states.dtype == torch.float16:
            hidden_states = hidden_states.clip(-65504, 65504)
        encoder_hidden_states, hidden_states = hidden_states[:, :text_seq_len], hidden_states[:, text_seq_len:]
        return encoder_hidden_states, hidden_states


@maybe_allow_in_graph
class FluxTransformerBlock(nn.Module):
    def __init__(self, dim, num_attention_heads, attention_head_dim, qk_norm="rms_norm", eps=1e-6):
        super().__init__()
        self.norm1 = AdaLayerNormZero(dim)
        self.norm1_context = AdaLayerNormZero(dim)
        self.attn = FluxAttention(
            query_dim=dim, added_kv_proj_dim=dim, dim_head=attention_head_dim,
            heads=num_attention_heads, out_dim=dim, context_pre_only=False,
            bias=True, processor=FluxAttnProcessor(), eps=eps,
        )
        self.norm2 = nn.LayerNorm(dim, elementwise_affine=False, eps=1e-6)
        self.ff = FeedForward(dim=dim, dim_out=dim, activation_fn="gelu-approximate")
        self.norm2_context = nn.LayerNorm(dim, elementwise_affine=False, eps=1e-6)
        self.ff_context = FeedForward(dim=dim, dim_out=dim, activation_fn="gelu-approximate")

    def forward(self, hidden_states, encoder_hidden_states, temb, image_rotary_emb=None, joint_attention_kwargs=None, attention_mask=None):
        norm_hidden_states, gate_msa, shift_mlp, scale_mlp, gate_mlp = self.norm1(hidden_states, emb=temb)
        norm_encoder_hidden_states, c_gate_msa, c_shift_mlp, c_scale_mlp, c_gate_mlp = self.norm1_context(encoder_hidden_states, emb=temb)

        attention_outputs = self.attn(
            hidden_states=norm_hidden_states, encoder_hidden_states=norm_encoder_hidden_states,
            image_rotary_emb=image_rotary_emb, **(joint_attention_kwargs or {}),
        )

        if len(attention_outputs) == 2:
            attn_output, context_attn_output = attention_outputs
        elif len(attention_outputs) == 3:
            attn_output, context_attn_output, ip_attn_output = attention_outputs

        hidden_states = hidden_states + gate_msa.unsqueeze(1) * attn_output
        norm_hidden_states = self.norm2(hidden_states)
        norm_hidden_states = norm_hidden_states * (1 + scale_mlp[:, None]) + shift_mlp[:, None]
        hidden_states = hidden_states + gate_mlp.unsqueeze(1) * self.ff(norm_hidden_states)
        if len(attention_outputs) == 3:
            hidden_states = hidden_states + ip_attn_output

        encoder_hidden_states = encoder_hidden_states + c_gate_msa.unsqueeze(1) * context_attn_output
        norm_encoder_hidden_states = self.norm2_context(encoder_hidden_states)
        norm_encoder_hidden_states = norm_encoder_hidden_states * (1 + c_scale_mlp[:, None]) + c_shift_mlp[:, None]
        encoder_hidden_states = encoder_hidden_states + c_gate_mlp.unsqueeze(1) * self.ff_context(norm_encoder_hidden_states)
        if encoder_hidden_states.dtype == torch.float16:
            encoder_hidden_states = encoder_hidden_states.clip(-65504, 65504)
        return encoder_hidden_states, hidden_states


# ---------------------------------------------------------------------------
# Position embedding
# ---------------------------------------------------------------------------

class FluxPosEmbed(nn.Module):
    def __init__(self, theta, axes_dim, training_resolution=64,
                 mscale_alpha=0.15, mscale_beta=1.5, mscale_min=1.0,
                 base_mscale_formula="power_res", base_mscale_coefficient=None,
                 training_res_pixels=None):
        super().__init__()
        self.theta = theta
        self.axes_dim = axes_dim
        self.training_resolution = training_resolution
        self.mscale_alpha = mscale_alpha
        self.mscale_beta = mscale_beta
        self.mscale_min = mscale_min
        self.base_mscale_formula = base_mscale_formula
        self.base_mscale_coefficient = base_mscale_coefficient
        self.training_res_pixels = training_res_pixels

    def forward(self, ids, ntk_factor=1.0, mscale_spread=None,
                energy_profile_h=None, energy_profile_w=None, target_res=None):
        n_axes = ids.shape[-1]
        cos_out, sin_out = [], []
        pos = ids.float()
        is_mps = ids.device.type == "mps"
        is_npu = ids.device.type == "npu"
        freqs_dtype = torch.float32 if (is_mps or is_npu) else torch.float64

        axis_energy = {1: energy_profile_h, 2: energy_profile_w}

        for i in range(n_axes):
            if i > 0 and ntk_factor > 1.0:
                cos, sin = get_1d_rotary_pos_embed(
                    self.axes_dim[i], pos[:, i], theta=self.theta,
                    repeat_interleave_real=True, use_real=True, freqs_dtype=freqs_dtype,
                    ntk_factor=ntk_factor,
                    energy_profile=axis_energy.get(i),
                    mscale_spread=mscale_spread,
                    mscale_alpha=self.mscale_alpha, mscale_beta=self.mscale_beta,
                    mscale_min=self.mscale_min,
                    base_mscale_formula=self.base_mscale_formula,
                    base_mscale_coefficient=self.base_mscale_coefficient,
                    target_res=target_res, training_res=self.training_res_pixels,
                )
            else:
                cos, sin = get_1d_rotary_pos_embed(
                    self.axes_dim[i], pos[:, i], theta=self.theta,
                    repeat_interleave_real=True, use_real=True, freqs_dtype=freqs_dtype,
                )
            cos_out.append(cos)
            sin_out.append(sin)

        return torch.cat(cos_out, dim=-1).to(ids.device), torch.cat(sin_out, dim=-1).to(ids.device)


# ---------------------------------------------------------------------------
# Transformer
# ---------------------------------------------------------------------------

class FluxTransformer2DModel(
    ModelMixin, ConfigMixin, PeftAdapterMixin, FromOriginalModelMixin,
    FluxTransformer2DLoadersMixin, CacheMixin, AttentionMixin,
):
    _supports_gradient_checkpointing = True
    _no_split_modules = ["FluxTransformerBlock", "FluxSingleTransformerBlock"]
    _skip_layerwise_casting_patterns = ["pos_embed", "norm"]
    _repeated_blocks = ["FluxTransformerBlock", "FluxSingleTransformerBlock"]

    @register_to_config
    def __init__(
        self,
        patch_size: int = 1,
        in_channels: int = 64,
        out_channels: Optional[int] = None,
        num_layers: int = 19,
        num_single_layers: int = 38,
        attention_head_dim: int = 128,
        num_attention_heads: int = 24,
        joint_attention_dim: int = 4096,
        pooled_projection_dim: int = 768,
        guidance_embeds: bool = False,
        axes_dims_rope: Tuple[int, int, int] = (16, 56, 56),
        training_resolution: int = 1024,
        spread_max: float = 1.0,
        spread_min: float = 0.0,
        spread_alpha: float = 1.5,
        mscale_alpha: float = 0.15,
        mscale_beta: float = 1.5,
        mscale_min: float = 1.0,
        base_mscale_formula: str = "power_res",
        base_mscale_coefficient: float = 0.08,
    ):
        super().__init__()
        self.out_channels = out_channels or in_channels
        self.inner_dim = num_attention_heads * attention_head_dim

        training_resolution_tokens = training_resolution / 16

        self.spread_min = spread_min
        self.spread_max = spread_max
        self.spread_alpha = spread_alpha
        self.training_resolution = training_resolution
        self._rope_spatial_dim = axes_dims_rope[1]

        self.pos_embed = FluxPosEmbed(
            theta=10000, axes_dim=axes_dims_rope,
            training_resolution=training_resolution_tokens,
            mscale_alpha=mscale_alpha, mscale_beta=mscale_beta, mscale_min=mscale_min,
            base_mscale_formula=base_mscale_formula,
            base_mscale_coefficient=base_mscale_coefficient,
            training_res_pixels=training_resolution,
        )

        text_time_guidance_cls = (
            CombinedTimestepGuidanceTextProjEmbeddings if guidance_embeds else CombinedTimestepTextProjEmbeddings
        )
        self.time_text_embed = text_time_guidance_cls(
            embedding_dim=self.inner_dim, pooled_projection_dim=pooled_projection_dim
        )
        self.context_embedder = nn.Linear(joint_attention_dim, self.inner_dim)
        self.x_embedder = nn.Linear(in_channels, self.inner_dim)

        self.transformer_blocks = nn.ModuleList([
            FluxTransformerBlock(dim=self.inner_dim, num_attention_heads=num_attention_heads, attention_head_dim=attention_head_dim)
            for _ in range(num_layers)
        ])
        self.single_transformer_blocks = nn.ModuleList([
            FluxSingleTransformerBlock(dim=self.inner_dim, num_attention_heads=num_attention_heads, attention_head_dim=attention_head_dim)
            for _ in range(num_single_layers)
        ])

        self.norm_out = AdaLayerNormContinuous(self.inner_dim, self.inner_dim, elementwise_affine=False, eps=1e-6)
        self.proj_out = nn.Linear(self.inner_dim, patch_size * patch_size * self.out_channels, bias=True)
        self.gradient_checkpointing = False

    def forward(
        self,
        hidden_states: torch.Tensor,
        encoder_hidden_states: torch.Tensor = None,
        pooled_projections: torch.Tensor = None,
        timestep: torch.LongTensor = None,
        img_ids: torch.Tensor = None,
        txt_ids: torch.Tensor = None,
        guidance: torch.Tensor = None,
        joint_attention_kwargs: Optional[Dict[str, Any]] = None,
        controlnet_block_samples=None,
        controlnet_single_block_samples=None,
        return_dict: bool = True,
        controlnet_blocks_repeat: bool = False,
        target_resolution: Optional[int] = None,
    ) -> Union[torch.Tensor, Transformer2DModelOutput]:
        if joint_attention_kwargs is not None:
            joint_attention_kwargs = joint_attention_kwargs.copy()
            lora_scale = joint_attention_kwargs.pop("scale", 1.0)
        else:
            lora_scale = 1.0

        if USE_PEFT_BACKEND:
            scale_lora_layers(self, lora_scale)
        hidden_states = self.x_embedder(hidden_states)

        timestep = timestep.to(hidden_states.dtype) * 1000
        if guidance is not None:
            guidance = guidance.to(hidden_states.dtype) * 1000

        temb = (
            self.time_text_embed(timestep, pooled_projections) if guidance is None
            else self.time_text_embed(timestep, guidance, pooled_projections)
        )
        encoder_hidden_states = self.context_embedder(encoder_hidden_states)

        if txt_ids.ndim == 3:
            txt_ids = txt_ids[0]
        if img_ids.ndim == 3:
            img_ids = img_ids[0]

        # Dynamic NTK factor
        max_h = int(img_ids[:, 1].max().item() - img_ids[:, 1].min().item()) + 1
        max_w = int(img_ids[:, 2].max().item() - img_ids[:, 2].min().item()) + 1
        if target_resolution is None:
            target_resolution = max(max_h, max_w) * 16
        s = target_resolution / self.training_resolution
        d = self._rope_spatial_dim
        current_ntk_factor = s ** (2 * d / (d - 2)) / (1 + 0.1 * math.log(s))

        dynamic_spread = None
        energy_profile_h = None
        energy_profile_w = None

        if current_ntk_factor > 1.0:
            n_spatial = hidden_states.shape[1]
            if max_h * max_w == n_spatial:
                img_h, img_w = max_h, max_w
            else:
                aspect = max_h / max_w
                img_w = int(math.sqrt(n_spatial / aspect))
                img_h = n_spatial // max(1, img_w)

            n_bins_h = max(img_h // 2, 8)
            n_bins_w = max(img_w // 2, 8)
            energy_profile_h, energy_profile_w = compute_axis_spectral_profiles(
                hidden_states, img_h, img_w, n_bins_h, n_bins_w,
            )
            iso_profile = compute_spectral_energy_profile(
                hidden_states, img_h, img_w, n_bins=max(img_h, img_w) // 2,
            )
            dynamic_spread = compute_dynamic_spread(
                iso_profile, spread_min=self.spread_min,
                spread_max=self.spread_max, alpha=self.spread_alpha,
            )

        ids = torch.cat((txt_ids, img_ids), dim=0)

        image_rotary_emb = self.pos_embed(
            ids, ntk_factor=current_ntk_factor, mscale_spread=dynamic_spread,
            energy_profile_h=energy_profile_h, energy_profile_w=energy_profile_w,
            target_res=target_resolution,
        )

        if joint_attention_kwargs is not None and "ip_adapter_image_embeds" in joint_attention_kwargs:
            ip_adapter_image_embeds = joint_attention_kwargs.pop("ip_adapter_image_embeds")
            ip_hidden_states = self.encoder_hid_proj(ip_adapter_image_embeds)
            joint_attention_kwargs.update({"ip_hidden_states": ip_hidden_states})

        for index_block, block in enumerate(self.transformer_blocks):
            if torch.is_grad_enabled() and self.gradient_checkpointing:
                encoder_hidden_states, hidden_states = self._gradient_checkpointing_func(
                    block, hidden_states, encoder_hidden_states, temb, image_rotary_emb, joint_attention_kwargs,
                )
            else:
                encoder_hidden_states, hidden_states = block(
                    hidden_states=hidden_states, encoder_hidden_states=encoder_hidden_states,
                    temb=temb, image_rotary_emb=image_rotary_emb, joint_attention_kwargs=joint_attention_kwargs,
                )
            if controlnet_block_samples is not None:
                interval_control = int(np.ceil(len(self.transformer_blocks) / len(controlnet_block_samples)))
                if controlnet_blocks_repeat:
                    hidden_states = hidden_states + controlnet_block_samples[index_block % len(controlnet_block_samples)]
                else:
                    hidden_states = hidden_states + controlnet_block_samples[index_block // interval_control]

        for index_block, block in enumerate(self.single_transformer_blocks):
            if torch.is_grad_enabled() and self.gradient_checkpointing:
                encoder_hidden_states, hidden_states = self._gradient_checkpointing_func(
                    block, hidden_states, encoder_hidden_states, temb, image_rotary_emb, joint_attention_kwargs,
                )
            else:
                encoder_hidden_states, hidden_states = block(
                    hidden_states=hidden_states, encoder_hidden_states=encoder_hidden_states,
                    temb=temb, image_rotary_emb=image_rotary_emb, joint_attention_kwargs=joint_attention_kwargs,
                )
            if controlnet_single_block_samples is not None:
                interval_control = int(np.ceil(len(self.single_transformer_blocks) / len(controlnet_single_block_samples)))
                hidden_states = hidden_states + controlnet_single_block_samples[index_block // interval_control]

        hidden_states = self.norm_out(hidden_states, temb)
        output = self.proj_out(hidden_states)

        if USE_PEFT_BACKEND:
            unscale_lora_layers(self, lora_scale)

        if not return_dict:
            return (output,)
        return Transformer2DModelOutput(sample=output)
