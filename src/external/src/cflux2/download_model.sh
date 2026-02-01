#!/bin/bash
set -e

BASE="https://huggingface.co/black-forest-labs/FLUX.2-klein-4B/resolve/main"
OUT="./flux-klein-model"

mkdir -p "$OUT"/{text_encoder,tokenizer,transformer,vae}

echo "=== Downloading FLUX.2-klein-4B Model Files ==="
echo "This will download ~16GB total"
echo ""

# Download all files in parallel for faster speed
echo "Starting parallel downloads..."

# Small files first (fast)
curl -L -o "$OUT/text_encoder/config.json" "$BASE/text_encoder/config.json" &
curl -L -o "$OUT/text_encoder/generation_config.json" "$BASE/text_encoder/generation_config.json" &
curl -L -o "$OUT/text_encoder/model.safetensors.index.json" "$BASE/text_encoder/model.safetensors.index.json" &
curl -L -o "$OUT/transformer/config.json" "$BASE/transformer/config.json" &
curl -L -o "$OUT/vae/config.json" "$BASE/vae/config.json" &

# Tokenizer files
curl -L -o "$OUT/tokenizer/added_tokens.json" "$BASE/tokenizer/added_tokens.json" &
curl -L -o "$OUT/tokenizer/chat_template.jinja" "$BASE/tokenizer/chat_template.jinja" &
curl -L -o "$OUT/tokenizer/merges.txt" "$BASE/tokenizer/merges.txt" &
curl -L -o "$OUT/tokenizer/special_tokens_map.json" "$BASE/tokenizer/special_tokens_map.json" &
curl -L -o "$OUT/tokenizer/tokenizer.json" "$BASE/tokenizer/tokenizer.json" &
curl -L -o "$OUT/tokenizer/tokenizer_config.json" "$BASE/tokenizer/tokenizer_config.json" &
curl -L -o "$OUT/tokenizer/vocab.json" "$BASE/tokenizer/vocab.json" &

# Large files (slowest)
echo "Downloading large model files..."
curl -L -o "$OUT/text_encoder/model-00001-of-00002.safetensors" "$BASE/text_encoder/model-00001-of-00002.safetensors" &
curl -L -o "$OUT/text_encoder/model-00002-of-00002.safetensors" "$BASE/text_encoder/model-00002-of-00002.safetensors" &
curl -L -o "$OUT/transformer/diffusion_pytorch_model.safetensors" "$BASE/transformer/diffusion_pytorch_model.safetensors" &
curl -L -o "$OUT/vae/diffusion_pytorch_model.safetensors" "$BASE/vae/diffusion_pytorch_model.safetensors" &

# Wait for all downloads to complete
wait

echo "✅ All downloads completed!"
echo "Total size: ~16GB"
