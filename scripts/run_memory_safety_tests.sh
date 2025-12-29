#!/bin/bash

# Memory Safety Test Runner Script
# This script demonstrates running ASan/UBSan memory safety tests

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE="${BUILD_DIR}/idtech3_vulkan_x86_64"
OUTPUT_DIR="${PROJECT_ROOT}/memory_safety_results"

# Default test configuration
SANITIZERS="${SANITIZERS:-asan,ubsan,lsan}"
STRICT_MODE="${STRICT_MODE:-false}"
GENERATE_REPORTS="${GENERATE_REPORTS:-true}"

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
        log_info "Please build the project with sanitizers enabled:"
        log_info "  mkdir build && cd build"
        log_info "  export CC=clang CXX=clang++"
        log_info "  cmake .. -DCMAKE_C_FLAGS=\"-fsanitize=address,undefined -fsanitize=leak\" \\"
        log_info "           -DCMAKE_CXX_FLAGS=\"-fsanitize=address,undefined -fsanitize=leak\" \\"
        log_info "           -DCMAKE_EXE_LINKER_FLAGS=\"-fsanitize=address,undefined -fsanitize=leak\""
        log_info "  make -j$(nproc)"
        exit 1
    fi
}

# Enable sanitizers
enable_sanitizers() {
    log_info "Enabling sanitizers..."

    # Enable ASan if requested
    if [[ "$SANITIZERS" == *"asan"* ]]; then
        "$EXECUTABLE" +set memtest enable asan 2>/dev/null || log_warning "ASan not available"
    fi

    # Enable UBSan if requested
    if [[ "$SANITIZERS" == *"ubsan"* ]]; then
        "$EXECUTABLE" +set memtest enable ubsan 2>/dev/null || log_warning "UBSan not available"
    fi

    # Enable LSan if requested
    if [[ "$SANITIZERS" == *"lsan"* ]]; then
        "$EXECUTABLE" +set memtest enable lsan 2>/dev/null || log_warning "LSan not available"
    fi

    # Set strict mode
    if [[ "$STRICT_MODE" == "true" ]]; then
        "$EXECUTABLE" +set memtest strict on 2>/dev/null || log_warning "Could not enable strict mode"
    fi
}

# Run individual test
run_individual_test() {
    local test_name="$1"
    log_info "Running memory safety test: $test_name"

    # Run the test and capture output
    local output
    output=$("$EXECUTABLE" +set memtest run "$test_name" 2>&1)

    # Check for sanitizer errors in output
    if echo "$output" | grep -q "ERROR\|runtime error\|AddressSanitizer\|UndefinedBehaviorSanitizer"; then
        log_error "Memory safety error detected in test: $test_name"
        echo "$output" | grep -A 10 -B 2 "ERROR\|runtime error\|AddressSanitizer\|UndefinedBehaviorSanitizer"
        return 1
    else
        log_success "Test '$test_name' passed"
        return 0
    fi
}

# Run test suite
run_test_suite() {
    local suite_name="$1"
    local suite_desc="$2"
    shift 2

    log_info "Creating memory safety test suite: $suite_name"

    # Create suite
    "$EXECUTABLE" +set memtest suite create "$suite_name" "$suite_desc" 2>/dev/null

    # Add tests to suite
    for test_name in "$@"; do
        "$EXECUTABLE" +set memtest suite add "$suite_name" "$test_name" 2>/dev/null
    done

    log_info "Running test suite: $suite_name"

    # Run the suite and capture output
    local output
    output=$("$EXECUTABLE" +set memtest suite run "$suite_name" 2>&1)

    # Analyze results
    local asan_errors ubsan_errors leaks crashes
    asan_errors=$(echo "$output" | grep -c "ASan error\|heap-buffer-overflow\|use-after-free\|AddressSanitizer" || echo "0")
    ubsan_errors=$(echo "$output" | grep -c "UBSan error\|runtime error\|UndefinedBehaviorSanitizer" || echo "0")
    leaks=$(echo "$output" | grep -c "memory leak\|LeakSanitizer" || echo "0")
    crashes=$(echo "$output" | grep -c "crashed\|CRASH" || echo "0")

    if [[ $asan_errors -gt 0 ]] || [[ $ubsan_errors -gt 0 ]] || [[ $leaks -gt 0 ]] || [[ $crashes -gt 0 ]]; then
        log_error "Memory safety issues detected in suite '$suite_name':"
        log_error "  ASan errors: $asan_errors"
        log_error "  UBSan errors: $ubsan_errors"
        log_error "  Memory leaks: $leaks"
        log_error "  Crashes: $crashes"

        # Show some error details
        echo "$output" | grep -A 5 "ERROR\|runtime error\|AddressSanitizer\|UndefinedBehaviorSanitizer\|memory leak" | head -20
        return 1
    else
        log_success "Test suite '$suite_name' completed without memory safety issues"
        return 0
    fi
}

