# SEGA: Spectral-Energy Guided Attention for Resolution Extrapolation in Diffusion Transformers

<p align="center">
  <a href="https://rajabi2001.github.io/sega/"><img src="https://img.shields.io/badge/Project_Page-5C6BC0?style=flat-square" alt="Project Page"></a>
  &nbsp;
  <a href="https://arxiv.org/abs/2605.22668"><img src="https://img.shields.io/badge/Paper-arXiv-B31B1B?style=flat-square&logo=arxiv&logoColor=white" alt="Paper"></a>
</p>

<p align="center">
  <img src="assets/teaser.png" alt="SEGA teaser" width="900">
</p>

Official inference code for **SEGA**, a training-free method that dynamically rescales attention across RoPE components from the latent's spatial-frequency content at each denoising step. SEGA improves high-resolution synthesis without retraining, new weights, or architecture changes. Implementations are provided for **FLUX** ([`flux_sega/`](flux_sega/)) and **Qwen-Image** ([`qwen_sega/`](qwen_sega/)).

## Installation

```bash
git clone https://github.com/rajabi2001/sega.git
cd sega

pip install torch torchvision --index-url https://download.pytorch.org/whl/cu128
pip install -r requirements.txt
```

Model weights are fetched from Hugging Face on first run.

## Usage

**FLUX.1:**

```bash
cd flux_sega
python run_flux.py --prompt "Your prompt here." --height 4096 --width 4096
```

**Qwen-Image:**

```bash
cd qwen_sega
python run_qwen.py --prompt "Your prompt here." --height 4096 --width 4096
```

Outputs are saved under `outputs/` in each subdirectory.

## Multi-GPU inference

Generating ultra-high-resolution images can exceed the memory of a single GPU. Both `run_flux.py` and `run_qwen.py` accept a `--multi_gpu` flag that distributes the transformer blocks across **all visible CUDA devices** (CLIP and VAE stay on `cuda:0`; for Qwen the text encoder is offloaded to CPU). At least **2 GPUs** must be visible for this flag to take effect.

As a rule of thumb, you should pass `--multi_gpu` (with two or more GPUs visible) in these cases:

- **Qwen-Image** at **4096×4096 or higher**, when the available GPU does not have enough VRAM for a single-device run.
- **FLUX** at **6144×6144 or higher**, when the available GPU does not have enough VRAM for a single-device run.

If a single GPU has enough memory, you can omit `--multi_gpu` and run on one device. If you hit OOM, add `--multi_gpu` and make sure `CUDA_VISIBLE_DEVICES` exposes two or more GPUs.

**Example — FLUX at 6144×6144:**

```bash
cd flux_sega
CUDA_VISIBLE_DEVICES=0,1 python run_flux.py \
    --prompt "Your prompt here." \
    --height 6144 --width 6144 \
    --multi_gpu
```

**Example — Qwen-Image at 4096×4096:**

```bash
cd qwen_sega
CUDA_VISIBLE_DEVICES=0,1 python run_qwen.py \
    --prompt "Your prompt here." \
    --height 4096 --width 4096 \
    --multi_gpu
```

## Citation

```bibtex
@article{rajabi2026sega,
  title={SEGA: Spectral-Energy Guided Attention for Resolution Extrapolation in Diffusion Transformers},
  author={Rajabi, Javad and Shaban, Kimia and Roohi, Koorosh and Lindell, David B and Taati, Babak},
  journal={arXiv preprint arXiv:2605.22668},
  year={2026}
}
```

## Acknowledgments
This repository adapts the inference layout and scripts from [DyPE](https://github.com/guyyariv/DyPE).
