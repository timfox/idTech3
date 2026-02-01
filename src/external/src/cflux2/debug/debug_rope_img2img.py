#!/usr/bin/env python3
"""
Debug script to verify RoPE computation for img2img.
Compares C implementation with Python reference.
"""

import torch
import math

def rope(pos, dim, theta):
    """Compute RoPE embeddings - same as model.py"""
    scale = torch.arange(0, dim, 2, dtype=pos.dtype, device=pos.device) / dim
    omega = 1.0 / (theta**scale)
    out = torch.einsum("...n,d->...nd", pos, omega)
    out = torch.stack([torch.cos(out), -torch.sin(out), torch.sin(out), torch.cos(out)], dim=-1)
    out = out.reshape(*out.shape[:-1], 2, 2)
    return out.float()

def compute_rope_c_style(pos, dim, theta):
    """Compute RoPE in C-style: store [cos, cos, sin, sin] for each frequency pair"""
    half_dim = dim // 2
    cos_out = torch.zeros(half_dim * 2)
    sin_out = torch.zeros(half_dim * 2)

    for d in range(half_dim):
        scale = (2 * d) / dim
        omega = 1.0 / (theta ** scale)
        angle = float(pos) * omega
        cos_val = math.cos(angle)
        sin_val = math.sin(angle)
        # C stores each cos/sin repeated twice
        cos_out[d * 2] = cos_val
        cos_out[d * 2 + 1] = cos_val
        sin_out[d * 2] = sin_val
        sin_out[d * 2 + 1] = sin_val

    return cos_out, sin_out

def apply_rope_c_style(x, cos_vals, sin_vals):
    """Apply RoPE in C-style"""
    out = torch.zeros_like(x)
    for d in range(0, len(x), 2):
        cos_val = cos_vals[d]
        sin_val = sin_vals[d]
        x0 = x[d]
        x1 = x[d + 1]
        out[d] = x0 * cos_val - x1 * sin_val
        out[d + 1] = x1 * cos_val + x0 * sin_val
    return out

def apply_rope_python_style(x, freqs_cis):
    """Apply RoPE in Python-style"""
    # x: (dim,) - flattened head
    # freqs_cis: (n_freqs, 2, 2) - rotation matrices per frequency
    n_freqs = freqs_cis.shape[0]
    x_pairs = x.reshape(n_freqs, 2)  # (n_freqs, 2)

    out = torch.zeros_like(x_pairs)
    for i in range(n_freqs):
        # freqs_cis[i] is a 2x2 rotation matrix
        # [[cos, -sin], [sin, cos]]
        rot = freqs_cis[i]  # (2, 2)
        x_pair = x_pairs[i]  # (2,)
        # Python: out = rot[:, 0] * x[0] + rot[:, 1] * x[1]
        out[i] = rot[:, 0] * x_pair[0] + rot[:, 1] * x_pair[1]

    return out.flatten()

