#!/bin/bash
set -e

echo "=== FLUX Model Setup for idTech3 (Multi-Variant) ==="
echo ""

# Model variant selection
MODEL_VARIANT="${1:-flux2-dev}"
echo "🎯 Setting up FLUX model variant: $MODEL_VARIANT"

# Map variant to directory and URL
case "$MODEL_VARIANT" in
    "flux1-schnell")
        MODEL_DIR="flux1-schnell"
        MODEL_URL_BASE="https://huggingface.co/black-forest-labs/FLUX.1-schnell/resolve/main"
        MODEL_SIZE="~12GB"
        MODEL_DESC="Fast distilled model (~1-5 seconds generation)"
        ;;
    "flux1-dev")
        MODEL_DIR="flux1-dev"
        MODEL_URL_BASE="https://huggingface.co/black-forest-labs/FLUX.1-dev/resolve/main"
        MODEL_SIZE="~23GB"
        MODEL_DESC="Balanced model (~5-15 seconds generation)"
        ;;
    "flux2-dev")
        MODEL_DIR="flux2-dev"
        MODEL_URL_BASE="https://huggingface.co/black-forest-labs/FLUX.2-dev/resolve/main"
        MODEL_SIZE="~16GB"
        MODEL_DESC="High quality model (~30-120+ seconds generation)"
        ;;
    *)
        echo "❌ Unknown model variant: $MODEL_VARIANT"
        echo "   Supported variants:"
        echo "   - flux1-schnell: Fast distilled model (~12GB, ~1-5 seconds)"
        echo "   - flux1-dev: Balanced model (~23GB, ~5-15 seconds)"
        echo "   - flux2-dev: High quality model (~16GB, ~30-120+ seconds)"
        exit 1
        ;;
esac

echo "📁 Using model directory: $MODEL_DIR"
echo "📏 Approximate size: $MODEL_SIZE"
echo "⚡ Performance: $MODEL_DESC"
echo "🔗 Model URL base: $MODEL_URL_BASE"
echo ""

# Check if we're in a release/flux directory
if [ ! -f "../../idtech3" ] && [ ! -f "../../quake3" ] && [ ! -f "../../ioquake3" ]; then
    echo "⚠️  Warning: Could not find idTech3 executable in grandparent directory"
    echo "   Make sure you're running this from your release/flux directory"
    echo "   Example: cd /path/to/idtech3/release/flux && /path/to/idtech3/src/external/src/cflux2/setup_models_variants.sh $MODEL_VARIANT"
    echo ""
fi

# Check if models already exist
if [ -d "$MODEL_DIR" ]; then
    echo "✅ FLUX model directory already exists: $MODEL_DIR/"
    echo "   Checking contents..."

    # Check for key files (different models have different file structures)
    missing_files=()
    case "$MODEL_VARIANT" in
        "flux1-schnell"|"flux1-dev")
            [ ! -f "$MODEL_DIR/vae/diffusion_pytorch_model.safetensors" ] && missing_files+=("VAE model")
            [ ! -f "$MODEL_DIR/transformer/diffusion_pytorch_model.safetensors" ] && missing_files+=("Transformer model")
            [ ! -f "$MODEL_DIR/text_encoder/model.safetensors" ] && missing_files+=("Text encoder model")
            ;;
        "flux2-dev")
            [ ! -f "$MODEL_DIR/vae/diffusion_pytorch_model.safetensors" ] && missing_files+=("VAE model")
            [ ! -f "$MODEL_DIR/transformer/diffusion_pytorch_model.safetensors" ] && missing_files+=("Transformer model")
            [ ! -f "$MODEL_DIR/text_encoder/model-00001-of-00002.safetensors" ] && missing_files+=("Text encoder model")
            ;;
    esac

    if [ ${#missing_files[@]} -eq 0 ]; then
        echo "✅ All required model files found!"
        echo ""
        echo "🎉 FLUX $MODEL_VARIANT is ready to use!"
        echo "   In-game: /cl_flux_model $MODEL_VARIANT"
        echo "   Generate: flux_generate \"your prompt here\""
        exit 0
    else
        echo "❌ Missing files: ${missing_files[*]}"
        echo "   The download may not be complete."
        echo ""
    fi
fi

echo "📥 Starting FLUX $MODEL_VARIANT model download..."
echo "   This will download $MODEL_SIZE of model files to: $MODEL_DIR/"
echo "   This may take a while depending on your internet connection."
echo ""
echo "⚠️  Note: FLUX.1 models may require authentication or alternative download methods."
echo "   If downloads fail, see README_idtech3.md for manual download instructions."
echo ""

# Create model directory and subdirectories
mkdir -p "$MODEL_DIR/vae" "$MODEL_DIR/transformer" "$MODEL_DIR/text_encoder"

# Download based on model variant
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "$MODEL_VARIANT" in
    "flux1-schnell")
        MODEL_FILE="flux1-schnell.safetensors"
        ;;
    "flux1-dev")
        MODEL_FILE="flux1-dev.safetensors"
        ;;
