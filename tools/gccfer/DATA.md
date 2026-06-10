# GCC-FER Dataset Acquisition

Reference: [GCC-FER repository](https://github.com/SonalikaSingh/GCCFER) (Singh et al., arXiv:2606.07063).

## Manifest format

CSV with header:

```csv
path,culture,expression
/path/to/video.mp4,caucasian,happy
```

- **path**: video file or directory of frames
- **culture**: `caucasian` | `east_asian` | `south_asian` | `african`
- **expression**: `angry` | `disgust` | `fear` | `happy` | `neutral` | `sad` | `surprise`

Full dataset: **23,934** clips (Table II).

## Validation

```bash
python validate_manifest.py --manifest /path/to/gccfer.csv
```

## AU priors (Algorithm 1 Phase 1)

Requires **Py-Feat** for production:

```bash
pip install py-feat
python extract_au_priors.py --manifest gccfer.csv --output cultural_priors.json
```

CI fixture uses `--synthetic-au` (see `extract_au_priors.py`).

## Training

```bash
python train_cafer.py --manifest gccfer.csv --priors cultural_priors.json --output checkpoints/
```

Mini fixture (CI): `fixtures/mini_manifest.csv` (10 clips).

## Checkpoints

Set engine cvar `cl_gccfer_checkpoint` or pass `--checkpoint` to `infer_cafer.py`.
Expected UAR after real training: ~65% GCC-FER / ~64% DFEW (Table III–IV).

## License / disk

Obtain GCC-FER from upstream authors; not redistributed in this repo.
Minimum: ~50 GB video storage; GPU 8 GB+ for ViViT training.
