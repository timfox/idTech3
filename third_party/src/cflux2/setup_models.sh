#!/bin/bash
# FLUX Model Setup Script for idTech3
# Run this from your game's base directory (e.g., base/)

set -e

echo "=== FLUX Model Setup for idTech3 ==="
echo "Supports FLUX.1-schnell, FLUX.1-dev, and FLUX.2-dev models"
echo ""

# Check if we're in a release/flux directory
if [ ! -f "../../idtech3" ] && [ ! -f "../../quake3" ] && [ ! -f "../../ioquake3" ]; then
    echo "⚠️  Warning: Could not find idTech3 executable in grandparent directory"
    echo "   Make sure you're running this from your release/flux directory"
    echo "   Example: cd /path/to/idtech3/release/flux && /path/to/idtech3/src/external/src/cflux2/setup_models.sh"
    echo ""
fi

# Check if models already exist
if [ -d "text_encoder" ] && [ -d "tokenizer" ] && [ -d "transformer" ] && [ -d "vae" ]; then
    echo "✅ FLUX model directories found in current directory"
    echo "   Checking contents..."

    # Check for key files
    missing_files=()
    [ ! -f "vae/diffusion_pytorch_model.safetensors" ] && missing_files+=("VAE model")
    [ ! -f "transformer/diffusion_pytorch_model.safetensors" ] && missing_files+=("Transformer model")
    [ ! -f "text_encoder/model-00001-of-00002.safetensors" ] && missing_files+=("Text encoder model")

    if [ ${#missing_files[@]} -eq 0 ]; then
        echo "✅ All required model files found!"
        echo ""
        echo "🎉 FLUX is ready to use!"
        echo "   In-game: /cl_flux_enable 1"
        echo "   Generate: flux_generate \"your prompt here\""
        exit 0
    else
        echo "❌ Missing files: ${missing_files[*]}"
        echo "   The download may not be complete."
        echo ""
    fi
fi

echo "📥 Starting FLUX model download..."
echo "   This will download ~16GB of model files to: flux-klein-model/"
echo "   This may take a while depending on your internet connection."
echo ""

# Run the download script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$SCRIPT_DIR/download_model.sh"

echo ""
echo "✅ Model download complete!"
echo ""
echo "🎮 To use FLUX in idTech3:"
echo "   1. Launch the game"
echo "   2. Enable FLUX: /cl_flux_enable 1"
echo "   3. Generate images: flux_generate \"a beautiful landscape\""
echo ""
echo "📚 For more options, see the console commands:"
echo "   - flux_reload: Reload textures"
echo "   - flux_show: Register shaders"
echo "   - flux_view: Complete workflow"