"""SEGA inference for FLUX diffusion transformers."""

import types
import torch
import argparse
import os

from pipeline_flux import FluxPipeline
from transformer_flux import FluxTransformer2DModel

GPU_MEMORY_THRESHOLD_GB = 60


def get_gpu_memory_gb() -> float:
    """Return total GPU memory in gigabytes for the current CUDA device, or 0 if unavailable."""
    if not torch.cuda.is_available():
        return 0.0
    return torch.cuda.get_device_properties(0).total_memory / (1024 ** 3)


def _move_to_device(v, device):
    """Recursively move tensors to device, handling tuples, lists, and dicts."""
    if isinstance(v, torch.Tensor):
        return v.to(device)
    elif isinstance(v, tuple):
        return tuple(_move_to_device(x, device) for x in v)
    elif isinstance(v, list):
        return [_move_to_device(x, device) for x in v]
    elif isinstance(v, dict):
        return {k: _move_to_device(val, device) for k, val in v.items()}
    return v


def main():
    parser = argparse.ArgumentParser(
        description='Generate ultra-high resolution images with FLUX (SEGA)'
    )
    parser.add_argument(
        '--prompt',
        type=str,
        default="A female astronaut in a sleek, high-tech suit stands against a backdrop of a turbulent cosmic scene featuring asteroids and a distant, fire-ridden planet, with spacecraft flying in formation.",
        help='Text prompt for image generation',
    )
    parser.add_argument('--height', type=int, default=4096, help='Image height in pixels')
    parser.add_argument('--width', type=int, default=4096, help='Image width in pixels')
    parser.add_argument('--steps', type=int, default=28, help='Number of inference steps')
    parser.add_argument('--seed', type=int, default=0, help='Random seed for reproducibility')
    parser.add_argument(
        '--checkpoint',
        type=str,
        default='Krea-dev',
        help='Checkpoint: "dev", "Krea-dev" (expands to black-forest-labs/FLUX.1-*), '
             'or any HF repo ID / local path containing a full Flux-dev pipeline',
    )
    parser.add_argument(
        '--multi_gpu',
        action='store_true',
        help='Distribute transformer blocks across all available GPUs',
    )
    parser.add_argument('--output_dir', type=str, default='outputs', help='Output directory for generated images')

    args = parser.parse_args()

    output_dir = args.output_dir
    os.makedirs(output_dir, exist_ok=True)

    _SHORTHAND = {"dev", "Krea-dev"}
    model_id = (
        f"black-forest-labs/FLUX.1-{args.checkpoint}"
        if args.checkpoint in _SHORTHAND
        else args.checkpoint
    )
    print(f"[info] Loading pipeline from: {model_id}")

    num_gpus = torch.cuda.device_count()
    enable_multi_gpu = args.multi_gpu and num_gpus >= 2

    if enable_multi_gpu:
        print(f"[info] Multi-GPU enabled: distributing FLUX.1 transformer across {num_gpus} GPUs")

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
        transformer = pipe.transformer

        for module in [
            transformer.x_embedder,
            transformer.context_embedder,
            transformer.time_text_embed,
            transformer.pos_embed,
            transformer.norm_out,
            transformer.proj_out,
        ]:
            module.to("cuda:0")

        dual_blocks_per_gpu = len(transformer.transformer_blocks) // num_gpus
        for i, block in enumerate(transformer.transformer_blocks):
            device = f"cuda:{min(i // dual_blocks_per_gpu, num_gpus - 1)}"
            block.to(device)
            block._original_forward = block.forward.__func__
            block._target_device = device

            def forward_wrapper(self, *args, **kwargs):
                target_dev = self._target_device
                args = tuple(_move_to_device(a, target_dev) for a in args)
                kwargs = {k: _move_to_device(v, target_dev) for k, v in kwargs.items()}
                return _move_to_device(self._original_forward(self, *args, **kwargs), "cuda:0")

            block.forward = types.MethodType(forward_wrapper, block)
            print(f"  Dual block {i} -> {device}")

        single_blocks_per_gpu = len(transformer.single_transformer_blocks) // num_gpus
        for i, block in enumerate(transformer.single_transformer_blocks):
            device = f"cuda:{min(i // single_blocks_per_gpu, num_gpus - 1)}"
            block.to(device)
            block._original_forward = block.forward.__func__
            block._target_device = device

            def forward_wrapper_single(self, *args, **kwargs):
                target_dev = self._target_device
                args = tuple(_move_to_device(a, target_dev) for a in args)
                kwargs = {k: _move_to_device(v, target_dev) for k, v in kwargs.items()}
                return _move_to_device(self._original_forward(self, *args, **kwargs), "cuda:0")

            block.forward = types.MethodType(forward_wrapper_single, block)
            print(f"  Single block {i} -> {device}")

        transformer._original_transformer_forward = transformer.forward.__func__

        def transformer_forward_wrapper(self, *args, **kwargs):
            args = tuple(_move_to_device(a, "cuda:0") for a in args)
            kwargs = {k: _move_to_device(v, "cuda:0") for k, v in kwargs.items()}
            result = self._original_transformer_forward(self, *args, **kwargs)
            if hasattr(result, 'sample'):
                result.sample = result.sample.to("cuda:0")
            return result

        transformer.forward = types.MethodType(transformer_forward_wrapper, transformer)
        print("[info] Transformer forward wrapped for multi-GPU device management")

        pipe.text_encoder.to("cuda:0")
        pipe.text_encoder_2.to("cpu")
        pipe.vae.to("cuda:0")
        pipe._execution_device = torch.device("cuda:0")
        print(f"[info] CLIP+VAE on cuda:0, T5-XXL on CPU, transformer across {num_gpus} GPUs")
    else:
        if get_gpu_memory_gb() < GPU_MEMORY_THRESHOLD_GB:
            print(f"[info] CPU offload enabled (GPU memory: {get_gpu_memory_gb():.1f}GB < {GPU_MEMORY_THRESHOLD_GB}GB)")
            pipe.enable_model_cpu_offload()
        else:
            pipe.to("cuda")

    _gen_device = "cuda" if getattr(pipe, "_execution_device", torch.device("cuda")).type == "cuda" else "cpu"
    generator = torch.Generator(_gen_device).manual_seed(args.seed)

    prompt_part = args.prompt.split()[0:3]
    multigpu_suffix = "_multigpu" if enable_multi_gpu else ""
    ckpt_tag = os.path.basename(model_id.rstrip("/"))

    print(f"Generating {args.height}x{args.width} image with {args.steps} steps...")

    image = pipe(
        prompt=args.prompt,
        height=args.height,
        width=args.width,
        guidance_scale=4.5,
        generator=generator,
        num_inference_steps=args.steps,
    ).images[0]

    filename = os.path.join(
        output_dir,
        f"{ckpt_tag}{multigpu_suffix}_{'_'.join([p.replace(',', '') for p in prompt_part]).lower()}_res_{args.height}x{args.width}_seed_{args.seed}.png",
    )
    image.save(filename)
    print(f"Image saved to: {filename}")


if __name__ == "__main__":
    main()
