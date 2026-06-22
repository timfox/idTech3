# Manual FLUX Model Download Guide

This guide explains how to download FLUX models without relying on Hugging Face authentication.

## FLUX.1 Models

FLUX.1 models (schnell and dev) can be obtained from various sources:

### Option 1: Direct Download (If Available)

Some communities provide direct download links or mirrors. Check:
- FLUX community forums
- Model sharing sites
- Torrent/magnet links

### Option 2: Using Alternative Tools

#### Using `wget` with Session Cookies
If you have access to Hugging Face but want to avoid CLI:

1. Log into Hugging Face in your browser
2. Extract session cookies
3. Use wget with cookies:
```bash
wget --load-cookies cookies.txt \
     https://huggingface.co/black-forest-labs/FLUX.1-schnell/resolve/main/flux1-schnell.safetensors
```

#### Using Python Requests
```python
import requests

session = requests.Session()
# Login and get cookies, then download files
```

### Option 3: Manual File Placement

If you obtain files from any source, place them in this structure:

```
flux1-schnell/
├── vae/
│   └── diffusion_pytorch_model.safetensors  (from ae.safetensors, ~335MB)
├── transformer/
│   └── diffusion_pytorch_model.safetensors  (from flux1-schnell.safetensors, ~23.8GB)
├── text_encoder/
│   ├── model.safetensors                     (from text_encoder_2/model.safetensors)
│   └── config.json                           (from text_encoder_2/config.json)
└── tokenizer/
    ├── tokenizer.json                        (from tokenizer_2/tokenizer.json)
    ├── special_tokens_map.json              (from tokenizer_2/special_tokens_map.json)
    └── tokenizer_config.json                 (from tokenizer_2/tokenizer_config.json)
```

### File Size Verification

Verify your downloads have correct sizes:
- `vae/diffusion_pytorch_model.safetensors`: ~335MB (not 140 bytes!)
- `transformer/diffusion_pytorch_model.safetensors`: ~23.8GB
- `text_encoder/model.safetensors`: ~4-5GB

If files are much smaller (like 140 bytes), they're error pages, not model files.

## FLUX.2 Models

FLUX.2-dev models are typically easier to obtain:
- Use the provided `download_model.sh` script
- Or download manually from Hugging Face (FLUX.2 is less restricted)

## Verification

After downloading, verify files:
```bash
cd /path/to/idtech3/release
./idtech3 +set cl_flux_enable 1 +set cl_flux_model flux1-schnell
# Should load without "invalid header size" errors
```

## Troubleshooting

**"invalid header size" error:**
- File is corrupted or incomplete
- File is an error page (check size - should be MB/GB, not bytes)
- Re-download the file

**"VAE file not found" error:**
- Check file paths match expected structure
- Ensure files are in correct subdirectories
- Verify file names match exactly
