#!/bin/bash
# Engine Performance Benchmarking Script
# Measures FPS, GPU usage, and rendering performance

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_DIR="$(dirname "$SCRIPT_DIR")"
RELEASE_DIR="$ENGINE_DIR/release"
LOGS_DIR="$ENGINE_DIR/logs"
BENCHMARK_DIR="$LOGS_DIR/benchmarks"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Benchmark settings
DURATION=30
MAP="q3dm9"
RESOLUTION="1920x1080"

# GPU monitoring
monitor_gpu() {
    local duration="$1"
    local log_file="$2"

    # Try different GPU monitoring tools
    if command -v nvidia-smi &> /dev/null; then
        echo "Using NVIDIA monitoring..."
        timeout "$duration" nvidia-smi --query-gpu=temperature.gpu,utilization.gpu,utilization.memory,power.draw --format=csv,noheader,nounits -l 1 > "$log_file" &
    elif command -v intel-gpu-top &> /dev/null; then
        echo "Using Intel GPU monitoring..."
        timeout "$duration" intel-gpu-top -l > "$log_file" &
    else
        echo "GPU monitoring not available"
        touch "$log_file"
    fi
}

# Calculate averages from GPU log
calculate_gpu_stats() {
    local log_file="$1"

    if [ ! -s "$log_file" ]; then
        echo "No GPU data available"
        return
    fi

    if command -v nvidia-smi &> /dev/null; then
        # Parse NVIDIA data
        awk -F', ' '
        NR > 1 {
            temp += $1
            gpu_util += $2
            mem_util += $3
            power += $4
            count++
        }
        END {
            if (count > 0) {
                printf "GPU Temperature: %.1f°C\n", temp/count
                printf "GPU Utilization: %.1f%%\n", gpu_util/count
                printf "Memory Utilization: %.1f%%\n", mem_util/count
                printf "Power Draw: %.1fW\n", power/count
            }
        }
        ' "$log_file"
    else
        echo "GPU stats calculation not supported for this GPU"
    fi
}

# Run benchmark
run_benchmark() {
    local renderer="$1"
    local test_name="${renderer}_benchmark"
    local benchmark_log="$BENCHMARK_DIR/${test_name}_$(date +%Y%m%d_%H%M%S).log"
    local gpu_log="$BENCHMARK_DIR/${test_name}_gpu_$(date +%Y%m%d_%H%M%S).log"

    echo -e "${BLUE}Running benchmark: $test_name${NC}"

    # Create benchmark directory
    mkdir -p "$BENCHMARK_DIR"

    # Start GPU monitoring
    monitor_gpu "$DURATION" "$gpu_log"
    local monitor_pid=$!

    # Run engine benchmark
    local start_time=$(date +%s)
    timeout "$DURATION" "$RELEASE_DIR/idtech3.x86_64" \
        +set cl_renderer "$renderer" \
        +set r_width "${RESOLUTION%x*}" \
        +set r_height "${RESOLUTION#*x}" \
        +set r_fullscreen 0 \
        +set r_perfhud 1 \
        +set developer 1 \
        +timedemo 1 \
        +set demodone "quit" \
        +demo demo001 \
        > "$benchmark_log" 2>&1 || true

    local end_time=$(date +%s)
    local actual_duration=$((end_time - start_time))

    # Wait for GPU monitoring to finish
    wait $monitor_pid 2>/dev/null || true

    # Extract performance data
    echo -e "${GREEN}Benchmark Results: $test_name${NC}"
    echo "Duration: ${actual_duration}s"
    echo "Resolution: $RESOLUTION"
    echo "Renderer: $renderer"
    echo ""

    # Extract FPS from log
    if grep -q "frames.*seconds" "$benchmark_log"; then
        grep "frames.*seconds" "$benchmark_log" | tail -1
    fi

    # Show GPU stats
    echo ""
    echo "GPU Statistics:"
    calculate_gpu_stats "$gpu_log"

    # Save results
    {
        echo "=== Benchmark Results: $test_name ==="
        echo "Date: $(date)"
        echo "Duration: ${actual_duration}s"
        echo "Resolution: $RESOLUTION"
        echo "Renderer: $renderer"
        echo ""
        echo "Performance Data:"
        grep -E "(frames.*seconds|fps|FPS)" "$benchmark_log" | tail -5 || echo "No FPS data found"
        echo ""
        echo "GPU Statistics:"
        calculate_gpu_stats "$gpu_log"
        echo ""
        echo "Full log: $benchmark_log"
        echo "GPU log: $gpu_log"
        echo "======================================="
        echo ""
    } >> "$BENCHMARK_DIR/benchmark_results.txt"

    echo -e "${GREEN}Results saved to: $BENCHMARK_DIR/benchmark_results.txt${NC}"
}

