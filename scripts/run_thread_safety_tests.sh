#!/bin/bash

# Thread Safety Test Runner Script
# This script runs ThreadSanitizer (TSan) thread safety tests

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE="${BUILD_DIR}/idtech3_vulkan_x86_64"
OUTPUT_DIR="${PROJECT_ROOT}/thread_safety_results"

# Default test configuration
SANITIZERS="${SANITIZERS:-tsan}"
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
        log_info "Please build the project with ThreadSanitizer enabled:"
        log_info "  mkdir build && cd build"
        log_info "  export CC=clang CXX=clang++"
        log_info "  cmake .. -DCMAKE_C_FLAGS=\"-fsanitize=thread -fno-omit-frame-pointer -g\" \\"
        log_info "           -DCMAKE_CXX_FLAGS=\"-fsanitize=thread -fno-omit-frame-pointer -g\" \\"
        log_info "           -DCMAKE_EXE_LINKER_FLAGS=\"-fsanitize=thread\""
        log_info "  make -j$(nproc)"
        exit 1
    fi
}

# Enable ThreadSanitizer
enable_tsan() {
    log_info "Enabling ThreadSanitizer..."

    # Enable TSan if requested
    if [[ "$SANITIZERS" == *"tsan"* ]]; then
        "$EXECUTABLE" +set threadtest enable tsan 2>/dev/null || log_warning "TSan not available"
    fi

    # Enable deadlock detection
    if [[ "$SANITIZERS" == *"deadlock"* ]]; then
        "$EXECUTABLE" +set threadtest enable deadlock 2>/dev/null || log_warning "Deadlock detection not available"
    fi

    # Enable race detection
    if [[ "$SANITIZERS" == *"race"* ]]; then
        "$EXECUTABLE" +set threadtest enable race 2>/dev/null || log_warning "Race detection not available"
    fi

    # Set strict mode
    if [[ "$STRICT_MODE" == "true" ]]; then
        "$EXECUTABLE" +set threadtest strict on 2>/dev/null || log_warning "Could not enable strict mode"
    fi
}

# Run individual test
run_individual_test() {
    local test_name="$1"
    log_info "Running thread safety test: $test_name"

    # Run the test and capture output
    local output
    output=$("$EXECUTABLE" +set threadtest run "$test_name" 2>&1)

    # Check for TSan errors in output
    if echo "$output" | grep -q "WARNING\|ERROR\|data race\|deadlock\|ThreadSanitizer\|WARNING: ThreadSanitizer"; then
        log_error "Thread safety issues detected in test: $test_name"
        echo "$output" | grep -A 10 -B 2 "WARNING\|ERROR\|data race\|deadlock\|ThreadSanitizer\|WARNING: ThreadSanitizer" | head -20
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

    log_info "Creating thread safety test suite: $suite_name"

    # Create suite
    "$EXECUTABLE" +set threadtest suite create "$suite_name" "$suite_desc" 2>/dev/null

    # Add tests to suite
    for test_name in "$@"; do
        "$EXECUTABLE" +set threadtest suite add "$suite_name" "$test_name" 2>/dev/null
    done

    log_info "Running test suite: $suite_name"

    # Run the suite and capture output
    local output
    output=$("$EXECUTABLE" +set threadtest suite run "$suite_name" 2>&1)

    # Analyze results
    local data_races deadlocks atomic_violations lock_order_violations
    data_races=$(echo "$output" | grep -c "DATA_RACE\|data race" || echo "0")
    deadlocks=$(echo "$output" | grep -c "DEADLOCK\|deadlock" || echo "0")
    atomic_violations=$(echo "$output" | grep -c "ATOMIC_VIOLATION\|atomic.*violation" || echo "0")
    lock_order_violations=$(echo "$output" | grep -c "LOCK_ORDER_VIOLATION\|lock.*order" || echo "0")

    if [[ $data_races -gt 0 ]] || [[ $deadlocks -gt 0 ]] || [[ $atomic_violations -gt 0 ]] || [[ $lock_order_violations -gt 0 ]]; then
        log_error "Thread safety issues detected in suite '$suite_name':"
        log_error "  Data races: $data_races"
        log_error "  Deadlocks: $deadlocks"
        log_error "  Atomic violations: $atomic_violations"
        log_error "  Lock order violations: $lock_order_violations"

        # Show some error details
        echo "$output" | grep -A 5 "WARNING\|ERROR\|data race\|deadlock\|ThreadSanitizer\|WARNING: ThreadSanitizer" | head -20
        return 1
    else
        log_success "Test suite '$suite_name' completed without thread safety issues"
        return 0
    fi
}

# Generate comprehensive test report
generate_report() {
    log_info "Generating thread safety test report..."

    mkdir -p "$OUTPUT_DIR"

    # Generate JSON report
    "$EXECUTABLE" +set threadtest report "${OUTPUT_DIR}/thread_safety_report.json" 2>/dev/null

    # Export results for CI
    "$EXECUTABLE" +set threadtest export "${OUTPUT_DIR}/ci_export" 2>/dev/null

    # Show final statistics
    log_info "Final test statistics:"
    "$EXECUTABLE" +set threadtest status 2>/dev/null

    log_success "Reports generated in: $OUTPUT_DIR"
}

