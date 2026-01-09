#!/bin/bash
# idTech3 NVIDIA GPU launcher script
# Forces the use of NVIDIA GPU instead of integrated graphics

export DRI_PRIME=1
export __NV_PRIME_RENDER_OFFLOAD=1
export __GLX_VENDOR_LIBRARY_NAME=nvidia
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.json

# Change to the parent directory (project root)
cd "$(dirname "$0")/.."

# Launch the engine with proper paths
exec ./release/idtech3.x86_64 +set fs_basepath "$(pwd)" "$@"