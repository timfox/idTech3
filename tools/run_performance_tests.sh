#!/bin/bash

# Performance Test Runner Script
# This script provides a convenient way to run performance tests
# locally or in CI/CD environments

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE="${BUILD_DIR}/idtech3_vulkan_x86_64"
OUTPUT_DIR="${PROJECT_ROOT}/perf_results"
BASELINE_DIR="${PROJECT_ROOT}/perf_baselines"

# Default test configuration
TEST_TYPE="${TEST_TYPE:-full}"
DURATION="${DURATION:-30}"
WARMUP="${WARMUP:-5}"
UPDATE_BASELINES="${UPDATE_BASELINES:-false}"
COMPARE_RESULTS="${COMPARE_RESULTS:-false}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

# Check if executable exists
check_executable() {
    if [[ ! -f "$EXECUTABLE" ]]; then
        log_error "Executable not found: $EXECUTABLE"
        log_info "Please build the project first:"
        log_info "  mkdir build && cd build"
        log_info "  cmake .. -DCMAKE_BUILD_TYPE=Release"
        log_info "  make -j$(nproc)"
        exit 1
    fi
}

# Run a single performance test
run_single_test() {
    local test_name="$1"
    local duration="$2"
    local warmup="$3"

    log_info "Running performance test: $test_name"
    log_info "  Duration: $duration seconds"
    log_info "  Warmup: $warmup seconds"

    "$EXECUTABLE" +set perftest run "$test_name" "$duration" "$warmup"

    local result=$?
    if [[ $result -eq 0 ]]; then
        log_success "Test '$test_name' completed successfully"
    else
        log_error "Test '$test_name' failed with exit code $result"
        return $result
    fi
}

# Run a test suite
run_test_suite() {
    local suite_name="$1"
    local suite_desc="$2"
    shift 2

    log_info "Creating test suite: $suite_name"
    "$EXECUTABLE" +set perftest suite create "$suite_name" "$suite_desc"

    # Add tests to suite
    while [[ $# -gt 0 ]]; do
        local test_name="$1"
        local duration="$2"
        local warmup="$3"
        shift 3

        log_info "Adding test to suite: $test_name ($duration sec, $warmup sec warmup)"
        "$EXECUTABLE" +set perftest suite add "$suite_name" "$test_name" "$duration" "$warmup"
    done

    log_info "Running test suite: $suite_name"
    "$EXECUTABLE" +set perftest suite run "$suite_name"

    local result=$?
    if [[ $result -eq 0 ]]; then
        log_success "Test suite '$suite_name' completed successfully"
    else
        log_error "Test suite '$suite_name' failed with exit code $result"
        return $result
    fi
}

# Load performance baselines
load_baselines() {
    if [[ -f "${BASELINE_DIR}/baselines.json" ]]; then
        log_info "Loading performance baselines..."
        "$EXECUTABLE" +set perftest baseline load "${BASELINE_DIR}/baselines.json"
    else
        log_warning "No baseline file found at ${BASELINE_DIR}/baselines.json"
    fi
}

# Save performance baselines
save_baselines() {
    log_info "Saving performance baselines..."
    mkdir -p "$BASELINE_DIR"
    "$EXECUTABLE" +set perftest baseline save "${BASELINE_DIR}/baselines.json"
    log_success "Baselines saved to ${BASELINE_DIR}/baselines.json"
}

# Generate performance report
generate_report() {
    local report_file="$1"
    log_info "Generating performance report: $report_file"
    mkdir -p "$(dirname "$report_file")"
    "$EXECUTABLE" +set perftest report "$report_file"
    log_success "Report generated: $report_file"
}

# Export results for CI
export_for_ci() {
    local export_dir="$1"
    log_info "Exporting results for CI: $export_dir"
    mkdir -p "$export_dir"
    "$EXECUTABLE" +set perftest ci export "$export_dir"
    log_success "Results exported to: $export_dir"
}

# Show performance statistics
show_stats() {
    log_info "Performance test statistics:"
    "$EXECUTABLE" +set perftest stats
}

# Main test execution based on type
run_tests() {
    case "$TEST_TYPE" in
        "smoke")
            log_info "Running smoke performance tests..."
            run_test_suite "smoke_suite" "Smoke Performance Test Suite" \
                "basic_rendering" 15 3 \
                "memory_basic" 10 2
            ;;

        "full")
            log_info "Running full performance test suite..."
            run_test_suite "full_suite" "Full Performance Test Suite" \
                "basic_rendering" 60 10 \
                "memory_stress" 45 5 \
                "asset_loading" 30 5 \
                "physics_simulation" 45 8 \
                "network_stress" 30 3 \
                "multithreaded_rendering" 40 7
            ;;

        "targeted")
            log_info "Running targeted performance tests..."
            run_single_test "basic_rendering" "$DURATION" "$WARMUP"
            ;;

        "custom")
            log_info "Running custom performance test..."
            run_single_test "${CUSTOM_TEST:-custom_test}" "$DURATION" "$WARMUP"
            ;;

        *)
            log_error "Unknown test type: $TEST_TYPE"
            log_info "Available types: smoke, full, targeted, custom"
            exit 1
            ;;
    esac
}

