#!/bin/bash
#
# Performance Benchmark Script
# Runs performance tests and generates reports
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BENCHMARK_DIR="$PROJECT_ROOT/benchmarks"
RESULTS_FILE="$BENCHMARK_DIR/results_$(date +%Y%m%d_%H%M%S).json"

mkdir -p "$BENCHMARK_DIR"

echo "Running performance benchmarks..."
echo "Results will be saved to: $RESULTS_FILE"

# Check if binaries exist
if [ ! -f "$BUILD_DIR/idtech3.x86_64" ] && [ ! -f "$BUILD_DIR/idtech3" ]; then
    echo "Error: Engine binary not found. Please build first."
    exit 1
fi

ENGINE_BINARY="$BUILD_DIR/idtech3.x86_64"
if [ ! -f "$ENGINE_BINARY" ]; then
    ENGINE_BINARY="$BUILD_DIR/idtech3"
fi

# Initialize JSON results
cat > "$RESULTS_FILE" << EOF
{
  "timestamp": "$(date -Iseconds)",
  "commit": "$(git rev-parse HEAD 2>/dev/null || echo 'unknown')",
  "branch": "$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo 'unknown')",
  "platform": "$(uname -s)",
  "arch": "$(uname -m)",
  "benchmarks": {
EOF

# Benchmark: Startup time
echo "  Benchmarking startup time..."
STARTUP_TIME=$(command time -p "$ENGINE_BINARY" +set com_developer 0 +set com_dedicated 1 +quit 2>&1 | grep real | awk '{print $2}')
echo "    \"startup_time_seconds\": $STARTUP_TIME," >> "$RESULTS_FILE"

# Benchmark: Memory usage
echo "  Benchmarking memory usage..."
MEMORY_KB=$(command time -v "$ENGINE_BINARY" +set com_developer 0 +set com_dedicated 1 +quit 2>&1 | grep "Maximum resident set size" | awk '{print $6}' || echo "0")
echo "    \"memory_usage_kb\": $MEMORY_KB," >> "$RESULTS_FILE"

# Benchmark: Binary size
echo "  Benchmarking binary size..."
BINARY_SIZE=$(stat -c%s "$ENGINE_BINARY" 2>/dev/null || stat -f%z "$ENGINE_BINARY" 2>/dev/null || echo "0")
echo "    \"binary_size_bytes\": $BINARY_SIZE," >> "$RESULTS_FILE"

# Benchmark: Build time (if available)
if [ -f "$BUILD_DIR/build_time.txt" ]; then
    BUILD_TIME=$(cat "$BUILD_DIR/build_time.txt")
    echo "    \"build_time_seconds\": $BUILD_TIME," >> "$RESULTS_FILE"
fi

# Close JSON
cat >> "$RESULTS_FILE" << EOF
    "benchmark_complete": true
  }
}
EOF

echo ""
echo "Benchmark results saved to: $RESULTS_FILE"
cat "$RESULTS_FILE" | python3 -m json.tool 2>/dev/null || cat "$RESULTS_FILE"

# Compare with baseline if it exists
BASELINE="$BENCHMARK_DIR/baseline.json"
if [ -f "$BASELINE" ]; then
    echo ""
    echo "Comparing with baseline..."
    python3 << PYEOF
import json
import sys

try:
    with open('$BASELINE', 'r') as f:
        baseline = json.load(f)
    
    with open('$RESULTS_FILE', 'r') as f:
        current = json.load(f)
    
    print("\n## Benchmark Comparison")
    print("| Metric | Baseline | Current | Change |")
    print("|--------|----------|---------|--------|")
    
    for key, baseline_val in baseline.get('benchmarks', {}).items():
        if key == 'benchmark_complete':
            continue
        current_val = current.get('benchmarks', {}).get(key, 0)
        if baseline_val > 0:
            change = ((current_val - baseline_val) / baseline_val * 100)
            print(f"| {key} | {baseline_val:.2f} | {current_val:.2f} | {change:+.2f}% |")
        else:
            print(f"| {key} | {baseline_val:.2f} | {current_val:.2f} | N/A |")
except Exception as e:
    print(f"Comparison failed: {e}", file=sys.stderr)
PYEOF
else
    echo ""
    echo "No baseline found. Saving current results as baseline..."
    cp "$RESULTS_FILE" "$BASELINE"
fi