# Compare renderers
compare_renderers() {
    echo -e "${BLUE}Comparing Vulkan vs OpenGL Performance${NC}"
    echo ""

    run_benchmark "vulkan"
    echo ""
    sleep 2
    run_benchmark "opengl"

    echo ""
    echo -e "${GREEN}Comparison complete. Check $BENCHMARK_DIR/benchmark_results.txt${NC}"
}

# System information
show_system_info() {
    echo -e "${BLUE}System Information:${NC}"
    echo "OS: $(uname -a)"
    echo "CPU: $(nproc) cores - $(grep "model name" /proc/cpuinfo | head -1 | cut -d: -f2 | sed 's/^ *//')"
    echo "Memory: $(free -h | grep '^Mem:' | awk '{print $2}')"
    echo "GPU: $(./scripts/run_engine.sh --detect-gpu 2>/dev/null || echo "Unknown")"

    if command -v glxinfo &> /dev/null; then
        echo "OpenGL: $(glxinfo 2>/dev/null | grep "OpenGL version" | head -1 | cut -d: -f2 | sed 's/^ *//')"
    fi

    if command -v vulkaninfo &> /dev/null; then
        echo "Vulkan: Available"
    else
        echo "Vulkan: Not available"
    fi

    echo ""
}

# Show usage
show_usage() {
    cat << EOF
Engine Performance Benchmarking Tool

USAGE: $0 [OPTIONS]

OPTIONS:
    --vulkan           Benchmark Vulkan renderer only
    --opengl           Benchmark OpenGL renderer only
    --compare          Compare both renderers (default)
    --duration=N       Benchmark duration in seconds (default: 30)
    --map=MAP          Map to benchmark (default: q3dm9)
    --resolution=WxH   Resolution to test (default: 1920x1080)
    --system-info      Show system information and exit

EXAMPLES:
    $0 --compare              # Compare Vulkan vs OpenGL
    $0 --vulkan --duration=60 # Vulkan benchmark for 60 seconds
    $0 --opengl --resolution=2560x1440  # OpenGL at 1440p

RESULTS:
    Results are saved to: logs/benchmarks/
    - benchmark_results.txt: Summary of all benchmarks
    - Individual log files for each benchmark run

EOF
}

# Parse arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --vulkan)
                BENCHMARK_TYPE="vulkan"
                shift
                ;;
            --opengl)
                BENCHMARK_TYPE="opengl"
                shift
                ;;
            --compare)
                BENCHMARK_TYPE="compare"
                shift
                ;;
            --duration=*)
                DURATION="${1#*=}"
                shift
                ;;
            --map=*)
                MAP="${1#*=}"
                shift
                ;;
            --resolution=*)
                RESOLUTION="${1#*=}"
                shift
                ;;
            --system-info)
                show_system_info
                exit 0
                ;;
            --help|-h)
                show_usage
                exit 0
                ;;
            *)
                echo -e "${RED}Unknown option: $1${NC}"
                show_usage
                exit 1
                ;;
        esac
    done
}

# Main execution
main() {
    BENCHMARK_TYPE="${BENCHMARK_TYPE:-compare}"

    # Create benchmark directory
    mkdir -p "$BENCHMARK_DIR"

    # Check prerequisites
    if [ ! -f "$RELEASE_DIR/idtech3.x86_64" ]; then
        echo -e "${RED}Error: Engine binary not found${NC}"
        exit 1
    fi

    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}        id Tech 3 Engine Performance Benchmark${NC}"
    echo -e "${BLUE}============================================================${NC}"

    show_system_info

    case $BENCHMARK_TYPE in
        "vulkan")
            run_benchmark "vulkan"
            ;;
        "opengl")
            run_benchmark "opengl"
            ;;
        "compare")
            compare_renderers
            ;;
    esac
}

# Run main function
main "$@"