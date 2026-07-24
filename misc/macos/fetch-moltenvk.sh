#!/bin/bash
# Fetch MoltenVK and stage it as code/libvulkan/macosx/libvulkan.1.dylib.
#
# Why rename to libvulkan.1.dylib: SDL2's SDL_Vulkan_LoadLibrary(NULL)
# searches that name first on macOS, and MoltenVK can act as a drop-in
# loader replacement (no separate Khronos loader or ICD JSON needed,
# because code/renderervk/vk.c already sets
# VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR for MoltenVK).
#
# Why install_name @rpath/libvulkan.1.dylib: the Surf / idtech3 binary
# carries LC_RPATH=@executable_path (darwin Makefile / CMake), so any
# @rpath-named dylib next to it in Surf.app/Contents/MacOS/ resolves
# without DYLD_LIBRARY_PATH or a system-wide Vulkan install.
#
# Override MOLTENVK_VERSION env var to pin a different release.

set -euo pipefail

VERSION="${MOLTENVK_VERSION:-v1.4.1}"
DEST_DIR="code/libvulkan/macosx"
DEST_FILE="${DEST_DIR}/libvulkan.1.dylib"

# Run from repo root regardless of where the script is invoked.
cd "$(dirname "$0")/../.."

if [ -f "${DEST_FILE}" ]; then
    echo "MoltenVK already present at ${DEST_FILE} (delete to refetch)"
    exit 0
fi

URL="https://github.com/KhronosGroup/MoltenVK/releases/download/${VERSION}/MoltenVK-macos.tar"
TMPDIR=$(mktemp -d)
trap 'rm -rf "${TMPDIR}"' EXIT

echo "Fetching MoltenVK ${VERSION} from ${URL}"
curl -fL --progress-bar -o "${TMPDIR}/MoltenVK-macos.tar" "${URL}"

# Extract the universal (arm64 + x86_64) dylib. MoltenVK 1.4.x reorganized
# the layout from MoltenVK.xcframework/macos-arm64_x86_64/ to a flatter
# MoltenVK/dynamic/dylib/macOS/. The dylib remains universal.
DYLIB_IN_TAR="MoltenVK/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib"
echo "Extracting ${DYLIB_IN_TAR}"
tar -xf "${TMPDIR}/MoltenVK-macos.tar" -C "${TMPDIR}" "${DYLIB_IN_TAR}"

mkdir -p "${DEST_DIR}"
cp "${TMPDIR}/${DYLIB_IN_TAR}" "${DEST_FILE}"

echo "Rewriting install_name to @rpath/libvulkan.1.dylib"
install_name_tool -id "@rpath/libvulkan.1.dylib" "${DEST_FILE}"

echo "Staged ${DEST_FILE}:"
ls -lh "${DEST_FILE}"
file "${DEST_FILE}"
