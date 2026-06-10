# GCC-FER / CA-FER

Culture-aware dynamic facial expression recognition (Singh et al., [arXiv:2606.07063](https://arxiv.org/abs/2606.07063)).

## Overview

| Component | Description |
|-----------|-------------|
| **GCC-FER** | 23,934 five-second video clips, 4 cultures × 7 Ekman expressions, 16 frames @ 224×224 |
| **CA-FER** | ViViT backbone + AU-grounded cultural embeddings + FiLM adaptation `f' = a_c ⊙ f + b_c` |

Upstream dataset/code (when released): [github.com/SonalikaSingh/GCCFER](https://github.com/SonalikaSingh/GCCFER)

## Engine (C)

Always compiled into `qcommon`:

- Dataset Table II statistics
- AU prior math + latent adaptation (Algorithm 1)
- Paper benchmark tables (GCC-FER Table III, DFEW Table IV)

**Cvars**

| Cvar | Default | Role |
|------|---------|------|
| `cl_gccfer_enable` | `1` | Enable console + startup log |
| `cl_gccfer_repo` | `""` | Repo root for Python tools (defaults to `.`) |
| `cl_gccfer_python` | `python3` | Interpreter for `gccfer_infer` |

**Commands**

```
gccfer_info
gccfer_dataset
gccfer_benchmark [gccfer|dfew]
gccfer_adapt_test [culture|global]
gccfer_infer <video_or_frames> [culture]
```

## Python training pipeline

```bash
cd tools/gccfer
pip install -r requirements.txt

# Manifest CSV: path,culture,expression
python extract_au_priors.py --manifest /path/to/gccfer.csv --output cultural_priors.json
python train_cafer.py --manifest /path/to/gccfer.csv --priors cultural_priors.json --output checkpoints
python infer_cafer.py --input sample.mp4 --checkpoint checkpoints/cafer_fold0.pt --culture global
```

### Model details (paper §III)

- **Backbone**: ViViT Model-3 (`google/vivit-b-16x2`)
- **Latent**: concat(CLS, mean patch) → linear 768-d
- **Culture prior**: 20 AUs × 4 stats → min-max norm → `W_proj` → 128-d embedding
- **Adaptation**: `a_c = W_a e_c`, `b_c = W_b e_c`, `f' = a_c ⊙ f + b_c`
- **Loss**: focal loss + label smoothing (0.1)
- **Optimizer**: AdamW, lr=1e-5, batch 8, grad clip 1.0

When culture is unknown at inference, use **global** prior (mean AU profile across cultures).

## Tests

```bash
cmake --build build-vk-Release --target unit_gccfer
./build-vk-Release/unit_gccfer
```

## API (C)

```c
#include "gccfer/gccfer.h"

gccfer_cafer_params_t params;
Gccfer_CaferInitDefaults(&params, 42u);
Gccfer_GenerateAdaptParams(&params, GCCFER_CULTURE_GLOBAL, a, b);
Gccfer_AdaptLatent(latent768, a, b, GCCFER_LATENT_DIM, adapted);
```
