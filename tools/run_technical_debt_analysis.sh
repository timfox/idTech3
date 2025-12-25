#!/bin/bash

# Technical Debt Analysis Runner Script
# This script runs automated technical debt tracking and analysis

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
EXECUTABLE="${BUILD_DIR}/idtech3_vulkan_x86_64"
OUTPUT_DIR="${PROJECT_ROOT}/technical_debt_results"

# Default configuration
AUTO_TRACKING="${AUTO_TRACKING:-true}"
ALERTS_ENABLED="${ALERTS_ENABLED:-true}"
CRITICAL_THRESHOLD="${CRITICAL_THRESHOLD:-10}"
HIGH_THRESHOLD="${HIGH_THRESHOLD:-25}"

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
        log_info "  cmake .. -DENABLE_TECHNICAL_DEBT_TRACKING=ON"
        log_info "  make -j$(nproc)"
        exit 1
    fi
}

# Configure debt tracking
configure_debt_tracking() {
    log_info "Configuring technical debt tracking..."

    # Set alert thresholds
    "$EXECUTABLE" +set debt thresholds "$CRITICAL_THRESHOLD" "$HIGH_THRESHOLD" 2>/dev/null || log_warning "Could not set alert thresholds"

    # Configure auto tracking
    if [[ "$AUTO_TRACKING" == "true" ]]; then
        "$EXECUTABLE" +set debt auto on 2>/dev/null || log_warning "Could not enable auto tracking"
    fi

    # Configure alerts
    if [[ "$ALERTS_ENABLED" == "true" ]]; then
        # Note: alerts command conflicts with alerts checking
        # This would need to be handled differently in a real implementation
        log_info "Alerts enabled"
    fi
}

# Run comprehensive debt analysis
run_comprehensive_analysis() {
    log_info "Running comprehensive technical debt analysis..."

    local analysis_output
    analysis_output=$("$EXECUTABLE" +set debt analyze 2>&1)

    # Check for analysis success
    if echo "$analysis_output" | grep -q "Automated technical debt analysis completed"; then
        log_success "Technical debt analysis completed"

        # Extract key metrics
        local debt_score unresolved_items critical_items velocity

        debt_score=$(echo "$analysis_output" | grep "Current debt score:" | grep -o "[0-9.]\+" || echo "0")
        unresolved_items=$(echo "$analysis_output" | grep "unresolved items" | grep -o "[0-9]\+" | head -1 || echo "0")
        critical_items=$(echo "$analysis_output" | grep "Critical Items:" | grep -o "[0-9]\+" || echo "0")
        velocity=$(echo "$analysis_output" | grep "Debt Velocity:" | grep -o "[0-9.-]\+" || echo "0")

        log_info "Debt Score: ${debt_score}"
        log_info "Unresolved Items: $unresolved_items"
        log_info "Critical Items: $critical_items"
        log_info "Debt Velocity: $velocity points/day"

        # Determine health status
        local health_status="unknown"
        if [ "$critical_items" -gt "5" ] || (( $(echo "$debt_score > 300" | bc -l 2>/dev/null || echo "0") )) || (( $(echo "$velocity > 3" | bc -l 2>/dev/null || echo "0") )); then
            health_status="unhealthy"
            log_error "Debt Health: UNHEALTHY 🚨"
        elif [ "$critical_items" -gt "2" ] || (( $(echo "$debt_score > 150" | bc -l 2>/dev/null || echo "0") )) || (( $(echo "$velocity > 1" | bc -l 2>/dev/null || echo "0") )); then
            health_status="concerning"
            log_warning "Debt Health: CONCERNING ⚠️"
        elif (( $(echo "$debt_score > 50" | bc -l 2>/dev/null || echo "0") )) || [ "$unresolved_items" -gt "10" ]; then
            health_status="moderate"
            log_warning "Debt Health: MODERATE 📊"
        else
            health_status="healthy"
            log_success "Debt Health: HEALTHY ✅"
        fi

        # Check for alerts
        local alerts_output
        alerts_output=$("$EXECUTABLE" +set debt alerts 2>&1)

        if echo "$alerts_output" | grep -q "Technical Debt Alerts"; then
            log_warning "Debt alerts detected:"
            echo "$alerts_output" | grep -A 5 "Alerts:" | head -10
        else
            log_success "No debt alerts at this time"
        fi

        return 0
    else
        log_error "Technical debt analysis failed"
        echo "$analysis_output"
        return 1
    fi
}

