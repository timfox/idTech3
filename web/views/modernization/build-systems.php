<?php
/**
 * Modern Build Systems for id Tech 3
 */
$title = 'Modern Build Systems - id Tech 3 Documentation';
$breadcrumbs = [
    '/modernization' => 'Modernization',
    '/modernization/build-systems' => 'Modern Build Systems'
];
?>

<h1>Modern Build Systems for id Tech 3</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Modernizing the id Tech 3 build process using contemporary build systems and package managers. This guide covers CMake, package managers, and cross-platform development workflows that improve developer experience and maintainability.</p>
    
    <div class="feature-list">
        <h3>Modern Build Benefits</h3>
        <ul>
            <li><strong>Cross-Platform:</strong> Single build configuration for Windows, Linux, macOS</li>
            <li><strong>Dependency Management:</strong> Automated library downloading and linking</li>
            <li><strong>IDE Integration:</strong> Native support in Visual Studio, CLion, VS Code</li>
            <li><strong>Parallel Builds:</strong> Faster compilation with multi-core support</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>CMake Configuration</h2>
    
    <h3>Root CMakeLists.txt</h3>
    <div class="code-block">
        <pre><code>cmake_minimum_required(VERSION 3.20)
project(Quake3e-Modern 
    VERSION 1.0.0
    DESCRIPTION "Modernized id Tech 3 Engine"
    LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_CLIENT "Build game client" ON)
option(BUILD_SERVER "Build dedicated server" ON)
option(BUILD_RENDERER_VULKAN "Build Vulkan renderer" ON)
option(BUILD_RENDERER_OPENGL "Build OpenGL renderer" ON)
option(USE_IMGUI "Enable Dear ImGui integration" ON)
option(USE_TRACY "Enable Tracy profiler" OFF)
option(BUILD_TESTS "Build unit tests" OFF)

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LINUX TRUE)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include modules
include(cmake/CompilerSettings.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Platform.cmake)

# Add subdirectories
add_subdirectory(src/engine)
add_subdirectory(src/game)
add_subdirectory(src/cgame)
add_subdirectory(src/ui)

if(BUILD_RENDERER_VULKAN)
    add_subdirectory(src/renderer_vk)
endif()

if(BUILD_RENDERER_OPENGL)
    add_subdirectory(src/renderer_gl)
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()</code></pre>
    </div>
    
    <h3>Compiler Settings Module</h3>
    <div class="code-block">
        <pre><code># cmake/CompilerSettings.cmake

# Compiler-specific settings
if(MSVC)
    # Visual Studio settings
    add_compile_options(
        /W4                     # Warning level 4
        /permissive-           # Strict conformance
        /Zc:__cplusplus        # Correct __cplusplus macro
        /MP                    # Multi-processor compilation
    )
    
    # Debug settings
    add_compile_options($<$<CONFIG:Debug>:/Od>)         # No optimization
    add_compile_options($<$<CONFIG:Debug>:/Zi>)         # Debug info
    add_compile_options($<$<CONFIG:Debug>:/RTC1>)       # Runtime checks
    
    # Release settings
    add_compile_options($<$<CONFIG:Release>:/O2>)       # Optimize for speed
    add_compile_options($<$<CONFIG:Release>:/GL>)       # Whole program optimization
    add_link_options($<$<CONFIG:Release>:/LTCG>)        # Link-time code generation
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang settings
    add_compile_options(
        -Wall -Wextra          # Standard warnings
        -Wno-unused-parameter  # Disable specific warnings
        -fno-exceptions        # Disable exceptions for performance
    )
    
    # Debug settings
    add_compile_options($<$<CONFIG:Debug>:-O0>)         # No optimization
    add_compile_options($<$<CONFIG:Debug>:-g3>)         # Full debug info
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address>) # AddressSanitizer
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)
    
    # Release settings
    add_compile_options($<$<CONFIG:Release>:-O3>)       # Maximum optimization
    add_compile_options($<$<CONFIG:Release>:-flto>)     # Link-time optimization
    add_compile_options($<$<CONFIG:Release>:-DNDEBUG>)  # Disable assertions
    add_link_options($<$<CONFIG:Release>:-flto>)
