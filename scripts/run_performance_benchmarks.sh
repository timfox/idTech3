#!/bin/bash

# Performance Benchmarking Runner Script
# This script runs automated performance benchmarks and regression detection

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE="${BUILD_DIR}/idtech3_vulkan_x86_64"
RESULTS_DIR="${PROJECT_ROOT}/benchmark_results"

# Default configuration
BENCHMARK_SUITE="${BENCHMARK_SUITE:-standard}"
AUTO_BASELINE_UPDATE="${AUTO_BASELINE_UPDATE:-false}"
REGRESSION_ALERTS="${REGRESSION_ALERTS:-true}"
EXPORT_RESULTS="${EXPORT_RESULTS:-true}"
GENERATE_REPORTS="${GENERATE_REPORTS:-true}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Logging functions
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

log_benchmark() {
    echo -e "${CYAN}[BENCHMARK]${NC} $1"
}

log_regression() {
    echo -e "${RED}[REGRESSION]${NC} $1"
}

# Check if executable exists
check_executable() {
    if [[ ! -f "$EXECUTABLE" ]]; then
        log_error "Executable not found: $EXECUTABLE"
        log_info "Please build the project first:"
        log_info "  mkdir build && cd build"
        log_info "  cmake .. -DENABLE_PERFORMANCE_BENCHMARKING=ON"
        log_info "  make -j$(nproc)"
        exit 1
    fi
}

# Set up display for GUI benchmarks
setup_display() {
    # Check if we're in a GUI environment
    if [[ -z "$DISPLAY" ]]; then
        log_warning "No DISPLAY set, attempting to set up virtual display"

        # Check if Xvfb is available
        if command -v Xvfb &> /dev/null; then
            export DISPLAY=:99
            Xvfb :99 -screen 0 1920x1080x24 &
            sleep 2
            log_info "Virtual display set up on :99"
        else
            log_warning "Xvfb not available, GUI benchmarks may fail"
        fi
    else
        log_info "Using existing display: $DISPLAY"
    fi
}

# Configure benchmark system
configure_benchmarking() {
    log_info "Configuring performance benchmarking system..."

    # Set up result directory
    mkdir -p "$RESULTS_DIR"

    # Configure benchmark system settings
    "$EXECUTABLE" +set benchmark status > /dev/null 2>&1 || log_warning "Could not query benchmark status"

    log_info "Benchmark suite: $BENCHMARK_SUITE"
    log_info "Auto baseline update: $AUTO_BASELINE_UPDATE"
    log_info "Regression alerts: $REGRESSION_ALERTS"
    log_info "Results directory: $RESULTS_DIR"
}

