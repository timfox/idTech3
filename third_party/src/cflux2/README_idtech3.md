# FLUX.2 Image Generation Integration for idTech3

This document describes the integration of FLUX.2 image generation capabilities into the idTech3 engine.

## Overview

FLUX.2 is a state-of-the-art text-to-image generation model that has been integrated as an optional feature in idTech3. This allows Quake III Arena and compatible games to generate images from text prompts directly within the game console.

## Features

- **Text-to-Image Generation**: Generate images from text prompts
- **Configurable Parameters**: Control image size, quality, and randomness
- **Fallback Support**: Gracefully degrades when dependencies are missing
- **Memory Efficient**: Uses mmap for large model files
- **Console Integration**: Access via console commands

## Building

### Prerequisites

- CMake 3.24+
- GCC 15+ or Clang 18+
- Optional: BLAS library (OpenBLAS on Linux, Accelerate on macOS) for acceleration
- Optional: Metal framework (macOS Apple Silicon only) for GPU acceleration

### Build Options

```bash
# Enable FLUX support (default: ON)
cmake .. -DUSE_FLUX=ON

# Disable FLUX support
cmake .. -DUSE_FLUX=OFF
```

### Backend Selection

The build system automatically selects the best available backend:

1. **Metal (macOS Apple Silicon)**: Fastest, uses Metal Performance Shaders
2. **BLAS**: Accelerated CPU computation
3. **Generic**: Pure C fallback (slowest but always available)

## Configuration Variables (CVARs)

### Core Settings

- `cl_flux_enable` (default: 0)
  - Enable/disable FLUX image generation
  - Set to 1 to enable, 0 to disable

- `cl_flux_async` (default: 0)
  - FLUX generation mode: 0=synchronous (blocking), 1=asynchronous (background)
  - **Note**: In-process generation can crash due to GPU resource conflicts with idTech3's Vulkan renderer
  - **Stability**: Use external generation mode for best stability

- `cl_flux_external` (default: 1)
  - Run FLUX in an external `flux_cli` process to avoid in-process crashes
  - Recommended for stability; keeps the engine alive if generation fails

- `cl_flux_device` (default: "auto")
  - Compute device selection: auto (best available), cpu (force CPU), gpu (force GPU), gpu:N (specific GPU)
  - **Note**: Runtime device selection requires FLUX library support (currently compile-time only)

- `cl_flux_model` (default: "flux1-schnell")
  - FLUX model variant: flux1-schnell (fast), flux1-dev (balanced), flux2-dev (high quality)
  - Note: FLUX.1 requires T5 encoder files; see manual download guide for required files

### Generation Parameters

- `cl_flux_width` (default: 256, range: 64-1792)
  - Width of generated images in pixels
  - Must be multiple of 16

- `cl_flux_height` (default: 256, range: 64-1792)
  - Height of generated images in pixels
  - Must be multiple of 16

- `cl_flux_steps` (default: 2, range: 1-50)
  - Number of denoising steps
  - Higher values = better quality but slower
  - 2 steps = fast (~15-30s), 4 steps = balanced (~30-60s), 8+ steps = high quality (2-5+ minutes)

- `cl_flux_seed` (default: -1, range: -1 to 2147483647)
  - Random seed for reproducible generation
  - -1 for random seed

## Usage

### Setup

1. Download the FLUX.2-klein model files to your release directory:
   ```bash
   cd /path/to/idtech3/release/flux  # Engine flux directory
   # Run the download script (downloads ~16GB directly to flux/)
   bash /path/to/idtech3/src/external/src/cflux2/download_model.sh
   ```

   Or manually copy from source:
   ```bash
   cd /path/to/idtech3/release
   cp -r /path/to/idtech3/src/external/src/cflux2/flux-klein-model ./flux/
   ```

2. Enable FLUX in the game:
   ```
   /cl_flux_enable 1
   ```

3. Configure model path if needed:
   ```
   /cl_flux_model "flux1-schnell"
   ```

### Generating Images

Use the console command to generate images:

```
flux_generate "A beautiful landscape at sunset"
/flux_generate "A cyberpunk city with neon lights"
/flux_generate "A fantasy castle on a mountain"
```

### Output

- Images are saved as PNG files in the game's base directory
- Filenames include timestamp and dimensions: `flux_[timestamp]_[width]x[height].png`
- Success/failure messages are displayed in the console
- Seed value is shown for reproducible generation
- **Real-time Hot Reload**: Images are automatically reloaded into the renderer!

### Examples

**Basic Generation:**
```
flux_generate "a red sports car on a beach"
```

**High Quality:**
```
/cl_flux_steps 8
/cl_flux_width 512
/cl_flux_height 512
flux_generate "a majestic eagle flying over mountains"
```

**Reproducible:**
```
/cl_flux_seed 12345
flux_generate "a steampunk robot"
```