# Generate reports
generate_reports() {
    log_info "Generating technical debt reports..."

    mkdir -p "$OUTPUT_DIR"

    # Generate JSON report
    "$EXECUTABLE" +set debt report "${OUTPUT_DIR}/debt_report.json" json 2>/dev/null || log_warning "Could not generate JSON report"

    # Generate HTML report (if supported)
    "$EXECUTABLE" +set debt report "${OUTPUT_DIR}/debt_report.html" html 2>/dev/null || log_warning "Could not generate HTML report"

    # Export for CI
    "$EXECUTABLE" +set debt export "${OUTPUT_DIR}/ci_export" 2>/dev/null || log_warning "Could not export CI results"

    log_success "Reports generated in: $OUTPUT_DIR"
}

# Run trend analysis
run_trend_analysis() {
    local days="${1:-30}"
    log_info "Analyzing debt trends over last $days days..."

    local trends_output
    trends_output=$("$EXECUTABLE" +set debt trends "$days" 2>&1)

    if echo "$trends_output" | grep -q "Debt Trends"; then
        log_success "Trend analysis completed"
        echo "$trends_output"

        # Extract trend indicators
        local velocity paydown_rate
        velocity=$(echo "$trends_output" | grep "Debt Velocity:" | grep -o "[0-9.-]\+" || echo "0")
        paydown_rate=$(echo "$trends_output" | grep "Paydown Rate:" | grep -o "[0-9.-]\+" || echo "0")

        # Analyze trends
        if (( $(echo "$velocity > 2.0" | bc -l 2>/dev/null || echo "0") )); then
            log_error "CRITICAL TREND: Debt is accumulating rapidly ($velocity points/day)"
        elif (( $(echo "$velocity > 0.5" | bc -l 2>/dev/null || echo "0") )); then
            log_warning "WARNING TREND: Debt is slowly increasing ($velocity points/day)"
        elif (( $(echo "$paydown_rate > 0.1" | bc -l 2>/dev/null || echo "0") )); then
            log_success "POSITIVE TREND: Debt is being reduced ($paydown_rate items/day)"
        else
            log_info "STABLE TREND: Debt level is stable"
        fi
    else
        log_error "Trend analysis failed or insufficient data"
        echo "$trends_output"
    fi
}

# Run debt prediction
run_debt_prediction() {
    local months="${1:-3}"
    log_info "Predicting debt levels $months months into the future..."

    local predict_output
    predict_output=$("$EXECUTABLE" +set debt predict "$months" 2>&1)

    if echo "$predict_output" | grep -q "Projected debt score"; then
        log_success "Debt prediction completed"
        echo "$predict_output"
    else
        log_error "Debt prediction failed"
        echo "$predict_output"
    fi
}

# Show debt dashboard
show_dashboard() {
    log_info "Technical Debt Dashboard"
    echo "========================"

    # Show status
    "$EXECUTABLE" +set debt status 2>/dev/null | head -15

    echo ""
    echo "Current Metrics:"
    "$EXECUTABLE" +set debt metrics 2>/dev/null | head -10

    echo ""
    echo "Recent History:"
    "$EXECUTABLE" +set debt history 2>/dev/null | head -10

    echo ""
    echo "Active Alerts:"
    "$EXECUTABLE" +set debt alerts 2>/dev/null
}

# Add sample debt items for demonstration
add_sample_debt() {
    log_info "Adding sample technical debt items for demonstration..."

    # Add some example debt items
    "$EXECUTABLE" +set debt add "legacy_code_refactor" "Refactor legacy rendering code" quality high 2>/dev/null
    "$EXECUTABLE" +set debt add "memory_leak_fix" "Fix memory leak in texture loading" security critical 2>/dev/null
    "$EXECUTABLE" +set debt add "complexity_reduction" "Reduce cyclomatic complexity in main loop" complexity medium 2>/dev/null
    "$EXECUTABLE" +set debt add "test_coverage" "Improve unit test coverage" coverage medium 2>/dev/null

    log_success "Sample debt items added"
}

