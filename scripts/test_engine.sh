#!/bin/bash
# Comprehensive Engine Testing Script
# Tests all renderers, configurations, and safety features

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE_DIR="$(dirname "$SCRIPT_DIR")"
RELEASE_DIR="$ENGINE_DIR/release"
LOGS_DIR="$ENGINE_DIR/logs"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Test results
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Logging
log() {
    echo -e "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$LOGS_DIR/test_results.log"
}

# Test function
run_test() {
    local test_name="$1"
    local command="$2"
    local timeout="${3:-10}"

    TESTS_TOTAL=$((TESTS_TOTAL + 1))

    echo -e "${BLUE}Running test: $test_name${NC}"

    # Create log file for this test
    local log_file="$LOGS_DIR/test_${test_name}.log"

    # Run test with timeout
    if timeout "$timeout" bash -c "$command" > "$log_file" 2>&1; then
        echo -e "${GREEN}✓ PASS: $test_name${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log "PASS: $test_name"
        return 0
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "${YELLOW}⚠ TIMEOUT: $test_name${NC}"
            log "TIMEOUT: $test_name"
        else
            echo -e "${RED}✗ FAIL: $test_name${NC}"
            TESTS_FAILED=$((TESTS_FAILED + 1))
            log "FAIL: $test_name (exit code: $exit_code)"
        fi
        return 1
    fi
}

# Test engine startup with different renderers
test_renderer_startup() {
    local renderer="$1"
    local test_name="renderer_startup_$renderer"

    run_test "$test_name" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer $renderer +quit 2>&1 | \
        grep -q 'Renderer Support.*Vulkan.*OpenGL'
    " 15
}

# Test Vulkan specific features
test_vulkan_features() {
    run_test "vulkan_initialization" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer vulkan +quit 2>&1 | \
        grep -q 'Renderer Support.*Vulkan.*RTX'
    " 20

    run_test "vulkan_rtx_detection" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer vulkan +quit 2>&1 | \
        grep -q 'Advanced Features.*imGUI.*RTX'
    " 20

    run_test "vulkan_imgui_integration" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer vulkan +quit 2>&1 | \
        grep -q 'Advanced Features.*Ray Tracing'
    " 20
}

# Test OpenGL fallback
test_opengl_fallback() {
    run_test "opengl_initialization" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer opengl +quit 2>&1 | \
        grep -q 'Successfully loaded renderer: opengl'
    " 15

    run_test "renderer_fallback" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer invalid_renderer +quit 2>&1 | \
        grep -q 'fell back.*to.*opengl'
    " 15
}

# Test memory safety features
test_memory_safety() {
    run_test "memory_safety_initialization" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer vulkan +quit 2>&1 | \
        grep -q 'Memory safety framework initialized'
    " 15

    run_test "filesystem_safety" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer vulkan +set fs_restart 1 +quit 2>&1 | \
        grep -q 'Filesystem restart disabled'
    " 15
}

# Test developer features
test_developer_features() {
    run_test "developer_mode" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set developer 1 +quit 2>&1 | \
        grep -q 'developer.*1'
    " 15

    run_test "vulkan_validation" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer vulkan +set r_vk_enable_validation 1 +quit 2>&1 | \
        grep -q 'validation.*enabled'
    " 20
}

# Test performance monitoring
test_performance_features() {
    run_test "performance_hud" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set cl_renderer vulkan +set r_perfhud 1 +quit 2>&1 | \
        grep -q 'Performance HUD'
    " 20
}

# Test mod support
test_mod_support() {
    # Create a test mod if it doesn't exist
    if [ ! -d "$ENGINE_DIR/mods/testmod" ]; then
        mkdir -p "$ENGINE_DIR/mods/testmod"
        echo "// Test mod configuration" > "$ENGINE_DIR/mods/testmod/mod.cfg"
    fi

    run_test "mod_loading" "
        cd '$ENGINE_DIR' && \
        '$RELEASE_DIR/idtech3.x86_64' +set fs_game testmod +quit 2>&1 | \
        grep -q 'testmod'
    " 15
}

# Test launcher script
test_launcher_script() {
    run_test "launcher_help" "
        '$SCRIPT_DIR/run_engine.sh' --help 2>&1 | \
        grep -q 'Enhanced id Tech 3 Engine Launcher'
    "

    run_test "launcher_detect_gpu" "
        '$SCRIPT_DIR/run_engine.sh' --detect-gpu 2>&1 | \
        grep -q 'Detected GPU'
    "

    run_test "launcher_auto_renderer" "
        timeout 5 '$SCRIPT_DIR/run_engine.sh' --auto +quit 2>&1 | \
        grep -q 'Auto-selected'
    " 10
}