### Additional Commands

**Show Device Information:**
```
flux_devices
```
Display current device settings, available backends, and device selection options.

**Reload Textures:**
```
flux_reload flux_123456_256x256.png
```
Manually reload a specific texture from disk.

**Register as Shader:**
```
flux_show flux_123456_256x256.png
```
Register a generated image as a shader for use in menus/materials.

**View Generated Image (Complete Workflow):**
```
flux_view flux_123456_256x256.png
```
Reload texture AND register as shader - the complete viewing workflow!

**Automatic Real-time Reload:**
- Images are automatically hot-reloaded after generation!
- No need for `vid_restart` - see your images instantly
- If hot reload fails, individual commands are available as fallback
- Use `imagelist` to see all loaded textures

## Model Requirements

### FLUX.2-klein-4B Model Files

**Location**: Place the `flux-klein-model` directory in your game's base directory (e.g., `baseq3/flux-klein-model/`)

The following files are required in the model directory:

- `vae/diffusion_pytorch_model.safetensors` - VAE weights
- `transformer/diffusion_pytorch_model.safetensors` - Transformer weights
- `text_encoder/model.safetensors` - Text encoder weights
- `tokenizer/` - Tokenizer files

### Memory Requirements

- **Minimum**: 8GB RAM (with mmap enabled)
- **Recommended**: 16GB+ RAM for optimal performance
- **Model Size**: ~16GB on disk

## Performance

### Typical Generation Times

- **256x256**: 5-10 seconds
- **512x512**: 15-30 seconds
- **1024x1024**: 60+ seconds

Times vary based on hardware and backend used.

### Hardware Acceleration

- **Apple Silicon**: Metal Performance Shaders (fastest)
- **Intel/AMD CPUs**: BLAS/OpenBLAS acceleration
- **Generic**: Pure C fallback

## Troubleshooting

### Common Issues

**"FLUX image generation is disabled"**
- Set `cl_flux_enable 1`

**"Failed to load model"**
- Verify model files exist in the correct directory
- Check `cl_flux_model` setting (flux1-schnell, flux1-dev, flux2-dev)
- Ensure sufficient disk space

**"Generation failed"**
- Check console for specific error messages
- Verify image dimensions are multiples of 16
- Try different prompts

**Slow Generation**
- Reduce image size or steps
- Ensure BLAS/Metal acceleration is available
- Check available RAM

### Error Messages

The integration provides detailed error messages:
- Model loading failures
- Invalid parameter values
- Memory allocation issues
- File I/O errors

## Architecture

### Integration Points

- **Build System**: CMake integration with optional compilation
- **Console Commands**: `flux_generate` command registration
- **CVAR System**: Parameter configuration
- **Logging**: Startup and runtime status messages

### Code Organization

- `src/external/src/cflux2/`: FLUX library source
- `src/client/cl_main.c`: Console command and CVAR integration
- `CMakeLists.txt`: Build system integration

### Safety Features

- **Conditional Compilation**: `#ifdef USE_FLUX` guards
- **Fallback Support**: Graceful degradation when disabled
- **Input Validation**: Parameter range checking
- **Error Recovery**: Proper cleanup on failures

## Technical Details

### Backend Selection Logic

The build system automatically selects the optimal backend:

```cmake
# Apple Silicon with Metal support
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    # Use Metal
elseif(BLAS_FOUND)
    # Use BLAS
else()
    # Use generic C
endif()
```

### Memory Management

- **mmap Support**: Memory-mapped model loading (default)
- **Automatic Cleanup**: Text encoder released after generation
- **Peak Memory**: ~4-5GB with mmap, ~16GB without

### Thread Safety

- Single-threaded generation (matches cflux2 design)
- Non-blocking console interface
- Safe CVAR access

## Future Enhancements

## Known Issues & Compatibility

### Critical Stability Issues
- **Process Crashes**: FLUX integration causes idTech3 to crash in both synchronous and asynchronous modes
- **GPU Conflicts**: FLUX cannot safely coexist with idTech3's Vulkan renderer in the same process
- **Memory Corruption**: FLUX's memory management conflicts with idTech3's allocation systems
- **Experimental Status**: This integration is currently unstable and may cause system crashes

### Model Support Status
- **FLUX.2 Support**: ✅ Full support for FLUX.2-klein-4B (flux2-dev) with Qwen3 text encoding
- **FLUX.1 Support**: ✅ Full T5 encoder implementation for FLUX.1-schnell and FLUX.1-dev
- **Architecture Detection**: ✅ Automatic detection of FLUX.1 vs FLUX.2 models
- **Transformer Compatibility**: ✅ Supports both FLUX.1 (4096 text dim) and FLUX.2 (7680 text dim)
- **Directory Structure**: ✅ Each model variant can have its own directory

