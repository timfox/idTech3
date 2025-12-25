#!/bin/bash

# Code Quality Analysis Runner Script
# This script runs automated code quality analysis and gate checking

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE="${BUILD_DIR}/idtech3_vulkan_x86_64"
OUTPUT_DIR="${PROJECT_ROOT}/code_quality_results"

# Default configuration
STRICT_MODE="${STRICT_MODE:-false}"
GENERATE_REPORTS="${GENERATE_REPORTS:-true}"
MIN_COVERAGE="${MIN_COVERAGE:-75.0}"
MAX_COMPLEXITY="${MAX_COMPLEXITY:-15}"
MIN_MAINTAINABILITY="${MIN_MAINTAINABILITY:-50.0}"
MAX_DUPLICATION="${MAX_DUPLICATION:-5.0}"

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
        log_info "  cmake .. -DENABLE_CODE_QUALITY_ANALYSIS=ON"
        log_info "  make -j$(nproc)"
        exit 1
    fi
}

# Configure quality gates
configure_gates() {
    log_info "Configuring quality gates..."

    # Set coverage gate
    "$EXECUTABLE" +set quality gate threshold minimum_coverage "$MIN_COVERAGE" -1 2>/dev/null || log_warning "Could not set coverage gate"

    # Set complexity gate
    "$EXECUTABLE" +set quality gate threshold maximum_complexity 0 "$MAX_COMPLEXITY" 2>/dev/null || log_warning "Could not set complexity gate"

    # Set maintainability gate
    "$EXECUTABLE" +set quality gate threshold maintainability_index "$MIN_MAINTAINABILITY" -1 2>/dev/null || log_warning "Could not set maintainability gate"

    # Set duplication gate
    "$EXECUTABLE" +set quality gate threshold code_duplication 0 "$MAX_DUPLICATION" 2>/dev/null || log_warning "Could not set duplication gate"

    # Set strict mode
    if [[ "$STRICT_MODE" == "true" ]]; then
        "$EXECUTABLE" +set quality strict on 2>/dev/null || log_warning "Could not enable strict mode"
    fi
}

# Run full quality analysis
run_full_analysis() {
    log_info "Running full code quality analysis..."

    local analysis_output
    analysis_output=$("$EXECUTABLE" +set quality analyze 2>&1)

    # Check for analysis success
    if echo "$analysis_output" | grep -q "analysis completed"; then
        log_success "Code quality analysis completed"

        # Extract key metrics
        local coverage complexity maintainability failed_gates

        coverage=$(echo "$analysis_output" | grep "Overall Coverage:" | grep -o "[0-9]\+\.[0-9]\+" || echo "0")
        max_complexity=$(echo "$analysis_output" | grep "Maximum Complexity:" | grep -o "[0-9]\+" || echo "0")
        maintainability=$(echo "$analysis_output" | grep "Maintainability Index:" | grep -o "[0-9]\+\.[0-9]\+" || echo "0")
        failed_gates=$(echo "$analysis_output" | grep -c "Failed Gates" || echo "0")

        log_info "Coverage: ${coverage}%"
        log_info "Max Complexity: $max_complexity"
        log_info "Maintainability: $maintainability"
        log_info "Failed Gates: $failed_gates"

        # Determine quality level
        local quality_level="unknown"
        if (( $(echo "$coverage >= 80" | bc -l 2>/dev/null || echo "0") )) && (( $(echo "$maintainability >= 50" | bc -l 2>/dev/null || echo "0") )) && [ "$max_complexity" -le 15 ]; then
            quality_level="excellent"
            log_success "Code Quality Level: EXCELLENT"
        elif (( $(echo "$coverage >= 70" | bc -l 2>/dev/null || echo "0") )) && (( $(echo "$maintainability >= 40" | bc -l 2>/dev/null || echo "0") )) && [ "$max_complexity" -le 20 ]; then
            quality_level="good"
            log_success "Code Quality Level: GOOD"
        elif (( $(echo "$coverage >= 60" | bc -l 2>/dev/null || echo "0") )) && (( $(echo "$maintainability >= 30" | bc -l 2>/dev/null || echo "0") )) && [ "$max_complexity" -le 25 ]; then
            quality_level="needs-improvement"
            log_warning "Code Quality Level: NEEDS IMPROVEMENT"
        else
            quality_level="poor"
            log_error "Code Quality Level: POOR"
        fi

        # Check gates
        if [ "$failed_gates" -gt 0 ]; then
            log_error "Quality gates failed: $failed_gates gate(s) not met"

            # Show failed gates
            echo "$analysis_output" | grep -A 10 "Failed Gates" | head -15

            if [[ "$STRICT_MODE" == "true" ]]; then
                log_error "Strict mode enabled - failing due to gate violations"
                return 1
            else
                log_warning "Non-strict mode - continuing despite gate violations"
            fi
        else
            log_success "All quality gates passed"
        fi

        return 0
    else
        log_error "Code quality analysis failed"
        echo "$analysis_output"
        return 1
    fi
}