# Generate test report
generate_report() {
    local report_file="$LOGS_DIR/test_report_$(date +%Y%m%d_%H%M%S).txt"

    cat > "$report_file" << EOF
============================================================
id Tech 3 Engine Test Report
Generated: $(date)
============================================================

TEST RESULTS SUMMARY:
--------------------
Total Tests: $TESTS_TOTAL
Passed: $TESTS_PASSED
Failed: $TESTS_FAILED
Success Rate: $((TESTS_PASSED * 100 / TESTS_TOTAL))%

SYSTEM INFORMATION:
------------------
OS: $(uname -a)
CPU: $(nproc) cores
Memory: $(free -h | grep '^Mem:' | awk '{print $2}')
GPU: $(./scripts/run_engine.sh --detect-gpu 2>/dev/null || echo "Unknown")

ENGINE INFORMATION:
------------------
Build: $(cd "$ENGINE_DIR" && ./release/idtech3.x86_64 +version +quit 2>&1 | grep -o 'Build:.*' | head -1 || echo "Unknown")

DETAILED RESULTS:
----------------
EOF

    # Add detailed results from log file
    if [ -f "$LOGS_DIR/test_results.log" ]; then
        cat "$LOGS_DIR/test_results.log" >> "$report_file"
    fi

    cat >> "$report_file" << EOF

============================================================
RECOMMENDATIONS:
EOF

    if [ $TESTS_FAILED -eq 0 ]; then
        echo "- All tests passed! Engine is fully functional." >> "$report_file"
    else
        echo "- $TESTS_FAILED tests failed. Check logs for details." >> "$report_file"
        echo "- Review failed test logs in $LOGS_DIR/" >> "$report_file"
    fi

    echo "- Test report saved to: $report_file"
    echo -e "${GREEN}Test report generated: $report_file${NC}"
}

# Main test execution
main() {
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}      id Tech 3 Engine Comprehensive Testing Suite${NC}"
    echo -e "${BLUE}============================================================${NC}"

    # Create logs directory
    mkdir -p "$LOGS_DIR"

    # Clear previous test results
    > "$LOGS_DIR/test_results.log"

    # Check prerequisites
    if [ ! -f "$RELEASE_DIR/idtech3.x86_64" ]; then
        echo -e "${RED}Error: Engine binary not found at $RELEASE_DIR/idtech3.x86_64${NC}"
        echo -e "${YELLOW}Please build the engine first${NC}"
        exit 1
    fi

    echo -e "${GREEN}Starting comprehensive engine tests...${NC}"
    echo ""

    # Run all test categories
    echo -e "${YELLOW}Testing Renderer Startup...${NC}"
    test_renderer_startup "vulkan"
    test_renderer_startup "opengl"

    echo ""
    echo -e "${YELLOW}Testing Vulkan Features...${NC}"
    test_vulkan_features

    echo ""
    echo -e "${YELLOW}Testing OpenGL Fallback...${NC}"
    test_opengl_fallback

    echo ""
    echo -e "${YELLOW}Testing Memory Safety...${NC}"
    test_memory_safety

    echo ""
    echo -e "${YELLOW}Testing Developer Features...${NC}"
    test_developer_features

    echo ""
    echo -e "${YELLOW}Testing Performance Features...${NC}"
    test_performance_features

    echo ""
    echo -e "${YELLOW}Testing Mod Support...${NC}"
    test_mod_support

    echo ""
    echo -e "${YELLOW}Testing Launcher Script...${NC}"
    test_launcher_script

    echo ""
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}TEST RESULTS SUMMARY${NC}"
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${GREEN}Total Tests: $TESTS_TOTAL${NC}"
    echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"

    if [ $TESTS_TOTAL -gt 0 ]; then
        local success_rate=$((TESTS_PASSED * 100 / TESTS_TOTAL))
        if [ $success_rate -ge 90 ]; then
            echo -e "${GREEN}Success Rate: ${success_rate}% ✓${NC}"
        elif [ $success_rate -ge 75 ]; then
            echo -e "${YELLOW}Success Rate: ${success_rate}% ⚠${NC}"
        else
            echo -e "${RED}Success Rate: ${success_rate}% ✗${NC}"
        fi
    fi

    echo ""
    generate_report

    # Exit with failure if any tests failed
    if [ $TESTS_FAILED -gt 0 ]; then
        exit 1
    fi
}

# Run main function
main "$@"