def main():
    theta = 2000  # Klein4B theta
    axis_dim = 32

    print("=" * 60)
    print("Testing RoPE for axis 0 (T dimension)")
    print("=" * 60)

    for t_pos in [0, 10]:
        print(f"\nT position = {t_pos}:")

        # C-style computation
        cos_c, sin_c = compute_rope_c_style(t_pos, axis_dim, theta)
        print(f"  C-style cos[:4] = {cos_c[:4].tolist()}")
        print(f"  C-style sin[:4] = {sin_c[:4].tolist()}")

        # Python-style computation
        pos = torch.tensor([t_pos], dtype=torch.float32)
        freqs = rope(pos, axis_dim, theta)  # (1, n_freqs, 2, 2)
        freqs = freqs[0]  # (n_freqs, 2, 2)

        # The freqs tensor stores [[cos, -sin], [sin, cos]] per frequency
        print(f"  Python freqs[0] (first freq rotation matrix):")
        print(f"    {freqs[0].tolist()}")

        # Test that both produce same rotation result
        print(f"\n  Testing rotation on random vector:")
        torch.manual_seed(42)
        x = torch.randn(axis_dim)
        print(f"    Input x[:4] = {x[:4].tolist()}")

        out_c = apply_rope_c_style(x, cos_c, sin_c)
        out_py = apply_rope_python_style(x, freqs)

        print(f"    C-style output[:4] = {out_c[:4].tolist()}")
        print(f"    Python output[:4] = {out_py[:4].tolist()}")

        diff = (out_c - out_py).abs().max().item()
        print(f"    Max diff: {diff}")

        if diff < 1e-5:
            print("    MATCH!")
        else:
            print("    MISMATCH!")

    print("\n" + "=" * 60)
    print("Testing full 128-dim RoPE (4 axes)")
    print("=" * 60)

    # For position (T=10, H=5, W=3, L=0)
    T, H, W, L = 10, 5, 3, 0
    print(f"\nPosition (T={T}, H={H}, W={W}, L={L}):")

    # Python: compute RoPE for each axis and concatenate
    ids = torch.tensor([[T, H, W, L]], dtype=torch.float32)  # (1, 4)
    axes_dim = [32, 32, 32, 32]

    all_freqs = []
    for i, dim in enumerate(axes_dim):
        axis_rope = rope(ids[:, i], dim, theta)  # (1, n_freqs, 2, 2)
        all_freqs.append(axis_rope)

    # Concatenate along frequency dimension
    full_freqs = torch.cat(all_freqs, dim=1)  # (1, 64, 2, 2)
    full_freqs = full_freqs[0]  # (64, 2, 2)

    print(f"  Full freqs shape: {full_freqs.shape}")
    print(f"  Axis 0 (T={T}), first freq rotation:")
    print(f"    {full_freqs[0].tolist()}")
    print(f"  Axis 1 (H={H}), first freq rotation:")
    print(f"    {full_freqs[16].tolist()}")

    # C-style for full 128-dim
    cos_full = torch.zeros(128)
    sin_full = torch.zeros(128)

    # Axis 0 (T)
    cos_t, sin_t = compute_rope_c_style(T, 32, theta)
    cos_full[:32] = cos_t
    sin_full[:32] = sin_t

    # Axis 1 (H)
    cos_h, sin_h = compute_rope_c_style(H, 32, theta)
    cos_full[32:64] = cos_h
    sin_full[32:64] = sin_h

    # Axis 2 (W)
    cos_w, sin_w = compute_rope_c_style(W, 32, theta)
    cos_full[64:96] = cos_w
    sin_full[64:96] = sin_w

    # Axis 3 (L)
    cos_l, sin_l = compute_rope_c_style(L, 32, theta)
    cos_full[96:128] = cos_l
    sin_full[96:128] = sin_l

    print(f"\n  C-style full cos[:4] (T axis) = {cos_full[:4].tolist()}")
    print(f"  C-style full cos[32:36] (H axis) = {cos_full[32:36].tolist()}")

    # Test full rotation
    print(f"\n  Testing full 128-dim rotation:")
    torch.manual_seed(123)
    x = torch.randn(128)

    # C-style: apply rotation dim by dim
    out_c = torch.zeros(128)
    for axis in range(4):
        axis_start = axis * 32
        x_axis = x[axis_start:axis_start+32]
        cos_axis = cos_full[axis_start:axis_start+32]
        sin_axis = sin_full[axis_start:axis_start+32]
        out_c[axis_start:axis_start+32] = apply_rope_c_style(x_axis, cos_axis, sin_axis)

    # Python-style: apply using 64 rotation matrices
    out_py = apply_rope_python_style(x, full_freqs)

    print(f"    C output[:4] = {out_c[:4].tolist()}")
    print(f"    Py output[:4] = {out_py[:4].tolist()}")

    diff = (out_c - out_py).abs().max().item()
    print(f"    Max diff: {diff}")

    if diff < 1e-5:
        print("    MATCH!")
    else:
        print("    MISMATCH!")

if __name__ == "__main__":
    main()