# Check for performance regressions
check_regressions() {
    log_info "Checking for performance regressions..."

    # Get regression count
    local regression_output
    regression_output=$("$EXECUTABLE" +set perftest stats 2>/dev/null | grep "Performance Regressions:" | awk '{print $3}' || echo "0")

    local regression_count=${regression_output:-0}

    if [[ $regression_count -gt 0 ]]; then
        log_error "PERFORMANCE REGRESSION DETECTED: $regression_count regression(s) found"
        "$EXECUTABLE" +set perftest findings | head -20
        return 1
    else
        log_success "No performance regressions detected"
        return 0
    fi
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --type)
                TEST_TYPE="$2"
                shift 2
                ;;
            --duration)
                DURATION="$2"
                shift 2
                ;;
            --warmup)
                WARMUP="$2"
                shift 2
                ;;
            --update-baselines)
                UPDATE_BASELINES=true
                shift
                ;;
            --compare)
                COMPARE_RESULTS=true
                shift
                ;;
            --output-dir)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            --help)
                echo "Performance Test Runner"
                echo ""
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  --type TYPE          Test type: smoke, full, targeted, custom (default: full)"
                echo "  --duration SEC       Test duration in seconds (default: 30)"
                echo "  --warmup SEC         Warmup time in seconds (default: 5)"
                echo "  --update-baselines   Update performance baselines after successful run"
                echo "  --compare            Compare results with previous run"
                echo "  --output-dir DIR     Output directory for reports (default: perf_results)"
                echo "  --help               Show this help"
                echo ""
                echo "Environment variables:"
                echo "  TEST_TYPE            Same as --type"
                echo "  DURATION             Same as --duration"
                echo "  WARMUP               Same as --warmup"
                echo "  UPDATE_BASELINES     Same as --update-baselines"
                echo "  CUSTOM_TEST          Test name for custom type"
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                log_info "Use --help for usage information"
                exit 1
                ;;
        esac
    done
}

# Main execution
main() {
    log_info "Performance Test Runner Starting"
    log_info "================================="

    # Parse command line arguments
    parse_args "$@"

    # Check prerequisites
    check_executable

    # Create output directories
    mkdir -p "$OUTPUT_DIR"
    mkdir -p "$BASELINE_DIR"

    # Load baselines if they exist
    load_baselines

    # Run the tests
    local test_start_time=$(date +%s)
    if run_tests; then
        local test_end_time=$(date +%s)
        local test_duration=$((test_end_time - test_start_time))

        log_success "All performance tests completed successfully"
        log_info "Total test time: $test_duration seconds"

        # Check for regressions
        if check_regressions; then
            # Generate reports
            local timestamp=$(date +%Y%m%d_%H%M%S)
            local report_file="${OUTPUT_DIR}/perf_report_${timestamp}.json"
            local ci_export_dir="${OUTPUT_DIR}/ci_export_${timestamp}"

            generate_report "$report_file"
            export_for_ci "$ci_export_dir"

            # Update baselines if requested
            if [[ "$UPDATE_BASELINES" == "true" ]]; then
                save_baselines
            fi

            # Show final statistics
            show_stats

            log_success "Performance testing completed successfully!"
            log_info "Reports saved to: $OUTPUT_DIR"
            return 0
        else
            log_error "Performance regressions detected!"
            return 1
        fi
    else
        log_error "Performance tests failed!"
        return 1
    fi
}

# Run main function
main "$@"
