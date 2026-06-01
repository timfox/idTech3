"""SEGA inference for Qwen-Image diffusion transformers."""

import os
import types
import argparse
import torch

from pipeline_qwenimage import QwenImagePipeline
from transformer_qwenimage import QwenImageTransformer2DModel

GPU_MEMORY_THRESHOLD_GB = 60


def get_gpu_memory_gb() -> float:
    """Return total GPU memory in gigabytes for the current CUDA device, or 0 if unavailable."""
    if not torch.cuda.is_available():
        return 0.0
    return torch.cuda.get_device_properties(0).total_memory / (1024 ** 3)


def _move_to_device(v, device):
    """Recursively move tensors to device, handling tuples, lists, dicts, and ModelOutput."""
    if isinstance(v, torch.Tensor):
        return v.to(device)
    elif isinstance(v, tuple):
        return tuple(_move_to_device(x, device) for x in v)
    elif isinstance(v, list):
        return [_move_to_device(x, device) for x in v]
    elif isinstance(v, dict):
        moved = {k: _move_to_device(val, device) for k, val in v.items()}
        if type(v) is not dict:
            try:
                return type(v)(**moved)
            except Exception:
                pass
        return moved
    return v


def main():
    parser = argparse.ArgumentParser(
        description='Generate ultra-high resolution images with Qwen (SEGA)'
    )
    parser.add_argument(
        '--prompt',
        type=str,
        default="A woman kneels on the forest floor, smiling as she offers grapes to a large brown bear beside her, surrounded by tall birch trees.",
        help='Text prompt for image generation',
    )
    parser.add_argument('--height', type=int, default=4096, help='Image height in pixels')
    parser.add_argument('--width', type=int, default=4096, help='Image width in pixels')
    parser.add_argument('--steps', type=int, default=40, help='Number of inference steps')
    parser.add_argument('--seed', type=int, default=0, help='Random seed for reproducibility')
    parser.add_argument(
        '--multi_gpu',
        action='store_true',
        help='Distribute transformer blocks across all available GPUs',
    )
    parser.add_argument('--output_dir', type=str, default='outputs', help='Output directory for generated images')

    args = parser.parse_args()

    output_dir = args.output_dir
    os.makedirs(output_dir, exist_ok=True)

    model_name = "Qwen/Qwen-Image"

    torch_dtype = torch.bfloat16 if torch.cuda.is_available() else torch.float32

    num_gpus = torch.cuda.device_count()
    enable_multi_gpu = args.multi_gpu and num_gpus >= 2

    if enable_multi_gpu:
        print(f"[info] Multi-GPU enabled: distributing Qwen transformer across {num_gpus} GPUs")

    print(f"Loading {model_name}...")

    # Load transformer on CPU first so diffusers doesn't pin the text encoder to cuda:0,
    # which would crowd out activation memory at high resolutions.
    transformer = QwenImageTransformer2DModel.from_pretrained(
        model_name,
        subfolder="transformer",
        torch_dtype=torch_dtype,
        low_cpu_mem_usage=True,
    )

    pipe = QwenImagePipeline.from_pretrained(
        model_name,
        transformer=transformer,
        torch_dtype=torch_dtype,
    )

    pipe.vae.enable_tiling()

    if enable_multi_gpu:
        transformer = pipe.transformer

        # Move all non-block submodules to cuda:0
        for name, child in transformer.named_children():
            if name != "transformer_blocks":
                child.to("cuda:0")
                print(f"  {name} -> cuda:0")

        # Distribute transformer blocks across GPUs
        blocks_per_gpu = len(transformer.transformer_blocks) // num_gpus
        for i, block in enumerate(transformer.transformer_blocks):
            device = f"cuda:{min(i // blocks_per_gpu, num_gpus - 1)}"
            block.to(device)
            block._original_forward = block.forward.__func__
            block._target_device = device

            def forward_wrapper(self, *args, **kwargs):
                target_dev = self._target_device
                args = tuple(_move_to_device(a, target_dev) for a in args)
                kwargs = {k: _move_to_device(v, target_dev) for k, v in kwargs.items()}
                return _move_to_device(self._original_forward(self, *args, **kwargs), "cuda:0")

            block.forward = types.MethodType(forward_wrapper, block)
            print(f"  Transformer block {i} -> {device}")

        # Wrap the top-level transformer forward to ensure all inputs land on cuda:0
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

        # Text encoder stays on CPU intentionally: it only runs once for prompt encoding
        # and weighs ~14 GB. Keeping it on GPU would waste memory across all 40 denoising
        # steps where it sits idle.
        #
        # Final layout example (2×L40S 44 GB, bfloat16):
        #   cuda:0  transformer first half + VAE  → large free pool for activations
        #   cuda:1  transformer second half        → large free pool for activations
        #   CPU     text encoder (~14 GB, encodes once)
        pipe.text_encoder.to("cpu")
        pipe.vae.to("cuda:0")
        print(f"[info] VAE on cuda:0, text encoder on CPU, transformer across {num_gpus} GPUs")

        # Patch 1: hard-wire _execution_device to cuda:0.
        # Because text_encoder is the first registered component, diffusers' device
        # property returns cpu, causing latent tensors to be created on CPU — a fatal
        # mismatch with the cuda:0 generator.
        _orig_cls = type(pipe)
        pipe.__class__ = type(
            _orig_cls.__name__,
            (_orig_cls,),
            {"_execution_device": property(lambda self: torch.device("cuda:0"))},
        )
        print("[info] Patched _execution_device -> cuda:0")

        # Patch 2: text encoder device bridge.
        # _get_qwen_prompt_embeds sends tokenized inputs to _execution_device (cuda:0),
        # but text_encoder weights live on CPU. Wrap forward to transparently move
        # inputs to CPU, run on CPU, then move outputs back to cuda:0.
        _real_te_forward = pipe.text_encoder.forward

        def _te_forward_cpu_bridge(*args, **kwargs):
            args = tuple(_move_to_device(a, "cpu") for a in args)
            kwargs = {k: _move_to_device(v, "cpu") for k, v in kwargs.items()}
            return _move_to_device(_real_te_forward(*args, **kwargs), "cuda:0")

        pipe.text_encoder.forward = _te_forward_cpu_bridge
        print("[info] Wrapped text_encoder forward with CPU<->cuda:0 bridge")
    else:
        if get_gpu_memory_gb() < GPU_MEMORY_THRESHOLD_GB:
            print(f"[info] CPU offload enabled (GPU memory: {get_gpu_memory_gb():.1f}GB < {GPU_MEMORY_THRESHOLD_GB}GB)")
            pipe.enable_model_cpu_offload()
        else:
            pipe.to("cuda")

    positive_magic = ", Ultra HD, 4K, cinematic composition."
    full_prompt = args.prompt + positive_magic
    negative_prompt = ""

    if enable_multi_gpu:
        gen_device = "cuda:0"
    elif torch.cuda.is_available():
        gen_device = "cuda"
    else:
        gen_device = "cpu"
    generator = torch.Generator(device=gen_device).manual_seed(args.seed)

    print(f"Generating {args.height}x{args.width} image with {args.steps} steps...")

    image = pipe(
        prompt=full_prompt,
        negative_prompt=negative_prompt,
        width=args.width,
        height=args.height,
        num_inference_steps=args.steps,
        true_cfg_scale=4.0,
        generator=generator,
    ).images[0]

    prompt_part = args.prompt.split()[0:3]
    multigpu_suffix = "_multigpu" if enable_multi_gpu else ""
    filename = os.path.join(
        output_dir,
        f"qwen{multigpu_suffix}_{args.height}x{args.width}_{'_'.join(prompt_part).lower()}_seed_{args.seed}_steps_{args.steps}.png",
    )

    image.save(filename)
    print(f"Image saved to: {filename}")


if __name__ == "__main__":
    main()
