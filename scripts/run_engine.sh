#!/bin/bash
# Enhanced id Tech 3 Engine Launcher Script
# Provides intelligent renderer selection and optimal configuration

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_DIR="$(dirname "$SCRIPT_DIR")"
RELEASE_DIR="$ENGINE_DIR/release"
LOGS_DIR="$ENGINE_DIR/logs"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default settings
RENDERER="auto"
GPU_DEVICE=""
DEVELOPER_MODE=0
VALIDATION=0
PERF_HUD=0
FULLSCREEN=1
RESOLUTION="1920x1080"

# GPU Detection
detect_gpu() {
    if command -v nvidia-smi &> /dev/null; then
        echo "nvidia"
        return
    fi

    if command -v vulkaninfo &> /dev/null; then
        if vulkaninfo 2>/dev/null | grep -q "NVIDIA"; then
            echo "nvidia"
            return
        fi
    fi

    echo "integrated"
}

# Intelligent renderer selection
select_renderer() {
    local gpu_type=$(detect_gpu)

    case $RENDERER in
        "auto")
            if [ "$gpu_type" = "nvidia" ]; then
                echo "vulkan"
                echo -e "${GREEN}Auto-selected Vulkan renderer (NVIDIA GPU detected)${NC}"
            else
                echo "opengl"
                echo -e "${YELLOW}Auto-selected OpenGL renderer (integrated GPU or unknown)${NC}"
            fi
            ;;
        "vulkan"|"opengl")
            echo "$RENDERER"
            ;;
        *)
            echo "opengl"
            echo -e "${YELLOW}Unknown renderer '$RENDERER', defaulting to OpenGL${NC}"
            ;;
    esac
}

# Build command line arguments
build_args() {
    local renderer=$(select_renderer)
    local args="+set cl_renderer $renderer"

    # GPU device selection
    if [ -n "$GPU_DEVICE" ]; then
        export VK_LAYER_MESA_device_select="device=$GPU_DEVICE"
        echo -e "${BLUE}Forcing GPU device: $GPU_DEVICE${NC}"
    fi

    # Developer settings
    if [ $DEVELOPER_MODE -eq 1 ]; then
        args="$args +set developer 1"
        echo -e "${BLUE}Developer mode enabled${NC}"
    fi

    # Vulkan validation
    if [ $VALIDATION -eq 1 ]; then
        args="$args +set r_vk_enable_validation 1"
        echo -e "${BLUE}Vulkan validation enabled${NC}"
    fi

    # Performance HUD
    if [ $PERF_HUD -eq 1 ]; then
        args="$args +set r_perfhud 1"
        echo -e "${BLUE}Performance HUD enabled${NC}"
    fi

    # Display settings
    if [ $FULLSCREEN -eq 0 ]; then
        args="$args +set r_fullscreen 0"
    fi

    # Resolution
    if [ -n "$RESOLUTION" ]; then
        IFS='x' read -r width height <<< "$RESOLUTION"
        args="$args +set r_width $width +set r_height $height"
    fi

    echo "$args"
}

# Show usage information
show_usage() {
    cat << EOF
Enhanced id Tech 3 Engine Launcher

USAGE: $0 [OPTIONS] [QUAKE3_ARGS]

RENDERER OPTIONS:
    --vulkan          Force Vulkan renderer
    --opengl          Force OpenGL renderer
    --auto            Auto-select renderer (default)

GPU OPTIONS:
    --gpu=N           Force GPU device N (0=integrated, 1=discrete)
    --detect-gpu      Show detected GPU type and exit

DEBUG OPTIONS:
    --developer       Enable developer mode
    --validation      Enable Vulkan validation layers
    --perf-hud        Enable performance HUD
    --windowed        Run in windowed mode

EXAMPLES:
    $0 --auto                    # Auto-select best renderer
    $0 --vulkan --gpu=1         # Force Vulkan on discrete GPU
    $0 --opengl --developer     # OpenGL with developer tools
    $0 --validation --perf-hud  # Vulkan with full debugging

GAME LAUNCH:
    $0 +map q3dm9               # Launch specific map
    $0 +set fs_game mymod       # Launch with mod

EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --vulkan)
                RENDERER="vulkan"
                shift
                ;;
            --opengl)
                RENDERER="opengl"
                shift
                ;;
            --auto)
                RENDERER="auto"
                shift
                ;;
            --gpu=*)
                GPU_DEVICE="${1#*=}"
                shift
                ;;
            --detect-gpu)
                gpu_type=$(detect_gpu)
                echo -e "${GREEN}Detected GPU: $gpu_type${NC}"
                exit 0
                ;;
            --developer)
                DEVELOPER_MODE=1
                shift
                ;;
            --validation)
                VALIDATION=1
                shift
                ;;
            --perf-hud)
                PERF_HUD=1
                shift
                ;;
            --windowed)
                FULLSCREEN=0
                shift
                ;;
            --resolution=*)
                RESOLUTION="${1#*=}"
                shift
                ;;
            --help|-h)
                show_usage
                exit 0
                ;;
            *)
                # Pass through to engine
                break
                ;;
        esac
    done

    # Remaining args go to engine
    ENGINE_ARGS="$*"
}

# Main execution
main() {
    # Create logs directory
    mkdir -p "$LOGS_DIR"

    # Parse arguments
    parse_args "$@"

    # Check if engine exists
    if [ ! -f "$RELEASE_DIR/idtech3.x86_64" ]; then
        echo -e "${RED}Error: Engine binary not found at $RELEASE_DIR/idtech3.x86_64${NC}"
        echo -e "${YELLOW}Please build the engine first with: make${NC}"
        exit 1
    fi

    # Build command line
    local args=$(build_args)

    # Add any additional engine arguments
    if [ -n "$ENGINE_ARGS" ]; then
        args="$args $ENGINE_ARGS"
    fi

    echo -e "${GREEN}Launching id Tech 3 Engine...${NC}"
    echo -e "${BLUE}Command: $RELEASE_DIR/idtech3.x86_64 $args${NC}"

    # Launch engine with crash recovery for Vulkan
    cd "$ENGINE_DIR"

    # Launch engine with automatic crash recovery
    cd "$ENGINE_DIR"

    # Check if we're trying Vulkan renderer
    if echo "$args" | grep -q "cl_renderer vulkan"; then
        echo -e "${YELLOW}Vulkan renderer selected - attempting to run...${NC}"

        # Try Vulkan first
        "$RELEASE_DIR/idtech3.x86_64" $args 2>&1 &
        engine_pid=$!

        # Wait a bit to see if it crashes
        sleep 3

        # Check if process is still running
        if kill -0 $engine_pid 2>/dev/null; then
            # Vulkan is running successfully
            echo -e "${GREEN}Vulkan renderer started successfully${NC}"
            wait $engine_pid
            exit $?
        else
            # Vulkan crashed, try OpenGL
            echo -e "${RED}Vulkan renderer crashed, automatically switching to OpenGL${NC}"
            wait $engine_pid 2>/dev/null || true

            # Replace vulkan with opengl in args
            args=$(echo "$args" | sed 's/cl_renderer vulkan/cl_renderer opengl/g')
            echo -e "${GREEN}Starting OpenGL renderer...${NC}"
            exec "$RELEASE_DIR/idtech3.x86_64" $args
        fi
    else
        # Not Vulkan, run normally
        exec "$RELEASE_DIR/idtech3.x86_64" $args
    fi
}

# Run main function
main "$@"

