#!/usr/bin/env python3
"""
idTech3 wrapper for Spectral-Energy Guided Attention hi-res FLUX generation.

Upstream: https://github.com/rajabi2001/sega (flux_sega/ package layout).

Requires a local checkout (--repo) with flux_sega/ and PyTorch/diffusers deps.
Invoked by spec_energy_generate / cl_spec_energy_cmd from the engine client.
"""

from __future__ import annotations

import argparse
import os
import sys
import types


def _fail(msg: str) -> None:
    print(f"spec_energy wrapper: {msg}", file=sys.stderr)
    sys.exit(1)


def _move_to_device(v, device):
    import torch

    if isinstance(v, torch.Tensor):
        return v.to(device)
    if isinstance(v, tuple):
        return tuple(_move_to_device(x, device) for x in v)
    if isinstance(v, list):
        return [_move_to_device(x, device) for x in v]
    if isinstance(v, dict):
        return {k: _move_to_device(val, device) for k, val in v.items()}
    return v


def main() -> None:
    ap = argparse.ArgumentParser(description="Spectral-Energy FLUX image generation for idTech3")
    ap.add_argument("--repo", required=True, help="Path to upstream git checkout (contains flux_sega/)")
    ap.add_argument("--prompt", required=True, help="Text prompt")
    ap.add_argument("--output", required=True, help="Output PNG path")
    ap.add_argument("--height", type=int, default=4096)
    ap.add_argument("--width", type=int, default=4096)
    ap.add_argument("--steps", type=int, default=28)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument(
        "--checkpoint",
        default="Krea-dev",
        help='HF shorthand "dev", "Krea-dev", or full repo id / local path',
    )
    ap.add_argument("--multi-gpu", action="store_true")
    ap.add_argument("--guidance", type=float, default=4.5)
    args = ap.parse_args()

    repo = os.path.abspath(args.repo)
    flux_dir = os.path.join(repo, "flux_sega")
    if not os.path.isdir(flux_dir):
        _fail(f"flux_sega/ not found under {repo} (clone upstream repo from github.com/rajabi2001/sega)")

    sys.path.insert(0, flux_dir)

    try:
        import torch
        from pipeline_flux import FluxPipeline
        from transformer_flux import FluxTransformer2DModel
    except ImportError as exc:
        _fail(f"import failed ({exc}). pip install -r {repo}/requirements.txt")

    if not torch.cuda.is_available():
        _fail("CUDA is not available — hi-res FLUX requires an NVIDIA GPU")

    shorthand = {"dev", "Krea-dev"}
    model_id = (
        f"black-forest-labs/FLUX.1-{args.checkpoint}"
        if args.checkpoint in shorthand
        else args.checkpoint
    )

    print(f"spec_energy wrapper: loading {model_id} ...")
    num_gpus = torch.cuda.device_count()
    enable_multi_gpu = args.multi_gpu and num_gpus >= 2

    transformer = FluxTransformer2DModel.from_pretrained(
        model_id,
        subfolder="transformer",
        torch_dtype=torch.bfloat16,
        low_cpu_mem_usage=True,
    )
    pipe = FluxPipeline.from_pretrained(
        model_id,
        transformer=transformer,
        torch_dtype=torch.bfloat16,
    )
    pipe.vae.enable_tiling()

    if enable_multi_gpu:
        print(f"spec_energy wrapper: multi-GPU across {num_gpus} devices")
        transformer = pipe.transformer
        for module in (
            transformer.x_embedder,
            transformer.context_embedder,
            transformer.time_text_embed,
            transformer.pos_embed,
            transformer.norm_out,
            transformer.proj_out,
        ):
            module.to("cuda:0")

        dual_blocks_per_gpu = max(1, len(transformer.transformer_blocks) // num_gpus)
        for i, block in enumerate(transformer.transformer_blocks):
            device = f"cuda:{min(i // dual_blocks_per_gpu, num_gpus - 1)}"
            block.to(device)
            block._original_forward = block.forward.__func__
            block._target_device = device

            def forward_wrapper(self, *a, **kw):
                target_dev = self._target_device
                a = tuple(_move_to_device(x, target_dev) for x in a)
                kw = {k: _move_to_device(v, target_dev) for k, v in kw.items()}
                return _move_to_device(self._original_forward(self, *a, **kw), "cuda:0")

            block.forward = types.MethodType(forward_wrapper, block)

        single_blocks_per_gpu = max(1, len(transformer.single_transformer_blocks) // num_gpus)
        for i, block in enumerate(transformer.single_transformer_blocks):
            device = f"cuda:{min(i // single_blocks_per_gpu, num_gpus - 1)}"
            block.to(device)
            block._original_forward = block.forward.__func__
            block._target_device = device

            def forward_wrapper_single(self, *a, **kw):
                target_dev = self._target_device
                a = tuple(_move_to_device(x, target_dev) for x in a)
                kw = {k: _move_to_device(v, target_dev) for k, v in kw.items()}
                return _move_to_device(self._original_forward(self, *a, **kw), "cuda:0")

            block.forward = types.MethodType(forward_wrapper_single, block)

        transformer._original_transformer_forward = transformer.forward.__func__

        def transformer_forward_wrapper(self, *a, **kw):
            a = tuple(_move_to_device(x, "cuda:0") for x in a)
            kw = {k: _move_to_device(v, "cuda:0") for k, v in kw.items()}
            result = self._original_transformer_forward(self, *a, **kw)
            if hasattr(result, "sample"):
                result.sample = result.sample.to("cuda:0")
            return result

        transformer.forward = types.MethodType(transformer_forward_wrapper, transformer)
        pipe.text_encoder.to("cuda:0")
        pipe.text_encoder_2.to("cpu")
        pipe.vae.to("cuda:0")
        pipe._execution_device = torch.device("cuda:0")
    else:
        gpu_mem_gb = torch.cuda.get_device_properties(0).total_memory / (1024 ** 3)
        if gpu_mem_gb < 60:
            print(f"spec_energy wrapper: CPU offload (GPU {gpu_mem_gb:.1f} GB < 60 GB)")
            pipe.enable_model_cpu_offload()
        else:
            pipe.to("cuda")

    out_dir = os.path.dirname(os.path.abspath(args.output))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    gen_device = "cuda"
    generator = torch.Generator(gen_device).manual_seed(args.seed)
    print(f"spec_energy wrapper: generating {args.width}x{args.height} ({args.steps} steps, seed {args.seed}) ...")

    image = pipe(
        prompt=args.prompt,
        height=args.height,
        width=args.width,
        guidance_scale=args.guidance,
        generator=generator,
        num_inference_steps=args.steps,
    ).images[0]

    image.save(args.output)
    print(f"spec_energy wrapper: saved {args.output}")


if __name__ == "__main__":
    main()