# Run CI gate check
run_ci_check() {
    log_info "Running CI gate check..."

    local ci_output
    ci_output=$("$EXECUTABLE" +set quality ci-check 2>&1)

    if echo "$ci_output" | grep -q "CI Gate Check: PASS"; then
        log_success "CI gates passed"
        return 0
    else
        log_error "CI gates failed"
        echo "$ci_output" | grep -A 10 "Blocking Issues"
        return 1
    fi
}

# Generate reports
generate_reports() {
    log_info "Generating quality reports..."

    mkdir -p "$OUTPUT_DIR"

    # Generate JSON report
    "$EXECUTABLE" +set quality report "${OUTPUT_DIR}/quality_report.json" json 2>/dev/null || log_warning "Could not generate JSON report"

    # Generate HTML report (if supported)
    "$EXECUTABLE" +set quality report "${OUTPUT_DIR}/quality_report.html" html 2>/dev/null || log_warning "Could not generate HTML report"

    # Export for CI
    "$EXECUTABLE" +set quality export "${OUTPUT_DIR}/ci_export" 2>/dev/null || log_warning "Could not export CI results"

    log_success "Reports generated in: $OUTPUT_DIR"
}

# Run incremental analysis
run_incremental_analysis() {
    local files="$*"

    if [[ -z "$files" ]]; then
        log_error "No files specified for incremental analysis"
        return 1
    fi

    log_info "Running incremental analysis on: $files"

    local incremental_output
    incremental_output=$("$EXECUTABLE" +set quality incremental $files 2>&1)

    if echo "$incremental_output" | grep -q "incremental analysis completed"; then
        log_success "Incremental analysis completed"
        echo "$incremental_output"
        return 0
    else
        log_error "Incremental analysis failed"
        echo "$incremental_output"
        return 1
    fi
}