endif()

# Platform-specific definitions
if(PLATFORM_WINDOWS)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
elseif(PLATFORM_LINUX)
    add_compile_definitions(
        _GNU_SOURCE
    )
elseif(PLATFORM_MACOS)
    add_compile_definitions(
        _DARWIN_FEATURE_64_BIT_INODE
    )
endif()</code></pre>
    </div>
</div>

<div class="section">
    <h2>Package Management</h2>
    
    <h3>vcpkg Integration</h3>
    <div class="code-block">
        <pre><code># vcpkg.json - Package manifest
{
  "name": "quake3e-modern",
  "version": "1.0.0",
  "dependencies": [
    "sdl2",
    "openal-soft",
    "vulkan-headers",
    "vulkan-memory-allocator",
    "glm",
    "stb",
    "nlohmann-json",
    {
      "name": "imgui",
      "features": ["core", "glfw-binding", "opengl3-binding", "vulkan-binding"]
    },
    {
      "name": "tracy",
      "features": ["enable"]
    }
  ],
  "overrides": [
    {
      "name": "vulkan-headers",
      "version": "1.3.268"
    }
  ]
}

# CMake integration
# cmake/Dependencies.cmake
find_package(SDL2 REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
find_package(glm REQUIRED)
find_package(unofficial-tracy REQUIRED)

if(USE_IMGUI)
    find_package(imgui REQUIRED)
endif()

# Create interface library for common dependencies
add_library(engine_dependencies INTERFACE)
target_link_libraries(engine_dependencies INTERFACE
    SDL2::SDL2
    SDL2::SDL2main
    OpenAL::OpenAL
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    glm::glm
)

if(USE_IMGUI)
    target_link_libraries(engine_dependencies INTERFACE
        imgui::imgui
    )
endif()

if(USE_TRACY)
    target_link_libraries(engine_dependencies INTERFACE
        Tracy::TracyClient
    )
endif()</code></pre>
    </div>
    
    <h3>Conan Package Manager</h3>
    <div class="code-block">
        <pre><code># conanfile.py
from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout

class Quake3eConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    requires = [
        "sdl/2.28.3",
        "openal/1.22.2", 
        "vulkan-headers/1.3.268",
        "glm/0.9.9.8",
        "stb/cci.20230920",
        "nlohmann_json/3.11.2"
    ]
    
    def requirements(self):
        if self.options.imgui:
            self.requires("imgui/1.89.9")
        if self.options.tracy:
            self.requires("tracy/0.9.1")
    
    def configure(self):
        self.options["sdl"].shared = False
        self.options["openal"].shared = False
        
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        
        tc = CMakeToolchain(self)
        tc.generate()
    
    def layout(self):
        cmake_layout(self)

# Usage
# conan install . --build=missing -s build_type=Release
# cmake --preset=conan-release
# cmake --build --preset=conan-release</code></pre>
    </div>
</div>

<div class="section">
    <h2>Cross-Platform Configuration</h2>
    
    <h3>Platform-Specific Settings</h3>
    <div class="code-block">
        <pre><code># cmake/Platform.cmake

if(PLATFORM_WINDOWS)
    # Windows-specific libraries and settings
    target_link_libraries(engine_dependencies INTERFACE
        kernel32 user32 gdi32 winspool shell32 ole32
        oleaut32 uuid comdlg32 advapi32 winmm wsock32 ws2_32
    )
    
    # Windows subsystem for release builds
    set_target_properties(quake3e PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
    
    # Enable high DPI awareness
    target_compile_definitions(engine_dependencies INTERFACE
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    )

elseif(PLATFORM_LINUX)
    # Linux-specific libraries
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(ALSA REQUIRED alsa)
    
    target_link_libraries(engine_dependencies INTERFACE
        ${X11_LIBRARIES}
        ${ALSA_LIBRARIES}
        dl
        pthread
        m
    )
    
    # Enable all symbols for better debugging
    target_link_options(engine_dependencies INTERFACE
        $<$<CONFIG:Debug>:-rdynamic>
    )

elseif(PLATFORM_MACOS)
    # macOS frameworks
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(OPENGL_FRAMEWORK OpenGL)
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
    find_library(IOKIT_FRAMEWORK IOKit)
    
    target_link_libraries(engine_dependencies INTERFACE
        ${COCOA_FRAMEWORK}
        ${OPENGL_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOUNIT_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
    )
    
    # macOS deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    
    # App bundle settings
    set_target_properties(quake3e PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Quake III Arena"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.idsoftware.quake3"
    )
endif()</code></pre>
    </div>
    
    <h3>Build Configurations</h3>
    <div class="code-block">
        <pre><code># Create custom build types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Profile" CACHE STRING "Build configs" FORCE)

# Profile build configuration
set(CMAKE_CXX_FLAGS_PROFILE "-O2 -g -DPROFILE_BUILD")
set(CMAKE_C_FLAGS_PROFILE "-O2 -g -DPROFILE_BUILD")
set(CMAKE_EXE_LINKER_FLAGS_PROFILE "")

# Feature-based configuration
if(BUILD_RENDERER_VULKAN)
    target_compile_definitions(engine_dependencies INTERFACE USE_VULKAN=1)
endif()

if(BUILD_RENDERER_OPENGL)
    target_compile_definitions(engine_dependencies INTERFACE USE_OPENGL=1)
endif()

if(USE_IMGUI)
    target_compile_definitions(engine_dependencies INTERFACE USE_IMGUI=1)
endif()

if(USE_TRACY)
    target_compile_definitions(engine_dependencies INTERFACE 
        TRACY_ENABLE
        TRACY_ON_DEMAND
    )
endif()</code></pre>
    </div>
</div>

<div class="section">
    <h2>Advanced Build Features</h2>
    
    <h3>Precompiled Headers</h3>
    <div class="code-block">
        <pre><code># src/engine/CMakeLists.txt
add_library(engine_core STATIC
    ${ENGINE_SOURCES}
)

# Precompiled header for faster compilation
target_precompile_headers(engine_core PRIVATE
    <vector>
    <string>
    <memory>
    <algorithm>
    <unordered_map>
    "common/q_shared.h"
    "engine/q_common.h"
)

# Reuse PCH for other targets
target_precompile_headers(game_module REUSE_FROM engine_core)
target_precompile_headers(cgame_module REUSE_FROM engine_core)</code></pre>
    </div>
    
    <h3>Unity Builds</h3>
    <div class="code-block">
        <pre><code># Enable unity builds for faster compilation
set_target_properties(engine_core PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 16
)

# Exclude problematic files from unity build
set_source_files_properties(
    src/engine/platform_specific.c
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)</code></pre>
    </div>
    
    <h3>Asset Pipeline Integration</h3>
    <div class="code-block">
        <pre><code># Custom commands for asset processing
find_program(TEXTURE_CONVERTER
    NAMES ImageMagick convert
    DOC "Texture conversion tool"
)

function(add_texture_target target_name input_file output_file)
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${TEXTURE_CONVERTER} ${input_file} -format DDS ${output_file}
        DEPENDS ${input_file}
        COMMENT "Converting texture ${input_file}"
    )
    
    add_custom_target(${target_name} DEPENDS ${output_file})
endfunction()

# Process all textures
file(GLOB_RECURSE TEXTURE_SOURCES "assets/textures/*.tga")
foreach(texture ${TEXTURE_SOURCES})
    get_filename_component(name ${texture} NAME_WE)
    get_filename_component(dir ${texture} DIRECTORY)
    file(RELATIVE_PATH rel_dir ${CMAKE_SOURCE_DIR}/assets/textures ${dir})
    
    set(output_file "${CMAKE_BINARY_DIR}/assets/textures/${rel_dir}/${name}.dds")
    add_texture_target(texture_${name} ${texture} ${output_file})
    
    list(APPEND PROCESSED_TEXTURES texture_${name})
endforeach()

add_custom_target(process_assets DEPENDS ${PROCESSED_TEXTURES})</code></pre>
    </div>
</div>

<div class="section">
    <h2>IDE Integration</h2>
    
    <h3>Visual Studio Integration</h3>
    <div class="code-block">
        <pre><code># CMakeSettings.json for Visual Studio
{
  "configurations": [
    {
      "name": "x64-Debug",
      "generator": "Ninja",
      "configurationType": "Debug",
      "buildRoot": "${projectDir}\\build\\${name}",
      "installRoot": "${projectDir}\\install\\${name}",
      "cmakeCommandArgs": "",
      "buildCommandArgs": "",
      "ctestCommandArgs": "",
      "inheritEnvironments": [ "msvc_x64_x64" ],
      "variables": [
        {
          "name": "CMAKE_TOOLCHAIN_FILE",
          "value": "${env.VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake",
          "type": "FILEPATH"
        },
        {
          "name": "VCPKG_TARGET_TRIPLET",
          "value": "x64-windows",
          "type": "STRING"
        }
      ]
    }
  ]
}</code></pre>
    </div>
    
    <h3>VS Code Configuration</h3>
    <div class="code-block">
        <pre><code># .vscode/settings.json
{
    "cmake.configureArgs": [
        "-DCMAKE_TOOLCHAIN_FILE=${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "-DVCPKG_TARGET_TRIPLET=x64-linux"
    ],
    "cmake.buildArgs": [
        "--parallel", "8"
    ],
    "cmake.debugConfig": {
        "name": "Launch Quake3e",
        "type": "cppdbg",
        "request": "launch",
        "program": "${command:cmake.launchTargetPath}",
        "args": ["+set", "r_mode", "6"],
        "stopAtEntry": false,
        "cwd": "${workspaceFolder}",
        "environment": [],
        "console": "integratedTerminal"
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Continuous Integration</h2>
    
    <h3>GitHub Actions Workflow</h3>
    <div class="code-block">
        <pre><code># .github/workflows/build.yml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        build_type: [Debug, Release]
        
    runs-on: ${{ matrix.os }}
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install vcpkg
      uses: lukka/run-vcpkg@v11
      with:
        vcpkgGitCommitId: 'a42af01b72c28a8e1d7b48107b33e4f286a55ef6'
    
    - name: Configure CMake
      run: |
        cmake -B build -S . \
          -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
          -DCMAKE_TOOLCHAIN_FILE=${{ github.workspace }}/vcpkg/scripts/buildsystems/vcpkg.cmake \
          -DBUILD_TESTS=ON
    
    - name: Build
      run: cmake --build build --config ${{ matrix.build_type }} --parallel
    
    - name: Test
      working-directory: build
      run: ctest --build-config ${{ matrix.build_type }} --parallel --output-on-failure
    
    - name: Package
      if: matrix.build_type == 'Release'
      run: |
        cmake --build build --target package
        
    - name: Upload Artifacts
      if: matrix.build_type == 'Release'
      uses: actions/upload-artifact@v3
      with:
        name: quake3e-${{ matrix.os }}
        path: build/*.zip</code></pre>
    </div>
</div>

<div class="section">
    <h2>Performance Optimization</h2>
    
    <h3>Link-Time Optimization</h3>
    <div class="code-block">
        <pre><code># Enable LTO for release builds
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

if(ipo_supported)
    set_target_properties(quake3e PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
    )
else()
    message(WARNING "IPO is not supported: ${ipo_error}")
endif()

# Profile-guided optimization (PGO) support
option(USE_PGO "Enable Profile-Guided Optimization" OFF)

if(USE_PGO)
    if(MSVC)
        target_compile_options(quake3e PRIVATE /GL)
        target_link_options(quake3e PRIVATE /LTCG:PGI)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(quake3e PRIVATE -fprofile-generate)
        target_link_options(quake3e PRIVATE -fprofile-generate)
    endif()
endif()</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/modernization/modern-cpp">Modern C++ Features</a></li>
        <li><a href="/modernization/package-management">Package Management</a></li>
        <li><a href="/modernization/ci-cd">CI/CD Pipeline</a></li>
        <li><a href="/external/libraries">External Libraries</a></li>
        <li><a href="/development/debugging">Debugging Tools</a></li>
    </ul>
</div>