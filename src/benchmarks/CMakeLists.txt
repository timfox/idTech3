# Enhanced renderer benchmarks with comprehensive metrics collection
add_executable(renderer_bench renderer_bench.cpp)
target_include_directories(renderer_bench PRIVATE
    ${CMAKE_SOURCE_DIR}/src/renderers
    ${CMAKE_SOURCE_DIR}/src/common
)
set_target_properties(renderer_bench PROPERTIES
    FOLDER "benchmarks"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Mock implementations for denoiser and FSR functions since we don't link full renderers
target_compile_definitions(renderer_bench PRIVATE MOCK_RENDERER_FUNCTIONS)

# Link required libraries for enhanced timing and file I/O
if(UNIX)
    target_link_libraries(renderer_bench PRIVATE m)
endif()

# Install benchmark binary
install(TARGETS renderer_bench
    RUNTIME DESTINATION bin
    COMPONENT benchmarks
)
