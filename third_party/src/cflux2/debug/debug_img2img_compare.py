#!/usr/bin/env python3
"""
Compare C and Python img2img outputs using identical inputs.

This script:
1. Encodes a reference image using diffusers VAE + batch normalization
2. Encodes a text prompt using Qwen3
3. Runs the FLUX.2 transformer for denoising
4. Saves intermediate values to /tmp/ for C comparison

Usage:
    python debug/debug_img2img_compare.py

Prerequisites:
    pip install torch diffusers transformers safetensors einops huggingface_hub

    # Clone flux2 for the model class (into project root):
    git clone https://github.com/black-forest-labs/flux flux2

    # Place a test image at /tmp/woman.png

After running, use C with --debug-py to compare:
    ./flux -d flux-klein-model --debug-py -W 256 -H 256 --steps 4 -o /tmp/c_debug.png
"""

import os
import sys

# Add flux2/src to path for model imports
script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(script_dir)
flux2_path = os.path.join(project_root, "flux2", "src")
if os.path.exists(flux2_path):
    sys.path.insert(0, flux2_path)
else:
    print(f"Warning: flux2 not found at {flux2_path}")
    print("Clone it with: git clone https://github.com/black-forest-labs/flux flux2")

import torch
import numpy as np
from PIL import Image
from einops import rearrange
from safetensors.torch import load_file as load_sft

from flux2.model import Flux2, Klein4BParams
from flux2.sampling import prc_img, prc_txt, get_schedule


def apply_batch_norm(z, running_mean, running_var, eps=1e-4):
    """Apply batch normalization to latent."""
    mean = running_mean.view(1, -1, 1, 1)
    var = running_var.view(1, -1, 1, 1)
    return (z - mean) / torch.sqrt(var + eps)


def inv_batch_norm(z, running_mean, running_var, eps=1e-4):
    """Inverse batch normalization."""
    mean = running_mean.view(1, -1, 1, 1)
    std = torch.sqrt(running_var.view(1, -1, 1, 1) + eps)
    return z * std + mean