# Show usage information
show_usage() {
    echo "Code Quality Analysis Runner"
    echo ""
    echo "This script runs automated code quality analysis and gate checking."
    echo ""
    echo "Usage: $0 [options] [command]"
    echo ""
    echo "Commands:"
    echo "  analyze          - Run full code quality analysis"
    echo "  ci-check         - Run CI gate check (pass/fail only)"
    echo "  incremental <files> - Run incremental analysis on specific files"
    echo "  gates            - Show current quality gates"
    echo "  reports          - Generate quality reports"
    echo "  status           - Show system status"
    echo ""
    echo "Options:"
    echo "  --strict         - Enable strict mode (fail on any gate violation)"
    echo "  --no-reports     - Skip report generation"
    echo "  --min-coverage N - Set minimum coverage percentage (default: 75.0)"
    echo "  --max-complexity N - Set maximum complexity (default: 15)"
    echo "  --min-maintainability N - Set minimum maintainability (default: 50.0)"
    echo "  --max-duplication N - Set maximum duplication % (default: 5.0)"
    echo "  --help           - Show this help"
    echo ""
    echo "Environment Variables:"
    echo "  STRICT_MODE      - Same as --strict"
    echo "  MIN_COVERAGE     - Same as --min-coverage"
    echo "  MAX_COMPLEXITY   - Same as --max-complexity"
    echo "  MIN_MAINTAINABILITY - Same as --min-maintainability"
    echo "  MAX_DUPLICATION  - Same as --max-duplication"
    echo ""
    echo "Examples:"
    echo "  $0 analyze"
    echo "  $0 --strict ci-check"
    echo "  $0 incremental src/common/q_shared.c src/common/common.c"
    echo "  $0 --min-coverage 80 --max-complexity 10 analyze"
    echo ""
    echo "Prerequisites:"
    echo "  Build with code quality analysis enabled:"
    echo "    cmake .. -DENABLE_CODE_QUALITY_ANALYSIS=ON"
    echo "    make -j$(nproc)"
    echo ""
}

# Parse command line arguments
parse_args() {
    COMMAND="analyze"

    while [[ $# -gt 0 ]]; do
        case $1 in
            --strict)
                STRICT_MODE=true
                shift
                ;;
            --no-reports)
                GENERATE_REPORTS=false
                shift
                ;;
            --min-coverage)
                MIN_COVERAGE="$2"
                shift 2
                ;;
            --max-complexity)
                MAX_COMPLEXITY="$2"
                shift 2
                ;;
            --min-maintainability)
                MIN_MAINTAINABILITY="$2"
                shift 2
                ;;
            --max-duplication)
                MAX_DUPLICATION="$2"
                shift 2
                ;;
            --help)
                show_usage
                exit 0
                ;;
            analyze|ci-check|incremental|gates|reports|status)
                COMMAND="$1"
                shift
                break
                ;;
            -*)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
            *)
                COMMAND="$1"
                shift
                break
                ;;
        esac
    done

    # Remaining arguments are for incremental command
    INCREMENTAL_ARGS="$*"
}

# Execute command
execute_command() {
    case "$COMMAND" in
        "analyze")
            run_full_analysis && generate_reports_if_requested
            ;;
        "ci-check")
            run_ci_check
            ;;
        "incremental")
            if [[ -z "$INCREMENTAL_ARGS" ]]; then
                log_error "No files specified for incremental analysis"
                show_usage
                exit 1
            fi
            run_incremental_analysis $INCREMENTAL_ARGS
            ;;
        "gates")
            show_gates
            ;;
        "reports")
            generate_reports
            ;;
        "status")
            show_status
            ;;
        *)
            log_error "Unknown command: $COMMAND"
            show_usage
            exit 1
            ;;
    esac
}

# Generate reports if requested
generate_reports_if_requested() {
    if [[ "$GENERATE_REPORTS" == "true" ]]; then
        generate_reports
    fi
}

# Show current gates
show_gates() {
    log_info "Current quality gates:"
    "$EXECUTABLE" +set quality gates 2>/dev/null
}

# Show system status
show_status() {
    log_info "Code quality system status:"
    "$EXECUTABLE" +set quality status 2>/dev/null
}

# Main execution
main() {
    log_info "Code Quality Analysis Runner Starting"
    log_info "====================================="

    # Parse command line arguments
    parse_args "$@"

    # Check prerequisites
    check_executable

    # Configure quality gates
    configure_gates

    # Show status before analysis
    log_info "System status:"
    "$EXECUTABLE" +set quality status 2>/dev/null | head -10

    # Execute the requested command
    local start_time=$(date +%s)
    if execute_command; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        log_success "Code quality analysis completed successfully"
        log_info "Total time: $duration seconds"

        exit 0
    else
        log_error "Code quality analysis failed"
        exit 1
    fi
}

# Run main function
main "$@"
