#!/bin/bash

# Build and reload QVM script for rapid development
# Supports both native shared libraries (.so/.dll) and legacy QVM bytecode
# Usage: ./scripts/build_and_reload_qvm.sh [vm_name] [mod_name] [--qvm]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Parse arguments
TARGET_VM=""
MOD_NAME="mymod"
BUILD_QVM=0

for arg in "$@"; do
    case "$arg" in
        --qvm)
            BUILD_QVM=1
            ;;
        --help|-h)
            echo "Usage: $0 [vm_name] [mod_name] [--qvm]"
            echo ""
            echo "Arguments:"
            echo "  vm_name    Specific VM to build (game, cgame, ui) - optional"
            echo "  mod_name   Mod directory name (default: mymod)"
            echo "  --qvm      Build QVM bytecode instead of native libraries"
            echo ""
            echo "Examples:"
            echo "  $0                    # Build all native VMs for mymod"
            echo "  $0 game mymod         # Build game VM for mymod"
            echo "  $0 cgame mymod --qvm  # Build cgame QVM for mymod"
            exit 0
            ;;
        game|cgame|ui)
            TARGET_VM="$arg"
            ;;
        *)
            if [ -z "$TARGET_VM" ] && [ "$MOD_NAME" = "mymod" ]; then
                # First non-flag argument could be mod name
                if [ -d "$PROJECT_ROOT/mods/$arg" ]; then
                    MOD_NAME="$arg"
                fi
            fi
            ;;
    esac
done

MOD_ROOT="$PROJECT_ROOT/mods/$MOD_NAME"
MOD_SOURCE_DIR="$MOD_ROOT/gamesrc"
MOD_VM_DIR="$MOD_ROOT/vm"
BUILD_DIR="$MOD_SOURCE_DIR/build"

echo "Building VMs for mod: $MOD_NAME"
echo "Target VM: ${TARGET_VM:-all}"
echo "Build type: $([ $BUILD_QVM -eq 1 ] && echo 'QVM bytecode' || echo 'Native libraries')"
echo

# Check if gamesrc exists
if [ ! -d "$MOD_SOURCE_DIR" ]; then
    echo "Error: Game source directory not found: $MOD_SOURCE_DIR"
    echo "Available mods:"
    ls -1 "$PROJECT_ROOT/mods" 2>/dev/null | grep -v "^\.$" || echo "  (none)"
    exit 1
fi

# Build QVM bytecode (legacy)
if [ $BUILD_QVM -eq 1 ]; then
    echo "Building QVM bytecode (legacy method)..."
    echo "Note: QVM compilation requires q3lcc and q3asm tools"
    
    if ! command -v q3lcc >/dev/null 2>&1 && ! command -v q3asm >/dev/null 2>&1; then
        echo "Error: q3lcc and q3asm not found. Install QVM compiler tools."
        echo "For native compilation (recommended), omit --qvm flag."
        exit 1
    fi
    
    # QVM build would go here - this is a placeholder
    echo "QVM compilation not fully implemented. Use native compilation instead."
    echo "Run: $0 $TARGET_VM $MOD_NAME  # (without --qvm flag)"
    exit 1
fi

# Build native shared libraries (recommended)
echo "Building native shared libraries..."

# Use compile_game.sh if available
if [ -f "$PROJECT_ROOT/scripts/compile_game.sh" ]; then
    echo "Using compile_game.sh..."
    "$PROJECT_ROOT/scripts/compile_game.sh" "$MOD_NAME" Release
    
    # Check for built libraries
    if [ -n "$TARGET_VM" ]; then
        VM_FILES=("$MOD_VM_DIR/${TARGET_VM}.x86_64.so" "$MOD_VM_DIR/${TARGET_VM}x86_64.so")
        for vm_file in "${VM_FILES[@]}"; do
            if [ -f "$vm_file" ]; then
                echo "✓ Built: $vm_file"
                break
            fi
        done
    else
        VM_COUNT=$(find "$MOD_VM_DIR" -name "*.so" -o -name "*.dll" 2>/dev/null | wc -l)
        echo "✓ Built $VM_COUNT VM file(s) in $MOD_VM_DIR"
    fi
else
    # Fallback: try CMake build directly
    if [ -f "$MOD_SOURCE_DIR/CMakeLists.txt" ]; then
        echo "Building with CMake..."
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake -DCMAKE_BUILD_TYPE=Release ..
        
        if [ -n "$TARGET_VM" ]; then
            cmake --build . --target "$TARGET_VM" -- -j$(nproc 2>/dev/null || echo 4)
        else
            cmake --build . -- -j$(nproc 2>/dev/null || echo 4)
        fi
    else
        echo "Error: No build system found. Expected CMakeLists.txt in $MOD_SOURCE_DIR"
        exit 1
    fi
fi

# Copy to runtime directory if specified
GAME_DIR="${GAME_DIR:-$HOME/.$MOD_NAME}"
RUNTIME_VM_DIR="$GAME_DIR/vm"

if [ -n "${GAME_DIR:-}" ] && [ "$GAME_DIR" != "$HOME/.ioquake3/baseq3" ]; then
    echo "Deploying to: $RUNTIME_VM_DIR"
    mkdir -p "$RUNTIME_VM_DIR"
    
    if [ -n "$TARGET_VM" ]; then
        for vm_file in "$MOD_VM_DIR/${TARGET_VM}"*.so "$MOD_VM_DIR/${TARGET_VM}"*.dll; do
            if [ -f "$vm_file" ]; then
                cp "$vm_file" "$RUNTIME_VM_DIR/"
                echo "Deployed: $(basename "$vm_file")"
            fi
        done
    else
        cp "$MOD_VM_DIR"/*.so "$MOD_VM_DIR"/*.dll "$RUNTIME_VM_DIR/" 2>/dev/null || true
        echo "Deployed all VM files"
    fi
fi

echo ""
echo "Build complete!"
echo ""
echo "Hot reload instructions:"
echo "1. Enable native VM loading (if using native libraries):"
echo "   set vm_game 0"
echo "   set vm_cgame 0"
echo "   set vm_ui 0"
echo ""
echo "2. Enable hot reload:"
echo "   set vm_hotReload 1"
echo ""
echo "3. Start or restart your game"
echo "   VM changes will be automatically detected and reloaded"
echo ""
echo "Manual reload commands (in-game):"
if [ -n "$TARGET_VM" ]; then
    echo "  vm_reload $TARGET_VM    # Reload specific VM"
else
    echo "  vm_reload game         # Reload game VM"
    echo "  vm_reload cgame        # Reload cgame VM"
    echo "  vm_reload ui           # Reload ui VM"
fi
echo "  vm_reload_all          # Reload all VMs"
echo "  vm_hotreload_status    # Show reload status"