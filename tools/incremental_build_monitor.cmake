# Incremental Build Monitor CMake Script
# This script provides build timing and incremental build monitoring capabilities

# Function to start build timing
function(incremental_build_start)
    # Get current time
    string(TIMESTAMP BUILD_START_TIME "%s" UTC)
    set(BUILD_START_TIME ${BUILD_START_TIME} PARENT_SCOPE)

    # Detect if this is an incremental build by checking for existing build artifacts
    if(EXISTS "${CMAKE_BINARY_DIR}/CMakeCache.txt")
        set(IS_INCREMENTAL_BUILD TRUE PARENT_SCOPE)
    else()
        set(IS_INCREMENTAL_BUILD FALSE PARENT_SCOPE)
    endif()

    # Write build start information
    file(WRITE "${CMAKE_BINARY_DIR}/build_start.info"
        "BUILD_START_TIME=${BUILD_START_TIME}\n"
        "IS_INCREMENTAL=${IS_INCREMENTAL_BUILD}\n"
        "CMAKE_GENERATOR=${CMAKE_GENERATOR}\n"
        "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}\n"
    )

    message(STATUS "Build started at ${BUILD_START_TIME} (incremental: ${IS_INCREMENTAL_BUILD})")
endfunction()

# Function to end build timing and generate report
function(incremental_build_end)
    # Read build start information
    if(EXISTS "${CMAKE_BINARY_DIR}/build_start.info")
        file(READ "${CMAKE_BINARY_DIR}/build_start.info" BUILD_START_INFO)

        # Parse start time and incremental flag
        string(REGEX MATCH "BUILD_START_TIME=([0-9]+)" _ ${BUILD_START_INFO})
        set(BUILD_START_TIME ${CMAKE_MATCH_1})

        string(REGEX MATCH "IS_INCREMENTAL=([^\\n]+)" _ ${BUILD_START_INFO})
        set(IS_INCREMENTAL ${CMAKE_MATCH_1})

        # Calculate build duration
        string(TIMESTAMP BUILD_END_TIME "%s" UTC)
        math(EXPR BUILD_DURATION "${BUILD_END_TIME} - ${BUILD_START_TIME}")

        # Count source files
        file(GLOB_RECURSE SRC_FILES
            "${CMAKE_SOURCE_DIR}/src/*.c"
            "${CMAKE_SOURCE_DIR}/src/*.cpp"
            "${CMAKE_SOURCE_DIR}/src/*.cc"
        )
        list(LENGTH SRC_FILES TOTAL_SRC_FILES)

        # Generate build report
        file(WRITE "${CMAKE_BINARY_DIR}/build_report.txt"
            "Build Report\n"
            "============\n"
            "Start Time: ${BUILD_START_TIME}\n"
            "End Time: ${BUILD_END_TIME}\n"
            "Duration: ${BUILD_DURATION} seconds\n"
            "Incremental: ${IS_INCREMENTAL}\n"
            "Generator: ${CMAKE_GENERATOR}\n"
            "Build Type: ${CMAKE_BUILD_TYPE}\n"
            "Source Files: ${TOTAL_SRC_FILES}\n"
            "Build Directory: ${CMAKE_BINARY_DIR}\n"
            "\n"
        )

        # Display build summary
        if(IS_INCREMENTAL)
            message(STATUS "Incremental build completed in ${BUILD_DURATION} seconds")
        else()
            message(STATUS "Clean build completed in ${BUILD_DURATION} seconds")
        endif()

        # Clean up
        file(REMOVE "${CMAKE_BINARY_DIR}/build_start.info")
    endif()
endfunction()

