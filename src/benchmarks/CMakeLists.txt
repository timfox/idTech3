add_executable(renderer_bench renderer_bench.cpp)
target_include_directories(renderer_bench PRIVATE ${CMAKE_SOURCE_DIR}/src/renderers)
set_target_properties(renderer_bench PROPERTIES FOLDER "benchmarks")