# Generate comprehensive test report
generate_report() {
    log_info "Generating memory safety test report..."

    mkdir -p "$OUTPUT_DIR"

    # Generate JSON report
    "$EXECUTABLE" +set memtest report "${OUTPUT_DIR}/memory_safety_report.json" 2>/dev/null

    # Export results for CI
    "$EXECUTABLE" +set memtest export "${OUTPUT_DIR}/ci_export" 2>/dev/null

    # Show final statistics
    log_info "Final test statistics:"
    "$EXECUTABLE" +set memtest stats 2>/dev/null

    log_success "Reports generated in: $OUTPUT_DIR"
}

# Show usage information
show_usage() {
    echo "Memory Safety Test Runner"
    echo ""
    echo "This script runs ASan/UBSan memory safety tests on the idtech3 engine."
    echo ""
    echo "Usage: $0 [options] [test_type]"
    echo ""
    echo "Test Types:"
    echo "  basic      - Run basic memory safety tests"
    echo "  comprehensive - Run comprehensive test suite"
    echo "  asan       - Run ASan-specific tests"
    echo "  ubsan      - Run UBSan-specific tests"
    echo "  leaks      - Run memory leak detection tests"
    echo "  custom     - Run custom test (specify with CUSTOM_TESTS)"
    echo ""
    echo "Options:"
    echo "  --sanitizers SANITIZERS   Comma-separated list of sanitizers (default: asan,ubsan,lsan)"
    echo "  --strict                  Enable strict mode (warnings as errors)"
    echo "  --no-reports              Skip report generation"
    echo "  --help                    Show this help"
    echo ""
    echo "Environment Variables:"
    echo "  SANITIZERS                Same as --sanitizers"
    echo "  STRICT_MODE               Same as --strict"
    echo "  CUSTOM_TESTS              Space-separated list of custom tests to run"
    echo ""
    echo "Examples:"
    echo "  $0 basic"
    echo "  $0 --sanitizers asan,lsan comprehensive"
    echo "  $0 --strict ubsan"
    echo ""
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --sanitizers)
                SANITIZERS="$2"
                shift 2
                ;;
            --strict)
                STRICT_MODE=true
                shift
                ;;
            --no-reports)
                GENERATE_REPORTS=false
                shift
                ;;
            --help)
                show_usage
                exit 0
                ;;
            -*)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
            *)
                TEST_TYPE="$1"
                shift
                ;;
        esac
    done

    # Set default test type
    TEST_TYPE="${TEST_TYPE:-basic}"
}

# Main test execution based on type
run_tests() {
    case "$TEST_TYPE" in
        "basic")
            log_info "Running basic memory safety tests..."
            run_test_suite "basic_suite" "Basic Memory Safety Test Suite" \
                "buffer_overflow" "use_after_free" "memory_leak" "memory_management"
            ;;

        "asan")
            log_info "Running ASan-specific tests..."
            run_test_suite "asan_suite" "ASan Memory Safety Test Suite" \
                "buffer_overflow" "use_after_free" "double_free" "stack_overflow" "memory_management"
            ;;

        "ubsan")
            log_info "Running UBSan-specific tests..."
            run_test_suite "ubsan_suite" "UBSan Memory Safety Test Suite" \
                "integer_overflow" "division_by_zero" "null_pointer_deref" "type_confusion"
            ;;

        "leaks")
            log_info "Running memory leak detection tests..."
            run_test_suite "leak_suite" "Memory Leak Detection Test Suite" \
                "memory_leak" "memory_management"
            ;;

        "comprehensive")
            log_info "Running comprehensive memory safety test suite..."
            run_test_suite "comprehensive_suite" "Comprehensive Memory Safety Test Suite" \
                "buffer_overflow" "use_after_free" "double_free" "memory_leak" \
                "integer_overflow" "division_by_zero" "null_pointer_deref" \
                "uninitialized_variable" "type_confusion" "stack_overflow" \
                "file_operations" "memory_management" "string_operations" "data_structures"
            ;;

        "custom")
            if [[ -z "$CUSTOM_TESTS" ]]; then
                log_error "CUSTOM_TESTS environment variable not set"
                log_info "Set CUSTOM_TESTS to a space-separated list of test names"
                exit 1
            fi

            log_info "Running custom memory safety tests: $CUSTOM_TESTS"
            run_test_suite "custom_suite" "Custom Memory Safety Test Suite" $CUSTOM_TESTS
            ;;

        *)
            log_error "Unknown test type: $TEST_TYPE"
            show_usage
            exit 1
            ;;
    esac
}

# Main execution
main() {
    log_info "Memory Safety Test Runner Starting"
    log_info "==================================="

    # Parse command line arguments
    parse_args "$@"

    # Check prerequisites
    check_executable

    # Enable sanitizers
    enable_sanitizers

    # Show sanitizer status
    log_info "Sanitizer status:"
    "$EXECUTABLE" +set memtest status 2>/dev/null

    # Run the tests
    local test_start_time=$(date +%s)
    if run_tests; then
        local test_end_time=$(date +%s)
        local test_duration=$((test_end_time - test_start_time))

        log_success "All memory safety tests completed successfully"
        log_info "Total test time: $test_duration seconds"

        # Generate reports
        if [[ "$GENERATE_REPORTS" == "true" ]]; then
            generate_report
        fi

        log_success "Memory safety testing completed successfully!"
        return 0
    else
        log_error "Memory safety tests detected issues!"
        return 1
    fi
}

# Run main function
main "$@"