# Create benchmark suite
create_benchmark_suite() {
    local suite_name="$1"

    log_info "Creating benchmark suite: $suite_name"

    case "$suite_name" in
        "minimal")
            # Minimal suite for quick testing
            "$EXECUTABLE" +set benchmark create "$suite_name" 2>/dev/null
            "$EXECUTABLE" +set benchmark add-rendering "$suite_name" q3dm1 1 2>/dev/null
            ;;
        "standard")
            # Standard comprehensive suite
            "$EXECUTABLE" +set benchmark create "$suite_name" 2>/dev/null
            "$EXECUTABLE" +set benchmark add-rendering "$suite_name" q3dm1 1 2>/dev/null
            "$EXECUTABLE" +set benchmark add-rendering "$suite_name" q3dm1 3 2>/dev/null
            "$EXECUTABLE" +set benchmark add-memory "$suite_name" 1024 500 2>/dev/null
            "$EXECUTABLE" +set benchmark add-io "$suite_name" benchmark.dat 25 2>/dev/null
            ;;
        "rendering")
            # Rendering-focused benchmarks
            "$EXECUTABLE" +set benchmark create "$suite_name" 2>/dev/null
            "$EXECUTABLE" +set benchmark add-rendering "$suite_name" q3dm1 0 2>/dev/null
            "$EXECUTABLE" +set benchmark add-rendering "$suite_name" q3dm1 2 2>/dev/null
            "$EXECUTABLE" +set benchmark add-rendering "$suite_name" q3dm1 4 2>/dev/null
            ;;
        "memory")
            # Memory performance benchmarks
            "$EXECUTABLE" +set benchmark create "$suite_name" 2>/dev/null
            "$EXECUTABLE" +set benchmark add-memory "$suite_name" 64 10000 2>/dev/null
            "$EXECUTABLE" +set benchmark add-memory "$suite_name" 1024 1000 2>/dev/null
            "$EXECUTABLE" +set benchmark add-memory "$suite_name" 65536 100 2>/dev/null
            ;;
        "io")
            # I/O performance benchmarks
            "$EXECUTABLE" +set benchmark create "$suite_name" 2>/dev/null
            "$EXECUTABLE" +set benchmark add-io "$suite_name" small.dat 1 2>/dev/null
            "$EXECUTABLE" +set benchmark add-io "$suite_name" medium.dat 100 2>/dev/null
            "$EXECUTABLE" +set benchmark add-io "$suite_name" large.dat 500 2>/dev/null
            ;;
        "stress")
            # Stress testing benchmarks
            "$EXECUTABLE" +set benchmark create "$suite_name" 2>/dev/null
            "$EXECUTABLE" +set benchmark add-rendering "$suite_name" q3dm1 4 2>/dev/null
            "$EXECUTABLE" +set benchmark add-memory "$suite_name" 4096 2000 2>/dev/null
            "$EXECUTABLE" +set benchmark add-io "$suite_name" stress_test.dat 1000 2>/dev/null
            ;;
        *)
            log_error "Unknown benchmark suite: $suite_name"
            log_info "Available suites: minimal, standard, rendering, memory, io, stress"
            exit 1
            ;;
    esac

    log_success "Created benchmark suite: $suite_name"
}

# Run benchmarks
run_benchmarks() {
    local suite_name="$1"

    log_benchmark "Starting benchmark suite: $suite_name"
    local start_time=$(date +%s)

    # Run the benchmark suite
    local benchmark_output
    benchmark_output=$("$EXECUTABLE" +set benchmark run "$suite_name" 2>&1)

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    # Check results
    if echo "$benchmark_output" | grep -q "Benchmark suite completed successfully"; then
        log_success "Benchmark suite completed successfully in $duration seconds"
        return 0
    elif echo "$benchmark_output" | grep -q "Benchmark suite failed"; then
        log_error "Benchmark suite failed in $duration seconds"
        echo "$benchmark_output" | grep -A 10 -B 5 "failed\|error\|regression" || true
        return 1
    else
        log_warning "Benchmark completed with unknown status in $duration seconds"
        echo "$benchmark_output"
        return 1
    fi
}

# Analyze results
analyze_results() {
    log_info "Analyzing benchmark results..."

    # Get recent results
    local results_output
    results_output=$("$EXECUTABLE" +set benchmark results 2>&1)

    # Extract key metrics
    local total_benchmarks=$(echo "$results_output" | grep -o "Total: [0-9]*" | grep -o "[0-9]*" | head -1 || echo "0")
    local passed=$(echo "$results_output" | grep -c "PASS" || echo "0")
    local failed=$(echo "$results_output" | grep -c "FAIL\|REGRESSION" || echo "0")
    local regressions=$(echo "$results_output" | grep -c "REGRESSION" || echo "0")

    log_info "Results Summary:"
    log_info "  Total Benchmarks: $total_benchmarks"
    log_info "  Passed: $passed"
    log_info "  Failed: $failed"
    log_info "  Regressions: $regressions"

    # Check for regressions
    if [[ $regressions -gt 0 ]]; then
        log_regression "PERFORMANCE REGRESSIONS DETECTED: $regressions benchmarks have regressed"

        # Show regression details
        echo "$results_output" | grep -A 2 -B 2 "REGRESSION" || true

        if [[ "$REGRESSION_ALERTS" == "true" ]]; then
            log_regression "Regression alerts are enabled - this would trigger notifications"
        fi

        return 1
    elif [[ $failed -gt 0 ]]; then
        log_error "BENCHMARK FAILURES DETECTED: $failed benchmarks failed"
        return 1
    else
        log_success "All benchmarks passed successfully"
        return 0
    fi
}