# List debt items
list_debt_items() {
    log_info "Listing technical debt items..."
    "$EXECUTABLE" +set debt list 2>/dev/null
}

# Show usage information
show_usage() {
    echo "Technical Debt Analysis Runner"
    echo ""
    echo "This script runs automated technical debt tracking and analysis."
    echo ""
    echo "Usage: $0 [options] [command]"
    echo ""
    echo "Commands:"
    echo "  analyze          - Run comprehensive debt analysis"
    echo "  trends [days]    - Analyze debt trends (default: 30 days)"
    echo "  predict [months] - Predict future debt levels (default: 3 months)"
    echo "  dashboard        - Show debt dashboard"
    echo "  reports          - Generate debt reports"
    echo "  add-sample       - Add sample debt items for testing"
    echo "  list             - List all debt items"
    echo "  status           - Show system status"
    echo ""
    echo "Options:"
    echo "  --auto-tracking     - Enable automatic debt tracking"
    echo "  --no-alerts         - Disable debt alerts"
    echo "  --critical-threshold N - Set critical alert threshold (default: 10)"
    echo "  --high-threshold N    - Set high alert threshold (default: 25)"
    echo "  --help               - Show this help"
    echo ""
    echo "Environment Variables:"
    echo "  AUTO_TRACKING       - Same as --auto-tracking"
    echo "  ALERTS_ENABLED      - Same as --no-alerts (inverted)"
    echo "  CRITICAL_THRESHOLD  - Same as --critical-threshold"
    echo "  HIGH_THRESHOLD      - Same as --high-threshold"
    echo ""
    echo "Examples:"
    echo "  $0 analyze"
    echo "  $0 --auto-tracking trends 60"
    echo "  $0 predict 6"
    echo "  $0 add-sample && $0 analyze"
    echo ""
    echo "Prerequisites:"
    echo "  Build with technical debt tracking enabled:"
    echo "    cmake .. -DENABLE_TECHNICAL_DEBT_TRACKING=ON"
    echo "    make -j$(nproc)"
    echo ""
}

# Parse command line arguments
parse_args() {
    COMMAND="analyze"

    while [[ $# -gt 0 ]]; do
        case $1 in
            --auto-tracking)
                AUTO_TRACKING=true
                shift
                ;;
            --no-alerts)
                ALERTS_ENABLED=false
                shift
                ;;
            --critical-threshold)
                CRITICAL_THRESHOLD="$2"
                shift 2
                ;;
            --high-threshold)
                HIGH_THRESHOLD="$2"
                shift 2
                ;;
            --help)
                show_usage
                exit 0
                ;;
            analyze|trends|predict|dashboard|reports|add-sample|list|status)
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

    # Remaining arguments are for commands that take parameters
    COMMAND_ARGS="$*"
}

# Execute command
execute_command() {
    case "$COMMAND" in
        "analyze")
            run_comprehensive_analysis && generate_reports_if_requested
            ;;
        "trends")
            run_trend_analysis "${COMMAND_ARGS:-30}"
            ;;
        "predict")
            run_debt_prediction "${COMMAND_ARGS:-3}"
            ;;
        "dashboard")
            show_dashboard
            ;;
        "reports")
            generate_reports
            ;;
        "add-sample")
            add_sample_debt
            ;;
        "list")
            list_debt_items
            ;;
        "status")
            "$EXECUTABLE" +set debt status 2>/dev/null
            ;;
        *)
            log_error "Unknown command: $COMMAND"
            show_usage
            exit 1
            ;;
    esac
}

# Generate reports if requested (for analyze command)
generate_reports_if_requested() {
    # Always generate reports for analyze command
    generate_reports
}

# Main execution
main() {
    log_info "Technical Debt Analysis Runner Starting"
    log_info "======================================="

    # Parse command line arguments
    parse_args "$@"

    # Check prerequisites
    check_executable

    # Configure debt tracking
    configure_debt_tracking

    # Show initial status
    log_info "System status:"
    "$EXECUTABLE" +set debt status 2>/dev/null | head -10

    # Execute the requested command
    local start_time=$(date +%s)
    if execute_command; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        log_success "Technical debt analysis completed successfully"
        log_info "Total time: $duration seconds"

        exit 0
    else
        log_error "Technical debt analysis failed"
        exit 1
    fi
}

# Run main function
main "$@"
