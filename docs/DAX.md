# DaX — General Pathology Representations Across Scales

Pathology vision foundation model (Zhao et al., [arXiv:2606.06983](https://arxiv.org/abs/2606.06983)).

Project page: [alibaba-damo-academy.github.io/DaX/benchboard](https://alibaba-damo-academy.github.io/DaX/benchboard/)

## Overview

| Component | Description |
|-----------|-------------|
| **DaX** | ViT-L/16, DINOv3 init, two-stage SSL (continuous mag + Gram-anchored dense refinement) |
| **Pretrain** | 104,569 WSIs (TCGA, GTEx, HistAI); 1920×1920 patches @ 0.5 μm/px, anchors 2.5×–20× |
| **Benchmark** | 161 tasks, 44 datasets, 28,182 patients, 34,394 slides; 5×4 patient-level CV |

## Engine (C)

Always compiled into `qcommon`:

- Benchmark taxonomy (4 domains, 9 categories, 161 tasks)
- Table 2 model registry + mean scores
- 5×4 fold assignment + statistical ranking (Eq. 1)
- Gram matrix Frobenius diff helper (Stage 2)

**Cvars**

| Cvar | Default | Role |
|------|---------|------|
| `cl_dax_enable` | `1` | Enable console + startup log |
| `cl_dax_repo` | `""` | Repo root for Python tools |
| `cl_dax_python` | `python3` | Interpreter for extract/eval/pretrain |
| `cl_dax_encoder` | `ViT-L/16` | Encoder architecture label |

**Commands**

```
dax_info
dax_benchmark
dax_models
dax_eval_test
dax_extract <patch_dir|list.json> [--checkpoint PATH]
dax_eval [--features PATH --labels PATH]
dax_pretrain_stage1 [--data PATH --init DINOv3.ckpt]
dax_pretrain_stage2 --checkpoint stage1.pt
```

## Python pipeline

```bash
cd tools/dax
pip install -r requirements.txt

# Stage 1: continuous magnification + cross-scale SSL
python pretrain_stage1.py --dry-run
python pretrain_stage1.py --data /path/to/patches --init dinov3_vitl.pth

# Stage 2: multi-size crops + Gram-anchored consistency
python pretrain_stage2.py --checkpoint stage1.pt --dry-run

# Frozen encoder → MIL downstream (benchboard protocol)
python extract_features.py /path/to/patches --checkpoint dax_vitl.pt --output feats.pt
python evaluate_benchmark.py --dry-run
python evaluate_benchmark.py --features feats.pt --labels tasks.json --aggregation abmil
```

### Pretraining (paper §2.2)

**Stage 1**

- Init: natural-image DINOv3 ViT-L weights
- Continuous magnification 2.5×–20× from multi-resolution patch pool
- Enlarged local–global scale gap (same anchor region)
- Augmentations: arbitrary rotation, stain jitter, Gaussian blur

**Stage 2**

- Multi-size crop pairs: (512,224), (384,168), (768,336)
- Gram-anchored dense consistency vs. early teacher
- 2× global resolution (up to 1536×1536)

### Evaluation (paper §2.3)

1. Patient-level 5×4 cross-validation (20 folds)
2. Tile WSIs at 10× or 20×; frozen encoder patch embeddings
3. Mean pooling or ABMIL slide aggregation
4. Task-specific head; rank by pairwise fold-level significance (α=0.05)

## Tests

```bash
cmake --build build-vk-Release --target unit_dax
./build-vk-Release/unit_dax
```

## API (C)

```c
#include "dax/dax.h"

dax_benchmark_stats_t stats = Dax_BenchmarkStats();
int rank = Dax_StatisticalRankScore(results, num_models, model_results, 0, 0.05f);
float gram = Dax_GramMatrixFrobeniusDiff(student, teacher, tokens, dim);
```

## Notes

- Public DaX weights and full benchboard splits are distributed via the [project page](https://alibaba-damo-academy.github.io/DaX/benchboard/); this repo ships a **synthetic 10-task mini-bench** under `tools/dax/fixtures/mini_bench/` until assets are public.
- **`cl_dax_weights`**: path / `huggingface:` / URL hook for `load_dax_weights()` in `tools/dax/encoder.py` (errors clearly when missing).
- For in-engine WSI tiling preview, see `docs/IRIS.md` (separate digital-pathology renderer module).

## Phase 5 — Pretrain (deferred)

Full two-stage SSL pretrain is **not** required for eval-first CI:

| Script | Status |
|--------|--------|
| `pretrain_stage1.py` | Dry-run only until TCGA/GTEx/HistAI manifests supplied |
| `pretrain_stage2.py` | Dry-run only; requires Stage-1 checkpoint + WSI patch store |
| WSI patch store (1920 px, tissue filter) | Deferred with pretrain |

When public weights arrive: set `cl_dax_weights`, run `dax_extract`, then `dax_eval` against the full 161-task manifest.