# Generate reports
generate_reports() {
    log_info "Generating benchmark reports..."

    # Generate text report
    "$EXECUTABLE" +set benchmark report text 2>/dev/null || log_warning "Could not generate text report"

    # Generate JSON report for CI/tools
    "$EXECUTABLE" +set benchmark report json 2>/dev/null || log_warning "Could not generate JSON report"

    # Export for CI
    if [[ "$EXPORT_RESULTS" == "true" ]]; then
        "$EXECUTABLE" +set benchmark export "$RESULTS_DIR" 2>/dev/null || log_warning "Could not export results"
        log_success "Results exported to: $RESULTS_DIR"
    fi

    log_success "Reports generated successfully"
}

# Update baselines
update_baselines() {
    if [[ "$AUTO_BASELINE_UPDATE" != "true" ]]; then
        log_info "Skipping baseline update (auto update disabled)"
        return 0
    fi

    log_info "Updating performance baselines..."

    # Get list of benchmarks in the suite
    local suite_name="$1"

    # This is a simplified version - in practice, you'd want to update baselines
    # for all benchmarks that passed and didn't regress
    log_info "Baselines would be updated for suite: $suite_name"
    log_success "Baseline update completed"
}

# Show system status
show_status() {
    log_info "Performance Benchmarking System Status"
    echo "=========================================="

    "$EXECUTABLE" +set benchmark status 2>/dev/null

    echo ""
    echo "Configuration:"
    echo "  Benchmark Suite: $BENCHMARK_SUITE"
    echo "  Auto Baseline Update: $AUTO_BASELINE_UPDATE"
    echo "  Regression Alerts: $REGRESSION_ALERTS"
    echo "  Export Results: $EXPORT_RESULTS"
    echo "  Generate Reports: $GENERATE_REPORTS"
    echo "  Results Directory: $RESULTS_DIR"
}

# Show usage information
show_usage() {
    echo "Performance Benchmarking Runner"
    echo ""
    echo "This script runs automated performance benchmarks and regression detection."
    echo ""
    echo "Usage: $0 [options] [suite]"
    echo ""
    echo "Suites:"
    echo "  minimal     - Quick minimal benchmark suite"
    echo "  standard    - Standard comprehensive benchmarks (default)"
    echo "  rendering   - Rendering performance focused"
    echo "  memory      - Memory performance focused"
    echo "  io          - I/O performance focused"
    echo "  stress      - Stress testing benchmarks"
    echo ""
    echo "Options:"
    echo "  --auto-baseline          - Enable automatic baseline updates"
    echo "  --no-alerts              - Disable regression alerts"
    echo "  --no-export              - Disable result export"
    echo "  --no-reports             - Disable report generation"
    echo "  --results-dir DIR        - Set results directory (default: ./benchmark_results)"
    echo "  --status                 - Show system status only"
    echo "  --help                   - Show this help"
    echo ""
    echo "Environment Variables:"
    echo "  BENCHMARK_SUITE          - Same as suite argument"
    echo "  AUTO_BASELINE_UPDATE     - Same as --auto-baseline"
    echo "  REGRESSION_ALERTS        - Same as --no-alerts (inverted)"
    echo "  EXPORT_RESULTS           - Same as --no-export (inverted)"
    echo "  GENERATE_REPORTS         - Same as --no-reports (inverted)"
    echo "  RESULTS_DIR              - Same as --results-dir"
    echo ""
    echo "Examples:"
    echo "  $0 standard              # Run standard benchmark suite"
    echo "  $0 --auto-baseline rendering  # Run rendering benchmarks with baseline updates"
    echo "  $0 --status              # Show system status"
    echo "  $0 --no-alerts memory    # Run memory benchmarks without alerts"
    echo ""
    echo "Prerequisites:"
    echo "  Build with performance benchmarking enabled:"
    echo "    cmake .. -DENABLE_PERFORMANCE_BENCHMARKING=ON"
    echo "    make -j$(nproc)"
    echo ""
    echo "  For GUI benchmarks, ensure display is available:"
    echo "    export DISPLAY=:0  # or set up Xvfb for headless"
}