def main():
    # Configuration
    model_dir = os.path.join(project_root, "flux-klein-model")
    input_image = "/tmp/woman.png"
    width, height = 256, 256
    num_steps = 4
    seed = 1769049725
    prompt = "Make it color image."

    # Auto-detect device
    if torch.backends.mps.is_available():
        device = torch.device("mps")
    elif torch.cuda.is_available():
        device = torch.device("cuda")
    else:
        device = torch.device("cpu")

    print(f"Device: {device}")
    print(f"Settings: {width}x{height}, {num_steps} steps, seed={seed}")
    print(f"Prompt: '{prompt}'")

    # Load diffusers VAE
    print("\nLoading VAE...")
    from diffusers import AutoencoderKL
    vae = AutoencoderKL.from_pretrained(
        os.path.join(model_dir, "vae"),
        torch_dtype=torch.bfloat16
    ).to(device)

    # Load batch norm parameters
    vae_weights = load_sft(
        os.path.join(model_dir, "vae", "diffusion_pytorch_model.safetensors"),
        device=str(device)
    )
    bn_mean = vae_weights["bn.running_mean"]
    bn_var = vae_weights["bn.running_var"]
    print(f"Batch norm: mean.mean={bn_mean.mean():.4f}, var.mean={bn_var.mean():.4f}")

    # Load transformer
    print("Loading transformer...")
    import huggingface_hub
    model_path = huggingface_hub.hf_hub_download(
        repo_id="black-forest-labs/FLUX.2-klein-4B",
        filename="flux-2-klein-4b.safetensors"
    )
    with torch.device("meta"):
        model = Flux2(Klein4BParams()).to(torch.bfloat16)
    sd = load_sft(model_path, device=str(device))
    model.load_state_dict(sd, strict=True, assign=True)
    model = model.to(device)
    model.eval()

    # Load and encode reference image
    print("\nEncoding reference image...")
    if not os.path.exists(input_image):
        print(f"Error: Input image not found: {input_image}")
        print("Please place a test image at /tmp/woman.png")
        return

    input_img = Image.open(input_image).convert("RGB")
    input_img = input_img.resize((width, height))

    img_tensor = torch.from_numpy(np.array(input_img)).permute(2, 0, 1).float() / 255.0
    img_tensor = img_tensor * 2 - 1  # [-1, 1]
    img_tensor = img_tensor.unsqueeze(0).to(device).to(torch.bfloat16)

    # Encode with diffusers VAE
    with torch.no_grad():
        latent = vae.encode(img_tensor).latent_dist.sample()

    # Patchify: (B, 32, H/8, W/8) -> (B, 128, H/16, W/16)
    latent = rearrange(latent, "b c (h p1) (w p2) -> b (c p1 p2) h w", p1=2, p2=2)

    # Debug: print pre-batchnorm stats
    pre_flat = latent.float().cpu().numpy().flatten()
    print(f"[VAE DEBUG] Pre-batchnorm: mean={pre_flat.mean():.6f}, std={pre_flat.std():.6f}")

    # Apply batch normalization
    ref_latent = apply_batch_norm(latent, bn_mean, bn_var)

    post_flat = ref_latent.float().cpu().numpy().flatten()
    print(f"[VAE DEBUG] Post-batchnorm: mean={post_flat.mean():.6f}, std={post_flat.std():.6f}")
    print(f"Reference latent: first 8: {post_flat[:8]}")

    # Save for C comparison
    ref_latent.float().cpu().numpy().astype(np.float32).tofile("/tmp/py_ref_latent.bin")

    # Encode text using Qwen3
    print("\nEncoding text with Qwen3...")
    from transformers import AutoModelForCausalLM, AutoTokenizer

    qwen = AutoModelForCausalLM.from_pretrained(
        os.path.join(model_dir, "text_encoder"),
        torch_dtype=torch.bfloat16,
        device_map=str(device),
        local_files_only=True
    )
    tokenizer = AutoTokenizer.from_pretrained(
        os.path.join(model_dir, "tokenizer"),
        local_files_only=True
    )

    messages = [{"role": "user", "content": prompt}]
    text = tokenizer.apply_chat_template(
        messages, tokenize=False, add_generation_prompt=True, enable_thinking=False,
    )

    tokens = tokenizer(text, return_tensors="pt", padding="max_length",
                       max_length=512, truncation=True)

    with torch.no_grad():
        output = qwen(
            input_ids=tokens.input_ids.to(device),
            attention_mask=tokens.attention_mask.to(device),
            output_hidden_states=True,
            use_cache=False,
        )

    # Concatenate layers 9, 18, 27 to get 7680 dim
    OUTPUT_LAYERS = [9, 18, 27]
    out = torch.stack([output.hidden_states[k] for k in OUTPUT_LAYERS], dim=1)
    text_emb = rearrange(out, "b c l d -> b l (c d)")

    text_flat = text_emb.float().cpu().numpy().flatten()
    print(f"Text emb: mean={text_flat.mean():.4f}, std={text_flat.std():.4f}")

    # Save for C comparison
    text_emb.float().cpu().numpy().astype(np.float32).tofile("/tmp/py_text_emb.bin")

    # Initialize noise
    print(f"\nInitializing noise with seed {seed}...")
    torch.manual_seed(seed)
    latent_h, latent_w = height // 16, width // 16
    z = torch.randn(1, 128, latent_h, latent_w, device=device, dtype=torch.bfloat16)

    z_flat = z.float().cpu().numpy().flatten()
    print(f"Initial noise: mean={z_flat.mean():.6f}, std={z_flat.std():.6f}")
    print(f"  first 8: {z_flat[:8]}")

    # Save for C comparison
    z.float().cpu().numpy().astype(np.float32).tofile("/tmp/py_noise.bin")

    # Prepare tokens
    print("\nPreparing tokens...")
    img, img_ids = prc_img(z[0], None)  # T=0
    img = img.unsqueeze(0)
    img_ids = img_ids.unsqueeze(0)

    t_coord = torch.tensor([10])  # T=10 for reference
    ref_tokens, ref_ids = prc_img(ref_latent[0], t_coord)
    ref_tokens = ref_tokens.unsqueeze(0)
    ref_ids = ref_ids.unsqueeze(0)

    txt, txt_ids = prc_txt(text_emb[0])
    txt = txt.unsqueeze(0)
    txt_ids = txt_ids.unsqueeze(0)

    # Run denoising
    print(f"\nRunning {num_steps} denoising steps...")
    timesteps = get_schedule(num_steps, img.shape[1])
    print(f"Schedule: {timesteps}")

    guidance_vec = torch.full((img.shape[0],), 1.0, device=device, dtype=torch.bfloat16)
    img_curr = img.to(torch.bfloat16)

    with torch.no_grad():
        for step, (t_curr, t_prev) in enumerate(zip(timesteps[:-1], timesteps[1:])):
            print(f"\n--- Step {step + 1}: t={t_curr:.6f} -> {t_prev:.6f} ---")

            t_vec = torch.full((img.shape[0],), t_curr, dtype=torch.bfloat16, device=device)
            img_input = torch.cat((img_curr, ref_tokens.to(torch.bfloat16)), dim=1)
            img_input_ids = torch.cat((img_ids, ref_ids), dim=1)

            pred = model(
                x=img_input, x_ids=img_input_ids, timesteps=t_vec,
                ctx=txt.to(torch.bfloat16), ctx_ids=txt_ids, guidance=guidance_vec,
            )
            pred = pred[:, :img_curr.shape[1]]

            pred_flat = pred.float().cpu().numpy().flatten()
            print(f"  Velocity: mean={pred_flat.mean():.6f}, std={pred_flat.std():.6f}")

            img_curr = img_curr + (t_prev - t_curr) * pred

    result = img_curr
    result_flat = result.float().cpu().numpy().flatten()
    print(f"\nResult: mean={result_flat.mean():.6f}, std={result_flat.std():.6f}")

    # Save result for comparison
    result.float().cpu().numpy().astype(np.float32).tofile("/tmp/py_result.bin")

    # Decode
    print("\nDecoding...")
    result_latent = result.reshape(1, latent_h, latent_w, 128).permute(0, 3, 1, 2)
    result_denorm = inv_batch_norm(result_latent, bn_mean, bn_var)
    result_unpatch = rearrange(result_denorm, "b (c p1 p2) h w -> b c (h p1) (w p2)", p1=2, p2=2)

    with torch.no_grad():
        decoded = vae.decode(result_unpatch).sample

    decoded = decoded[0].float().cpu().numpy()
    decoded = (decoded + 1) / 2
    decoded = np.clip(decoded * 255, 0, 255).astype(np.uint8)
    decoded = decoded.transpose(1, 2, 0)

    output_img = Image.fromarray(decoded)
    output_img.save("/tmp/py_woman_color.png")
    print("Saved output to /tmp/py_woman_color.png")
    print("\nFiles saved to /tmp/:")
    print("  py_noise.bin      - Initial noise")
    print("  py_ref_latent.bin - VAE-encoded reference")
    print("  py_text_emb.bin   - Text embeddings")
    print("  py_result.bin     - Final latent result")
    print("  py_woman_color.png - Decoded output image")


if __name__ == "__main__":
    main()