# Function to analyze build dependencies
function(analyze_dependencies)
    message(STATUS "Analyzing build dependencies...")

    # Find all source files
    file(GLOB_RECURSE SRC_FILES
        "${CMAKE_SOURCE_DIR}/src/*.c"
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.cc"
    )

    # Find all header files
    file(GLOB_RECURSE HDR_FILES
        "${CMAKE_SOURCE_DIR}/src/*.h"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
    )

    # Count dependencies for each source file
    foreach(SRC_FILE ${SRC_FILES})
        get_filename_component(SRC_NAME ${SRC_FILE} NAME_WE)
        file(READ ${SRC_FILE} SRC_CONTENT)

        set(DEP_COUNT 0)
        foreach(HDR_FILE ${HDR_FILES})
            get_filename_component(HDR_NAME ${HDR_FILE} NAME)
            string(FIND "${SRC_CONTENT}" "#include \"${HDR_NAME}\"" INCLUDE_POS)
            if(NOT ${INCLUDE_POS} EQUAL -1)
                math(EXPR DEP_COUNT "${DEP_COUNT} + 1")
            endif()
        endforeach()

        # Write dependency info
        file(APPEND "${CMAKE_BINARY_DIR}/dependency_analysis.txt"
            "${SRC_NAME}: ${DEP_COUNT} dependencies\n"
        )
    endforeach()

    list(LENGTH SRC_FILES SRC_COUNT)
    list(LENGTH HDR_FILES HDR_COUNT)

    file(APPEND "${CMAKE_BINARY_DIR}/dependency_analysis.txt"
        "\nSummary:\n"
        "Source files: ${SRC_COUNT}\n"
        "Header files: ${HDR_COUNT}\n"
    )

    message(STATUS "Dependency analysis complete: ${SRC_COUNT} sources, ${HDR_COUNT} headers")
endfunction()

# Function to check for modified files since last build
function(check_modified_files)
    # Check if we have a previous build timestamp
    if(EXISTS "${CMAKE_BINARY_DIR}/last_build_timestamp")
        file(READ "${CMAKE_BINARY_DIR}/last_build_timestamp" LAST_BUILD_TIME)

        # Find recently modified source files
        file(GLOB_RECURSE ALL_SRC_FILES
            "${CMAKE_SOURCE_DIR}/src/*.c"
            "${CMAKE_SOURCE_DIR}/src/*.cpp"
            "${CMAKE_SOURCE_DIR}/src/*.cc"
            "${CMAKE_SOURCE_DIR}/src/*.h"
            "${CMAKE_SOURCE_DIR}/src/*.hpp"
        )

        set(MODIFIED_COUNT 0)
        foreach(SRC_FILE ${ALL_SRC_FILES})
            file(TIMESTAMP ${SRC_FILE} FILE_TIME "%s")
            if(FILE_TIME GREATER LAST_BUILD_TIME)
                math(EXPR MODIFIED_COUNT "${MODIFIED_COUNT} + 1")
                # Optional: list modified files
                # get_filename_component(FILE_NAME ${SRC_FILE} NAME)
                # message(STATUS "Modified: ${FILE_NAME}")
            endif()
        endforeach()

        message(STATUS "Files modified since last build: ${MODIFIED_COUNT}")
    else()
        message(STATUS "No previous build timestamp found (first build)")
    endif()

    # Update timestamp for next build
    string(TIMESTAMP CURRENT_TIME "%s" UTC)
    file(WRITE "${CMAKE_BINARY_DIR}/last_build_timestamp" ${CURRENT_TIME})
endfunction()

# Function to measure and display build performance
function(display_build_performance)
    # Check if time command is available
    find_program(TIME_CMD time)
    if(TIME_CMD)
        # This would be used in the build command to measure time
        message(STATUS "Build performance monitoring available")
    endif()

    # Display system information
    cmake_host_system_information(RESULT PROCESSOR_COUNT QUERY NUMBER_OF_PHYSICAL_CORES)
    cmake_host_system_information(RESULT LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
    cmake_host_system_information(RESULT TOTAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)

    message(STATUS "Build System Info:")
    message(STATUS "  Physical cores: ${PROCESSOR_COUNT}")
    message(STATUS "  Logical cores: ${LOGICAL_CORES}")
    message(STATUS "  Total memory: ${TOTAL_MEMORY} MB")
endfunction()

# Main incremental build monitor setup
function(setup_incremental_build_monitor)
    message(STATUS "Setting up incremental build monitoring...")

    # Start build timing
    incremental_build_start()

    # Check for modified files
    check_modified_files()

    # Analyze dependencies
    analyze_dependencies()

    # Display build performance info
    display_build_performance()

    # Add custom target for build end
    add_custom_target(build_end
        COMMAND ${CMAKE_COMMAND}
            -D CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}
            -P ${CMAKE_SOURCE_DIR}/tools/incremental_build_monitor.cmake
            --end-build
        COMMENT "Finalizing build and generating report"
    )

    message(STATUS "Incremental build monitoring configured")
endfunction()

# Handle script arguments for build end
if(CMAKE_SCRIPT_MODE_FILE)
    if(DEFINED ARGV1 AND "${ARGV1}" STREQUAL "--end-build")
        incremental_build_end()
    endif()
endif()
