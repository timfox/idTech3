# Build Optimization Analysis Script
# Analyzes build results for performance, size, and optimization effectiveness

# Get current time for duration calculation
STRING(TIMESTAMP BUILD_END_TIME "%s" UTC)

# Calculate build duration
IF(DEFINED BUILD_START_TIME)
    MATH(EXPR BUILD_DURATION "${BUILD_END_TIME} - ${BUILD_START_TIME}")
ELSE()
    SET(BUILD_DURATION "unknown")
ENDIF()

# Analyze target file
IF(DEFINED TARGET_FILE)
    # Get file size
    FILE(SIZE ${TARGET_FILE} FILE_SIZE_BYTES)

    # Convert to human readable format
    IF(FILE_SIZE_BYTES LESS 1024)
        SET(FILE_SIZE "${FILE_SIZE_BYTES} B")
    ELSEIF(FILE_SIZE_BYTES LESS 1048576)
        MATH(EXPR FILE_SIZE_KB "${FILE_SIZE_BYTES} / 1024")
        SET(FILE_SIZE "${FILE_SIZE_KB} KB")
    ELSE()
        MATH(EXPR FILE_SIZE_MB "${FILE_SIZE_BYTES} / 1048576")
        SET(FILE_SIZE "${FILE_SIZE_MB} MB")
    ENDIF()

    # Get file information
    GET_FILENAME_COMPONENT(TARGET_NAME ${TARGET_FILE} NAME)
    GET_FILENAME_COMPONENT(TARGET_DIR ${TARGET_FILE} DIRECTORY)

    # Analyze binary sections (if available)
    FIND_PROGRAM(READELF_EXECUTABLE readelf)
    FIND_PROGRAM(OBJDUMP_EXECUTABLE objdump)
    FIND_PROGRAM(SIZE_EXECUTABLE size)

    IF(SIZE_EXECUTABLE)
        EXECUTE_PROCESS(
            COMMAND ${SIZE_EXECUTABLE} ${TARGET_FILE}
            OUTPUT_VARIABLE SIZE_OUTPUT
            ERROR_QUIET
        )
    ENDIF()

    IF(READELF_EXECUTABLE)
        # Get section information
        EXECUTE_PROCESS(
            COMMAND ${READELF_EXECUTABLE} -S ${TARGET_FILE}
            OUTPUT_VARIABLE SECTION_INFO
            ERROR_QUIET
        )

        # Count sections
        STRING(REGEX MATCHALL "\\.text|\\.data|\\.bss|\\.rodata" SECTIONS "${SECTION_INFO}")
        LIST(LENGTH SECTIONS SECTION_COUNT)
    ENDIF()

    # Generate optimization report
    SET(REPORT_FILE "${CMAKE_BINARY_DIR}/build_optimization_report.txt")
    FILE(WRITE ${REPORT_FILE}
        "Build Optimization Report\n"
        "=========================\n"
        "Target: ${TARGET_NAME}\n"
        "Location: ${TARGET_DIR}\n"
        "Build Time: ${BUILD_DURATION} seconds\n"
        "File Size: ${FILE_SIZE} (${FILE_SIZE_BYTES} bytes)\n"
        "Timestamp: ${BUILD_END_TIME}\n"
        "\n"
    )

    # Add size analysis
    IF(SIZE_OUTPUT)
        FILE(APPEND ${REPORT_FILE} "Size Analysis:\n${SIZE_OUTPUT}\n\n")
    ENDIF()

    # Add section analysis
    IF(SECTION_INFO)
        FILE(APPEND ${REPORT_FILE} "Section Analysis:\n")
        FILE(APPEND ${REPORT_FILE} "Total sections: ${SECTION_COUNT}\n\n")

        # Extract key sections
        STRING(REGEX MATCH ".*\\.text.*" TEXT_SECTION "${SECTION_INFO}")
        STRING(REGEX MATCH ".*\\.data.*" DATA_SECTION "${SECTION_INFO}")
        STRING(REGEX MATCH ".*\\.bss.*" BSS_SECTION "${SECTION_INFO}")
        STRING(REGEX MATCH ".*\\.rodata.*" RODATA_SECTION "${SECTION_INFO}")

        IF(TEXT_SECTION)
            FILE(APPEND ${REPORT_FILE} "Text section found (code)\n")
        ENDIF()
        IF(DATA_SECTION)
            FILE(APPEND ${REPORT_FILE} "Data section found (initialized data)\n")
        ENDIF()
        IF(BSS_SECTION)
            FILE(APPEND ${REPORT_FILE} "BSS section found (uninitialized data)\n")
        ENDIF()
        IF(RODATA_SECTION)
            FILE(APPEND ${REPORT_FILE} "ROData section found (read-only data)\n")
        ENDIF()

        FILE(APPEND ${REPORT_FILE} "\n")
    ENDIF()

    # Performance recommendations
    FILE(APPEND ${REPORT_FILE} "Optimization Recommendations:\n")

    IF(FILE_SIZE_BYTES GREATER 10485760) # > 10MB
        FILE(APPEND ${REPORT_FILE} "- Consider enabling LTO for better optimization\n")
        FILE(APPEND ${REPORT_FILE} "- Binary size is large, consider dead code elimination\n")
    ELSEIF(FILE_SIZE_BYTES GREATER 5242880) # > 5MB
        FILE(APPEND ${REPORT_FILE} "- Binary size moderate, LTO would help\n")
    ELSE()
        FILE(APPEND ${REPORT_FILE} "- Binary size optimized\n")
    ENDIF()

    IF(BUILD_DURATION GREATER 300) # > 5 minutes
        FILE(APPEND ${REPORT_FILE} "- Build time is high, consider ccache or incremental builds\n")
        FILE(APPEND ${REPORT_FILE} "- Consider ThinLTO for faster builds\n")
    ELSEIF(BUILD_DURATION GREATER 60) # > 1 minute
        FILE(APPEND ${REPORT_FILE} "- Build time moderate, ccache would help\n")
    ELSE()
        FILE(APPEND ${REPORT_FILE} "- Build time optimized\n")
    ENDIF()

    FILE(APPEND ${REPORT_FILE} "\nReport generated: ${BUILD_END_TIME}\n")

    # Display summary
    MESSAGE(STATUS "Build optimization analysis complete:")
    MESSAGE(STATUS "  Target: ${TARGET_NAME}")
    MESSAGE(STATUS "  Size: ${FILE_SIZE}")
    MESSAGE(STATUS "  Build time: ${BUILD_DURATION} seconds")
    MESSAGE(STATUS "  Report: ${REPORT_FILE}")

ELSE()
    MESSAGE(WARNING "TARGET_FILE not defined for build analysis")
ENDIF()

# Save build statistics for trend analysis
SET(STATS_FILE "${CMAKE_BINARY_DIR}/build_stats.csv")
FILE(APPEND ${STATS_FILE} "${BUILD_END_TIME},${BUILD_DURATION},${FILE_SIZE_BYTES}\n")

# Create summary for CI/CD
SET(SUMMARY_FILE "${CMAKE_BINARY_DIR}/build_summary.json")
FILE(WRITE ${SUMMARY_FILE}
    "{\n"
    "  \"timestamp\": ${BUILD_END_TIME},\n"
    "  \"duration_seconds\": ${BUILD_DURATION},\n"
    "  \"size_bytes\": ${FILE_SIZE_BYTES},\n"
    "  \"target\": \"${TARGET_NAME}\"\n"
    "}\n"
)