### FLUX.1 Model Downloads

FLUX.1 models can be obtained from various sources. Here are options:

#### Option 1: Manual Download (Any Source)
Download FLUX.1 model files from any source and place them in the correct directory structure:

**Required Files:**
- `ae.safetensors` (~335MB) → Place as `vae/diffusion_pytorch_model.safetensors`
- `flux1-schnell.safetensors` (~23.8GB) → Place as `transformer/diffusion_pytorch_model.safetensors`
- `text_encoder_2/model.safetensors` → Place as `text_encoder/model.safetensors`
- `text_encoder_2/config.json` → Place as `text_encoder/config.json`
- `tokenizer_2/tokenizer.json` → Place as `tokenizer/tokenizer.json`
- `tokenizer_2/special_tokens_map.json` → Place as `tokenizer/special_tokens_map.json`
- `tokenizer_2/tokenizer_config.json` → Place as `tokenizer/tokenizer_config.json`
- `vae/config.json` → Place as `vae/config.json`
- `transformer/config.json` → Place as `transformer/config.json`

#### Option 2: Using Hugging Face CLI (if you have access)
```bash
pip install huggingface_hub
huggingface-cli login
huggingface-cli download black-forest-labs/FLUX.1-schnell \
    --local-dir /path/to/idtech3/release/flux1-schnell \
    --local-dir-use-symlinks False
```

#### Option 3: Alternative Sources
- **NVIDIA NIM**: Containerized inference service
- **ComfyUI**: Node-based workflow for local inference
- **Direct mirrors**: Some communities provide direct download links
- **Torrent/magnet links**: Check FLUX community resources

#### Directory Structure
For FLUX.1 models, ensure files are in this structure:
```
flux1-schnell/
├── ae.safetensors                          → vae/diffusion_pytorch_model.safetensors
├── flux1-schnell.safetensors              → transformer/diffusion_pytorch_model.safetensors
├── text_encoder_2/
│   ├── model.safetensors                  → text_encoder/model.safetensors
│   └── config.json                        → text_encoder/config.json
├── tokenizer_2/
│   ├── tokenizer.json                     → tokenizer/tokenizer.json
│   ├── special_tokens_map.json            → tokenizer/special_tokens_map.json
│   └── tokenizer_config.json              → tokenizer/tokenizer_config.json
├── vae/config.json                        → vae/config.json
└── transformer/config.json                → transformer/config.json
```

### Workarounds
- **External Process**: Run FLUX in a separate process and communicate via files/network
- **Standalone Tool**: Use FLUX as a separate command-line tool outside of idTech3
- **Alternative Models**: Consider lighter-weight AI models more suitable for game integration

### Performance Considerations
- **Generation Time**: 30-120+ seconds depending on hardware and settings
- **Memory Usage**: ~16GB RAM required for model loading
- **GPU Memory**: Significant VRAM usage during generation
- **System Stability**: May cause system-wide instability during generation

## Implemented Features

### Architecture Support
- ✅ **FLUX.2 Full Support**: Complete FLUX.2-klein-4B implementation with Qwen3 text encoding
- ✅ **FLUX.1 Detection**: Automatic detection of FLUX.1-schnell and FLUX.1-dev models
- ✅ **Model Directory Structure**: Support for separate directories per model variant
- ⚠️ **FLUX.1 Encoding**: Uses dummy embeddings until T5 encoder implementation

### Model Variants
- ✅ **flux1-schnell**: Fast distilled model (2 steps default, directory: flux1-schnell)
- ✅ **flux1-dev**: Balanced model (4 steps default, directory: flux1-dev)
- ✅ **flux2-dev**: High quality model (4 steps default, directory: flux2-dev)

## Implementation Details

### T5 Encoder Architecture
- **Layers**: 24 encoder-only transformer layers
- **Hidden Size**: 4096 dimensions
- **Attention**: 32 heads × 128 dimensions per head
- **Feed-Forward**: 10240 intermediate dimensions with GELU activation
- **Normalization**: Layer normalization (not RMS norm)
- **Position Embeddings**: Relative position bias support

### Transformer Compatibility
- **Text Dimension Detection**: Automatically detects text embedding dimension from model weights
- **FLUX.1**: Uses 4096-dim text embeddings from T5 encoder
- **FLUX.2**: Uses 7680-dim text embeddings from Qwen3 encoder
- **Dynamic Adaptation**: Transformer adapts to detected text dimension

## Future Improvements

- **FLUX.1 Transformer Differences**: Further optimization for FLUX.1-specific transformer architectures
- **Sampling Schedules**: FLUX.1-specific sampling schedule optimization
- Image-to-image generation
- Multiple reference image support
- Interactive generation with progress feedback
- Batch processing capabilities

## License

FLUX integration inherits the engine's GPL license. The underlying cflux2 library uses MIT license.