esac

case "$MODEL_VARIANT" in
    "flux1-schnell"|"flux1-dev")
        # FLUX.1 models use combined safetensors files and separate directories
        echo "Downloading FLUX.1 model files..."
        echo "⚠️  Note: Direct downloads may fail if authentication is required."
        echo "   If downloads fail, you can manually download files from alternative sources."
        echo ""

        # Download the combined model file
        echo "Downloading transformer model ($MODEL_FILE)..."
        curl -L -f -o "$MODEL_DIR/$MODEL_FILE" "$MODEL_URL_BASE/$MODEL_FILE" || {
            echo "❌ Failed to download $MODEL_FILE"
            echo "   This file may require authentication or alternative download method"
            echo "   File size should be ~23.8GB for schnell, ~23.8GB for dev"
        }

        # Download VAE (autoencoder)
        echo "Downloading VAE (ae.safetensors)..."
        curl -L -f -o "$MODEL_DIR/ae.safetensors" "$MODEL_URL_BASE/ae.safetensors" || {
            echo "❌ Failed to download ae.safetensors"
            echo "   This file should be ~335MB"
        }

        # Download text encoder 2 (T5) - this is what FLUX.1 uses
        echo "Downloading text encoder..."
        curl -L -f -o "$MODEL_DIR/text_encoder/model.safetensors" "$MODEL_URL_BASE/text_encoder_2/model.safetensors" || {
            echo "❌ Failed to download text encoder model"
        }
        curl -L -f -o "$MODEL_DIR/text_encoder/config.json" "$MODEL_URL_BASE/text_encoder_2/config.json" || {
            echo "⚠️  Failed to download text encoder config (optional)"
        }

        # Download tokenizer 2 (T5 tokenizer)
        mkdir -p "$MODEL_DIR/tokenizer"
        echo "Downloading tokenizer..."
        curl -L -f -o "$MODEL_DIR/tokenizer/tokenizer.json" "$MODEL_URL_BASE/tokenizer_2/tokenizer.json" || {
            echo "❌ Failed to download tokenizer.json"
        }
        curl -L -f -o "$MODEL_DIR/tokenizer/special_tokens_map.json" "$MODEL_URL_BASE/tokenizer_2/special_tokens_map.json" || {
            echo "⚠️  Failed to download special_tokens_map.json (optional)"
        }
        curl -L -f -o "$MODEL_DIR/tokenizer/tokenizer_config.json" "$MODEL_URL_BASE/tokenizer_2/tokenizer_config.json" || {
            echo "⚠️  Failed to download tokenizer_config.json (optional)"
        }

        # Download VAE config
        mkdir -p "$MODEL_DIR/vae"
        curl -L -f -o "$MODEL_DIR/vae/config.json" "$MODEL_URL_BASE/vae/config.json" || {
            echo "⚠️  Failed to download VAE config (optional)"
        }

        # Download transformer config
        mkdir -p "$MODEL_DIR/transformer"
        curl -L -f -o "$MODEL_DIR/transformer/config.json" "$MODEL_URL_BASE/transformer/config.json" || {
            echo "⚠️  Failed to download transformer config (optional)"
        }

        # Move files to expected locations
        if [ -f "$MODEL_DIR/ae.safetensors" ]; then
            mv "$MODEL_DIR/ae.safetensors" "$MODEL_DIR/vae/diffusion_pytorch_model.safetensors"
            echo "✅ VAE file moved to correct location"
        fi
        if [ -f "$MODEL_DIR/$MODEL_FILE" ]; then
            mv "$MODEL_DIR/$MODEL_FILE" "$MODEL_DIR/transformer/diffusion_pytorch_model.safetensors"
            echo "✅ Transformer file moved to correct location"
        fi
        
        # Verify downloads
        echo ""
        echo "📋 Download summary:"
        if [ -f "$MODEL_DIR/vae/diffusion_pytorch_model.safetensors" ]; then
            SIZE=$(du -h "$MODEL_DIR/vae/diffusion_pytorch_model.safetensors" | cut -f1)
            echo "   ✅ VAE: $SIZE"
        else
            echo "   ❌ VAE: Missing"
        fi
        if [ -f "$MODEL_DIR/transformer/diffusion_pytorch_model.safetensors" ]; then
            SIZE=$(du -h "$MODEL_DIR/transformer/diffusion_pytorch_model.safetensors" | cut -f1)
            echo "   ✅ Transformer: $SIZE"
        else
            echo "   ❌ Transformer: Missing"
        fi
        if [ -f "$MODEL_DIR/text_encoder/model.safetensors" ]; then
            SIZE=$(du -h "$MODEL_DIR/text_encoder/model.safetensors" | cut -f1)
            echo "   ✅ Text Encoder: $SIZE"
        else
            echo "   ❌ Text Encoder: Missing"
        fi
        ;;

    "flux2-dev")
        # FLUX.2 model structure (current implementation)
        "$SCRIPT_DIR/download_model.sh"
        if [ -d "flux-klein-model" ]; then
            mv flux-klein-model/* "$MODEL_DIR/" 2>/dev/null || true
            rmdir flux-klein-model 2>/dev/null || true
        fi
        ;;
esac

echo ""
echo "📋 Verifying downloads..."

# Check if critical files exist and have reasonable sizes
CRITICAL_FILES_OK=1
case "$MODEL_VARIANT" in
    "flux1-schnell"|"flux1-dev")
        if [ ! -f "$MODEL_DIR/vae/diffusion_pytorch_model.safetensors" ]; then
            echo "❌ Missing: vae/diffusion_pytorch_model.safetensors"
            CRITICAL_FILES_OK=0
        elif [ $(stat -f%z "$MODEL_DIR/vae/diffusion_pytorch_model.safetensors" 2>/dev/null || stat -c%s "$MODEL_DIR/vae/diffusion_pytorch_model.safetensors" 2>/dev/null || echo 0) -lt 1000000 ]; then
            echo "❌ VAE file too small (likely corrupted or error page)"
            CRITICAL_FILES_OK=0
        fi
        
        if [ ! -f "$MODEL_DIR/transformer/diffusion_pytorch_model.safetensors" ]; then
            echo "❌ Missing: transformer/diffusion_pytorch_model.safetensors"
            CRITICAL_FILES_OK=0
        elif [ $(stat -f%z "$MODEL_DIR/transformer/diffusion_pytorch_model.safetensors" 2>/dev/null || stat -c%s "$MODEL_DIR/transformer/diffusion_pytorch_model.safetensors" 2>/dev/null || echo 0) -lt 1000000000 ]; then
            echo "❌ Transformer file too small (likely corrupted or error page)"
            CRITICAL_FILES_OK=0
        fi
        
        if [ ! -f "$MODEL_DIR/text_encoder/model.safetensors" ]; then
            echo "❌ Missing: text_encoder/model.safetensors"
            CRITICAL_FILES_OK=0
        fi
        ;;
    "flux2-dev")
        # FLUX.2 verification handled by download_model.sh
        ;;
esac

if [ $CRITICAL_FILES_OK -eq 1 ]; then
    echo "✅ Download complete!"
    echo ""
    echo "🎉 FLUX $MODEL_VARIANT is ready to use!"
    echo "   In-game: /cl_flux_model $MODEL_VARIANT"
    echo "   Generate: flux_generate \"your prompt here\""
else
    echo ""
    echo "⚠️  Some files failed to download or are corrupted."
    echo "   This may be due to authentication requirements or network issues."
    echo ""
    echo "   Options:"
    echo "   1. Download files manually from alternative sources"
    echo "   2. See MANUAL_DOWNLOAD.md for detailed instructions"
    echo "   3. Check file sizes - they should be MB/GB, not bytes"
    echo ""
    echo "   Required file sizes:"
    case "$MODEL_VARIANT" in
        "flux1-schnell"|"flux1-dev")
            echo "   - VAE: ~335MB"
            echo "   - Transformer: ~23.8GB"
            echo "   - Text Encoder: ~4-5GB"
            ;;
    esac
fi