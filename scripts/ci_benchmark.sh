#!/bin/bash
# CI Benchmark Runner and Publisher
# Runs enhanced benchmarks and publishes results to CI/CD systems

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Configuration
BENCHMARK_BINARY="${PROJECT_ROOT}/build/src/benchmarks/renderer_bench"
BENCHMARK_DIR="${PROJECT_ROOT}/tests/benchmarks"
REPORTS_DIR="${PROJECT_ROOT}/bench_reports"
ANALYZER_SCRIPT="${PROJECT_ROOT}/tools/bench_analyzer.py"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if benchmark binary exists
if [ ! -f "$BENCHMARK_BINARY" ]; then
    log_error "Benchmark binary not found: $BENCHMARK_BINARY"
    log_info "Building benchmark first..."
    cd "$PROJECT_ROOT"
    ./scripts/compile_engine.sh -DBUILD_BENCHMARKS=ON
fi

if [ ! -f "$BENCHMARK_BINARY" ]; then
    log_error "Failed to build benchmark binary"
    exit 1
fi

# Create directories
mkdir -p "$BENCHMARK_DIR"
mkdir -p "$REPORTS_DIR"

log_info "Starting enhanced benchmark run..."

# Run benchmark with timing
BENCH_START_TIME=$(date +%s)
cd "$PROJECT_ROOT"

# Enable benchmark and run
export ENABLE_RENDERER_BENCH=1
"$BENCHMARK_BINARY" --enable-bench

BENCH_END_TIME=$(date +%s)
BENCH_DURATION=$((BENCH_END_TIME - BENCH_START_TIME))

log_success "Benchmark completed in ${BENCH_DURATION}s"

# Check if benchmark data was generated
if [ ! -f "$BENCHMARK_DIR/bench.json" ]; then
    log_error "Benchmark data not generated"
    exit 1
fi

log_info "Running benchmark analysis..."

# Run analysis script
if [ ! -f "$ANALYZER_SCRIPT" ]; then
    log_error "Benchmark analyzer script not found: $ANALYZER_SCRIPT"
    exit 1
fi

python3 "$ANALYZER_SCRIPT" --bench-dir "$BENCHMARK_DIR" --output-dir "$REPORTS_DIR"

if [ $? -ne 0 ]; then
    log_error "Benchmark analysis failed"
    exit 1
fi

log_success "Analysis complete"

# Run CI analysis for regressions
log_info "Checking for performance regressions..."
python3 "$ANALYZER_SCRIPT" --ci

CI_STATUS=$?
if [ $CI_STATUS -eq 0 ]; then
    log_success "No performance regressions detected"
else
    log_warning "Performance regressions detected - review bench_results.json"
fi

# Generate summary for CI systems
log_info "Generating CI summary..."

BENCH_SUMMARY_FILE="$BENCHMARK_DIR/bench_summary.json"
if [ -f "$BENCH_SUMMARY_FILE" ]; then
    # Extract key metrics for CI reporting
    DURATION=$(jq -r '.duration_seconds' "$BENCH_SUMMARY_FILE" 2>/dev/null || echo "N/A")
    MEMORY_END=$(jq -r '.memory_end_mb' "$BENCH_SUMMARY_FILE" 2>/dev/null || echo "N/A")
    PATHTRACER_AVG=$(jq -r '.pathTracer_avg_ms' "$BENCH_SUMMARY_FILE" 2>/dev/null || echo "N/A")
    RTX_AVG=$(jq -r '.rtx_avg_ms' "$BENCH_SUMMARY_FILE" 2>/dev/null || echo "N/A")

    echo "## Benchmark Results" >> $GITHUB_STEP_SUMMARY 2>/dev/null || true
    echo "| Metric | Value |" >> $GITHUB_STEP_SUMMARY 2>/dev/null || true
    echo "|--------|-------|" >> $GITHUB_STEP_SUMMARY 2>/dev/null || true
    echo "| Duration | ${DURATION}s |" >> $GITHUB_STEP_SUMMARY 2>/dev/null || true
    echo "| Peak Memory | ${MEMORY_END}MB |" >> $GITHUB_STEP_SUMMARY 2>/dev/null || true
    echo "| PathTracer Avg | ${PATHTRACER_AVG}ms |" >> $GITHUB_STEP_SUMMARY 2>/dev/null || true
    echo "| RTX Avg | ${RTX_AVG}ms |" >> $GITHUB_STEP_SUMMARY 2>/dev/null || true
    echo "" >> $GITHUB_STEP_SUMMARY 2>/dev/null || true
    echo "📊 [Detailed Report]($REPORTS_DIR/benchmark_report.html)" >> $GITHUB_STEP_SUMMARY 2>/dev/null || true
fi

# Set outputs for GitHub Actions
echo "benchmark_duration=$BENCH_DURATION" >> $GITHUB_OUTPUT 2>/dev/null || true
echo "reports_dir=$REPORTS_DIR" >> $GITHUB_OUTPUT 2>/dev/null || true

log_success "CI benchmark pipeline completed"

# Exit with CI analysis status
exit $CI_STATUS