#!/bin/bash

# Build and reload QVM script for rapid development
# Usage: ./tools/build_and_reload_qvm.sh [vm_name]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Default to building all VMs if no specific VM requested
TARGET_VM="$1"

echo "Building QVMs..."

# Change to project root
cd "$PROJECT_ROOT"

# Build QVMs (adjust this command based on your build system)
if [ -n "$TARGET_VM" ]; then
    echo "Building specific VM: $TARGET_VM"
    # Add specific VM build command here
    # Example: make qvm_$TARGET_VM
    echo "TODO: Implement specific VM building for $TARGET_VM"
else
    echo "Building all VMs..."
    # Add your QVM build command here
    # Example: make qvms
    echo "TODO: Implement full QVM build process"
fi

# Copy QVMs to game directory (adjust paths as needed)
GAME_DIR="${GAME_DIR:-$HOME/.ioquake3/baseq3}"
VM_DIR="$GAME_DIR/vm"

echo "Deploying QVMs to: $VM_DIR"

# Create VM directory if it doesn't exist
mkdir -p "$VM_DIR"

# Copy built QVMs (adjust source paths)
if [ -n "$TARGET_VM" ]; then
    # Copy specific VM
    if [ -f "build/vm/$TARGET_VM.qvm" ]; then
        cp "build/vm/$TARGET_VM.qvm" "$VM_DIR/"
        echo "Deployed: $TARGET_VM.qvm"
    else
        echo "Warning: $TARGET_VM.qvm not found in build directory"
    fi
else
    # Copy all QVMs
    if [ -d "build/vm" ]; then
        cp build/vm/*.qvm "$VM_DIR/" 2>/dev/null || echo "No QVMs found in build/vm/"
    else
        echo "Warning: build/vm directory not found"
    fi
fi

echo "QVM deployment complete."
echo ""
echo "Hot reload instructions:"
echo "1. Ensure vm_hotReload is enabled (set vm_hotReload 1)"
echo "2. Start or restart your game"
echo "3. QVM changes will be automatically detected and reloaded"
echo ""
echo "Manual reload commands:"
echo "  vm_reload $TARGET_VM    # Reload specific VM"
echo "  vm_reload_all          # Reload all VMs"
echo "  vm_hotreload_status    # Show reload status"