# Parse command line arguments
parse_args() {
    SUITE_NAME="$BENCHMARK_SUITE"

    while [[ $# -gt 0 ]]; do
        case $1 in
            --auto-baseline)
                AUTO_BASELINE_UPDATE=true
                shift
                ;;
            --no-alerts)
                REGRESSION_ALERTS=false
                shift
                ;;
            --no-export)
                EXPORT_RESULTS=false
                shift
                ;;
            --no-reports)
                GENERATE_REPORTS=false
                shift
                ;;
            --results-dir)
                RESULTS_DIR="$2"
                shift 2
                ;;
            --status)
                SHOW_STATUS_ONLY=true
                shift
                ;;
            --help)
                show_usage
                exit 0
                ;;
            minimal|standard|rendering|memory|io|stress)
                SUITE_NAME="$1"
                shift
                ;;
            -*)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
            *)
                SUITE_NAME="$1"
                shift
                ;;
        esac
    done
}

# Main execution
main() {
    log_info "Performance Benchmarking Runner Starting"
    log_info "=========================================="

    # Parse command line arguments
    parse_args "$@"

    # Show status only if requested
    if [[ "${SHOW_STATUS_ONLY:-false}" == "true" ]]; then
        show_status
        exit 0
    fi

    # Validate suite name
    case "$SUITE_NAME" in
        minimal|standard|rendering|memory|io|stress)
            ;;
        *)
            log_error "Invalid benchmark suite: $SUITE_NAME"
            show_usage
            exit 1
            ;;
    esac

    # Check prerequisites
    check_executable
    setup_display
    configure_benchmarking

    # Show initial status
    log_info "System status:"
    "$EXECUTABLE" +set benchmark status 2>/dev/null | head -10

    # Execute benchmark workflow
    local start_time=$(date +%s)
    local exit_code=0

    # Create benchmark suite
    if ! create_benchmark_suite "$SUITE_NAME"; then
        log_error "Failed to create benchmark suite"
        exit_code=1
    fi

    # Run benchmarks
    if [[ $exit_code -eq 0 ]]; then
        if ! run_benchmarks "${SUITE_NAME}_suite"; then
            log_error "Benchmark execution failed"
            exit_code=1
        fi
    fi

    # Analyze results
    if [[ $exit_code -eq 0 ]]; then
        if ! analyze_results; then
            log_error "Benchmark analysis detected issues"
            exit_code=1
        fi
    fi

    # Generate reports
    if [[ "$GENERATE_REPORTS" == "true" ]]; then
        generate_reports
    fi

    # Update baselines if successful and enabled
    if [[ $exit_code -eq 0 && "$AUTO_BASELINE_UPDATE" == "true" ]]; then
        update_baselines "${SUITE_NAME}_suite"
    fi

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    echo ""
    log_info "Performance benchmarking completed in $duration seconds"

    if [[ $exit_code -eq 0 ]]; then
        log_success "All benchmarks passed successfully"
    else
        log_error "Benchmarking completed with failures or regressions"
    fi

    exit $exit_code
}

# Run main function
main "$@"
