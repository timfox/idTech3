#!/bin/bash
# Performance test runner for Surf engine
# This script runs performance benchmarks and generates reports

set -e

# Configuration
BENCHMARK_DURATION=60
OUTPUT_DIR="${1:-build-perf}"
LOG_FILE="${OUTPUT_DIR}/performance_log.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=== Surf Performance Test Suite ==="
echo "Output directory: ${OUTPUT_DIR}"
echo "Log file: ${LOG_FILE}"
echo ""

# Create output directory
mkdir -p "${OUTPUT_DIR}"

# Function to run a benchmark
run_benchmark() {
    local name="$1"
    local command="$2"
    local iterations="${3:-3}"
    
    echo -e "${YELLOW}Running benchmark: ${name}${NC}"
    echo "Command: ${command}"
    
    local times=()
    for i in $(seq 1 $iterations); do
        echo "  Iteration ${i}/${iterations}..."
        local start_time=$(date +%s.%N)
        eval "$command" > /dev/null 2>&1
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc)
        times+=("$duration")
        echo "    Duration: ${duration}s"
    done
    
    # Calculate statistics
    local sum=0
    for t in "${times[@]}"; do
        sum=$(echo "$sum + $t" | bc)
    done
    local avg=$(echo "scale=3; $sum / ${#times[@]}" | bc)
    
    # Find min and max
    local min=${times[0]}
    local max=${times[0]}
    for t in "${times[@]}"; do
        if (( $(echo "$t < $min" | bc -l) )); then min=$t; fi
        if (( $(echo "$t > $max" | bc -l) )); then max=$t; fi
    done
    
    echo "  Statistics: min=${min}s, max=${max}s, avg=${avg}s"
    echo ""
    
    # Return results as space-separated values
    echo "${name} ${avg} ${min} ${max}"
}

# Function to measure memory usage
measure_memory() {
    local process_name="$1"
    local pid=$(pgrep -f "$process_name")
    
    if [ -n "$pid" ]; then
        local mem=$(ps -o rss= -p $pid 2>/dev/null | awk '{sum+=$1} END {print sum/1024}')
        echo "Memory usage: ${mem} MB"
        return 0
    else
        echo "Process not found: $process_name"
        return 1
    fi
}

# Function to measure CPU usage
measure_cpu() {
    local process_name="$1"
    local duration="${2:-5}"
    
    local cpu=$(top -b -n 2 -d $duration -p $(pgrep -f "$process_name" | head -1) 2>/dev/null | grep "$process_name" | awk '{print $9}')
    echo "CPU usage: ${cpu}%"
}

# Main benchmark suite
echo "Starting performance benchmarks..."
echo ""

# Run movement physics benchmark
if [ -f "build-debug/tests/unit_surfmove" ]; then
    run_benchmark "movement_physics" "./build-debug/tests/unit_surfmove --benchmark" 3 >> "${LOG_FILE}"
fi

# Run BSP collision benchmark
if [ -f "build-debug/tests/unit_surf_trace_ex" ]; then
    run_benchmark "bsp_collision" "./build-debug/tests/unit_surf_trace_ex --benchmark" 3 >> "${LOG_FILE}"
fi

# Run pmove replay benchmark
if [ -f "build-debug/tests/unit_pmove_replay" ]; then
    run_benchmark "pmove_replay" "./build-debug/tests/unit_pmove_replay --benchmark" 3 >> "${LOG_FILE}"
fi

# Run VR codec benchmark
if [ -f "build-debug/tests/unit_vr_head_codec" ]; then
    run_benchmark "vr_head_codec" "./build-debug/tests/unit_vr_head_codec --benchmark" 3 >> "${LOG_FILE}"
fi

# Generate summary report
echo "=== Performance Test Summary ===" >> "${LOG_FILE}"
echo "Generated: $(date)" >> "${LOG_FILE}"
echo "" >> "${LOG_FILE}"

# Read results and generate summary
if [ -f "${LOG_FILE}" ]; then
    grep "Statistics:" "${LOG_FILE}" >> "${LOG_FILE}.summary"
    echo "Results saved to ${LOG_FILE}.summary"
fi

echo -e "${GREEN}Performance tests completed successfully!${NC}"
echo "Results saved to: ${LOG_FILE}"

exit 0