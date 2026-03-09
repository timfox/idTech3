cmake_minimum_required(VERSION 3.16)
project(cflux2 C)

# Source files for the core library
set(LIBFLUX2C_SOURCES
    flux.c
    flux_kernels.c
    flux_tokenizer.c
    flux_vae.c
    flux_transformer.c
    flux_sample.c
    flux_image.c
    flux_safetensors.c
    flux_qwen3.c
    flux_qwen3_tokenizer.c
    flux_t5.c
    flux_t5_tokenizer.c
    flux_t5_model.c
    terminals.c
)

# Platform detection for backend selection
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    option(FLUX_USE_MPS "Use Metal Performance Shaders backend" ON)
    option(FLUX_USE_BLAS "Use BLAS backend" OFF)
elseif(UNIX)
    find_package(BLAS QUIET)
    if(BLAS_FOUND)
        option(FLUX_USE_BLAS "Use BLAS backend" ON)
    else()
        option(FLUX_USE_BLAS "Use BLAS backend" OFF)
    endif()
    option(FLUX_USE_MPS "Use Metal Performance Shaders backend" OFF)
else()
    option(FLUX_USE_BLAS "Use BLAS backend" OFF)
    option(FLUX_USE_MPS "Use Metal Performance Shaders backend" OFF)
endif()

option(FLUX_USE_GENERIC "Use generic backend (pure C)" OFF)

# Configure build flags based on selected backend
set(FLUX_COMPILE_DEFINITIONS)
set(FLUX_LINK_LIBRARIES m)

if(FLUX_USE_MPS)
    list(APPEND LIBFLUX2C_SOURCES flux_metal.m)
    list(APPEND FLUX_COMPILE_DEFINITIONS USE_BLAS USE_METAL ACCELERATE_NEW_LAPACK)
    list(APPEND FLUX_LINK_LIBRARIES
        "-framework Accelerate"
        "-framework Metal"
        "-framework MetalPerformanceShaders"
        "-framework MetalPerformanceShadersGraph"
        "-framework Foundation"
    )

    # Generate Metal shader header
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/flux_shaders_source.h
        COMMAND xxd -i -n flux_shaders_metal ${CMAKE_CURRENT_SOURCE_DIR}/flux_shaders.metal
                > ${CMAKE_CURRENT_BINARY_DIR}/flux_shaders_source.h
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/flux_shaders.metal
    )
    list(APPEND LIBFLUX2C_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/flux_shaders_source.h)

elseif(FLUX_USE_BLAS)
    if(APPLE)
        list(APPEND FLUX_COMPILE_DEFINITIONS USE_BLAS ACCELERATE_NEW_LAPACK)
        list(APPEND FLUX_LINK_LIBRARIES "-framework Accelerate")
    else()
        if(NOT BLAS_FOUND)
            message(WARNING "BLAS not found, falling back to generic backend")
            list(APPEND FLUX_COMPILE_DEFINITIONS GENERIC_BUILD)
        else()
            list(APPEND FLUX_COMPILE_DEFINITIONS USE_BLAS USE_OPENBLAS)
            list(APPEND FLUX_LINK_LIBRARIES BLAS::BLAS)
            include_directories(/usr/include/openblas)
        endif()
    endif()

elseif(FLUX_USE_GENERIC)
    list(APPEND FLUX_COMPILE_DEFINITIONS GENERIC_BUILD)
else()
    # Default to generic if nothing specified
    list(APPEND FLUX_COMPILE_DEFINITIONS GENERIC_BUILD)
endif()

# Create the static library
add_library(cflux2 STATIC ${LIBFLUX2C_SOURCES})

# Set compile definitions
target_compile_definitions(cflux2 PRIVATE ${FLUX_COMPILE_DEFINITIONS} _GNU_SOURCE)

# Suppress some pedantic warnings for FLUX
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cflux2 PRIVATE
        -Wno-missing-prototypes
        -Wno-unused-function
        -Wno-unused-parameter
        -Wno-cast-qual
        $<$<C_COMPILER_ID:GNU>:-Wno-discarded-qualifiers>
        $<$<C_COMPILER_ID:Clang>:-Wno-ignored-qualifiers>)
endif()

# Set include directories
target_include_directories(cflux2
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    INTERFACE
        $<INSTALL_INTERFACE:include>
)

# Link libraries
target_link_libraries(cflux2 PRIVATE ${FLUX_LINK_LIBRARIES})

# CLI executable (external generator)
set(FLUX_CLI_SOURCES
    main.c
    flux_cli.c
    embcache.c
)
if(WIN32 OR MINGW OR MSYS)
    list(APPEND FLUX_CLI_SOURCES)
else()
    list(APPEND FLUX_CLI_SOURCES linenoise.c)
endif()

add_executable(flux_cli ${FLUX_CLI_SOURCES})

target_compile_definitions(flux_cli PRIVATE ${FLUX_COMPILE_DEFINITIONS} _GNU_SOURCE)
if(WIN32)
    target_compile_definitions(flux_cli PRIVATE FLUX_CLI_NO_LINENOISE)
endif()
target_include_directories(flux_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(flux_cli PRIVATE cflux2 ${FLUX_LINK_LIBRARIES})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(flux_cli PRIVATE
        -Wno-missing-prototypes
        -Wno-unused-function
        -Wno-unused-parameter
        -Wno-cast-qual
        $<$<C_COMPILER_ID:GNU>:-Wno-discarded-qualifiers>
        $<$<C_COMPILER_ID:Clang>:-Wno-ignored-qualifiers>
        -Wno-format-truncation
        $<$<C_COMPILER_ID:GNU>:-Wno-stringop-truncation>)
endif()

# Set C standard
set_target_properties(cflux2 PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Install headers and library
install(TARGETS cflux2
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES flux.h flux_kernels.h flux_safetensors.h flux_qwen3.h
    DESTINATION include
)