# Show usage information
show_usage() {
    echo "Thread Safety Test Runner"
    echo ""
    echo "This script runs ThreadSanitizer (TSan) thread safety tests on the idtech3 engine."
    echo ""
    echo "Usage: $0 [options] [test_type]"
    echo ""
    echo "Test Types:"
    echo "  basic         - Run basic thread safety tests"
    echo "  comprehensive - Run comprehensive test suite"
    echo "  race          - Run data race detection tests"
    echo "  deadlock      - Run deadlock detection tests"
    echo "  atomic        - Run atomic operation tests"
    echo "  stress        - Run stress tests"
    echo "  custom        - Run custom test (specify with CUSTOM_TESTS)"
    echo ""
    echo "Options:"
    echo "  --sanitizers SANITIZERS   Comma-separated list of sanitizers (default: tsan)"
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
    echo "  $0 --sanitizers tsan,deadlock comprehensive"
    echo "  $0 --strict race"
    echo ""
    echo "Prerequisites:"
    echo "  Build with ThreadSanitizer:"
    echo "    export CC=clang CXX=clang++"
    echo "    cmake .. -DCMAKE_C_FLAGS=\"-fsanitize=thread\" \\"
    echo "             -DCMAKE_CXX_FLAGS=\"-fsanitize=thread\" \\"
    echo "             -DCMAKE_EXE_LINKER_FLAGS=\"-fsanitize=thread\""
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
            log_info "Running basic thread safety tests..."
            run_test_suite "basic_thread_suite" "Basic Thread Safety Test Suite" \
                "data_race_basic" "data_race_atomic" "deadlock_mutex" "shared_data_structures"
            ;;

        "race")
            log_info "Running data race detection tests..."
            run_test_suite "race_suite" "Data Race Detection Test Suite" \
                "data_race_basic" "data_race_atomic" "use_after_free_concurrent" \
                "shared_data_structures" "memory_allocator" "thread_pool"
            ;;

        "deadlock")
            log_info "Running deadlock detection tests..."
            run_test_suite "deadlock_suite" "Deadlock Detection Test Suite" \
                "deadlock_mutex" "deadlock_rwlock" "lock_order_violation" \
                "double_lock" "unlock_unlocked_mutex"
            ;;

        "atomic")
            log_info "Running atomic operation tests..."
            run_test_suite "atomic_suite" "Atomic Operations Test Suite" \
                "data_race_atomic" "atomic_operations" "synchronization_primitives"
            ;;

        "stress")
            log_info "Running stress tests..."
            run_test_suite "stress_suite" "Thread Safety Stress Test Suite" \
                "high_contention" "long_running_threads" "frequent_context_switches" \
                "max_threads" "shared_data_structures"
            ;;

        "comprehensive")
            log_info "Running comprehensive thread safety test suite..."
            run_test_suite "comprehensive_thread_suite" "Comprehensive Thread Safety Test Suite" \
                "data_race_basic" "data_race_atomic" "deadlock_mutex" "deadlock_rwlock" \
                "lock_order_violation" "use_after_free_concurrent" "double_lock" \
                "unlock_unlocked_mutex" "condition_variable_race" "semaphore_race" \
                "memory_allocator" "shared_data_structures" "thread_pool" \
                "message_queues" "resource_management" "synchronization_primitives" \
                "lock_free_data_structures" "atomic_operations" "high_contention"
            ;;

        "custom")
            if [[ -z "$CUSTOM_TESTS" ]]; then
                log_error "CUSTOM_TESTS environment variable not set"
                log_info "Set CUSTOM_TESTS to a space-separated list of test names"
                exit 1
            fi

            log_info "Running custom thread safety tests: $CUSTOM_TESTS"
            run_test_suite "custom_thread_suite" "Custom Thread Safety Test Suite" $CUSTOM_TESTS
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
    log_info "Thread Safety Test Runner Starting"
    log_info "==================================="

    # Parse command line arguments
    parse_args "$@"

    # Check prerequisites
    check_executable

    # Enable ThreadSanitizer
    enable_tsan

    # Show TSan status
    log_info "ThreadSanitizer status:"
    "$EXECUTABLE" +set threadtest status 2>/dev/null

    # Run the tests
    local test_start_time=$(date +%s)
    if run_tests; then
        local test_end_time=$(date +%s)
        local test_duration=$((test_end_time - test_start_time))

        log_success "All thread safety tests completed successfully"
        log_info "Total test time: $test_duration seconds"

        # Generate reports
        if [[ "$GENERATE_REPORTS" == "true" ]]; then
            generate_report
        fi

        log_success "Thread safety testing completed successfully!"
        return 0
    else
        log_error "Thread safety tests detected issues!"
        return 1
    fi
}

# Run main function
main "$@"
