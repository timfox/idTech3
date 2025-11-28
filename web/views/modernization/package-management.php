<?php
/**
 * Package Management and Dependencies for id Tech 3
 */
$title = 'Package Management - id Tech 3 Documentation';
$breadcrumbs = [
    '/modernization' => 'Modernization',
    '/modernization/package-management' => 'Package Management'
];
?>

<h1>Package Management and Dependencies</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Modern dependency management for id Tech 3 development using contemporary package managers. This guide covers setting up automated dependency resolution, version management, and cross-platform library distribution for streamlined development workflows.</p>
    
    <div class="feature-list">
        <h3>Package Management Benefits</h3>
        <ul>
            <li><strong>Automated Dependencies:</strong> Automatic downloading and building of required libraries</li>
            <li><strong>Version Control:</strong> Reproducible builds with locked dependency versions</li>
            <li><strong>Cross-Platform:</strong> Consistent library setup across Windows, Linux, macOS</li>
            <li><strong>Security:</strong> Verified package sources and integrity checking</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>vcpkg Package Manager</h2>
    
    <h3>Initial Setup</h3>
    <div class="code-block">
        <pre><code># Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Windows
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Linux/macOS  
./bootstrap-vcpkg.sh
./vcpkg integrate install

# Set environment variable
export VCPKG_ROOT=/path/to/vcpkg

# Add to CMake toolchain
cmake -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake ..</code></pre>
    </div>
    
    <h3>Package Manifest (vcpkg.json)</h3>
    <div class="code-block">
        <pre><code>{
  "name": "quake3e-modern",
  "version": "1.0.0",
  "description": "Modernized id Tech 3 Engine",
  "license": "GPL-2.0",
  "supports": "windows & linux & osx",
  
  "dependencies": [
    {
      "name": "sdl2",
      "version>=": "2.28.0",
      "features": ["vulkan"]
    },
    {
      "name": "openal-soft", 
      "version>=": "1.22.0"
    },
    {
      "name": "vulkan-headers",
      "version>=": "1.3.268"
    },
    {
      "name": "vulkan-memory-allocator",
      "version>=": "3.0.1"
    },
    {
      "name": "glm",
      "version>=": "0.9.9"
    },
    {
      "name": "stb",
      "features": ["stb-image", "stb-image-write", "stb-image-resize"]
    },
    {
      "name": "nlohmann-json",
      "version>=": "3.11.0"
    },
    {
      "name": "zlib",
      "version>=": "1.2.11"
    },
    {
      "name": "libpng",
      "version>=": "1.6.37"
    },
    {
      "name": "libjpeg-turbo",
      "version>=": "2.1.0"
    }
  ],
  
  "features": {
    "imgui": {
      "description": "Dear ImGui integration",
      "dependencies": [
        {
          "name": "imgui",
          "features": [
            "core", 
            "glfw-binding", 
            "opengl3-binding", 
            "vulkan-binding",
            "freetype"
          ]
        },
        "glfw3"
      ]
    },
    
    "tracy": {
      "description": "Tracy profiler support", 
      "dependencies": [
        {
          "name": "tracy",
          "features": ["enable"]
        }
      ]
    },
    
    "networking": {
      "description": "Enhanced networking support",
      "dependencies": [
        "curl",
        "openssl",
        "miniupnpc"
      ]
    },
    
    "audio-codecs": {
      "description": "Additional audio codec support",
      "dependencies": [
        "libvorbis",
        "opus",
        "libflac"
      ]
    },
    
    "testing": {
      "description": "Unit testing framework",
      "dependencies": [
        "gtest",
        "benchmark"
      ]
    }
  },
  
  "overrides": [
    {
      "name": "vulkan-headers",
      "version": "1.3.268"
    }
  ]
}</code></pre>
    </div>
    
    <h3>CMake Integration</h3>
    <div class="code-block">
        <pre><code># CMakeLists.txt - vcpkg integration
cmake_minimum_required(VERSION 3.20)

# Set vcpkg toolchain before project()
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

project(Quake3e-Modern)

# Feature options that map to vcpkg features
option(FEATURE_IMGUI "Enable Dear ImGui" ON)
option(FEATURE_TRACY "Enable Tracy profiler" OFF)
option(FEATURE_NETWORKING "Enhanced networking" ON)
option(FEATURE_AUDIO_CODECS "Additional audio codecs" ON)
option(FEATURE_TESTING "Enable testing framework" OFF)

# Build vcpkg feature list
set(VCPKG_MANIFEST_FEATURES "")
if(FEATURE_IMGUI)
    list(APPEND VCPKG_MANIFEST_FEATURES "imgui")
endif()
if(FEATURE_TRACY)
    list(APPEND VCPKG_MANIFEST_FEATURES "tracy")
endif()
if(FEATURE_NETWORKING)
    list(APPEND VCPKG_MANIFEST_FEATURES "networking")
endif()
if(FEATURE_AUDIO_CODECS)
    list(APPEND VCPKG_MANIFEST_FEATURES "audio-codecs")
endif()
if(FEATURE_TESTING)
    list(APPEND VCPKG_MANIFEST_FEATURES "testing")
endif()

# Find packages
find_package(SDL2 REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
find_package(glm REQUIRED)
find_package(unofficial-stb REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(libjpeg-turbo REQUIRED)

if(FEATURE_IMGUI)
    find_package(imgui REQUIRED)
    find_package(glfw3 REQUIRED)
endif()

if(FEATURE_TRACY)
    find_package(Tracy REQUIRED)
endif()

if(FEATURE_NETWORKING)
    find_package(CURL REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(unofficial-miniupnpc REQUIRED)
endif()

if(FEATURE_TESTING)
    find_package(GTest REQUIRED)
    find_package(benchmark REQUIRED)
endif()</code></pre>
    </div>
</div>

<div class="section">
    <h2>Conan Package Manager</h2>
    
    <h3>Conanfile Configuration</h3>
    <div class="code-block">
        <pre><code># conanfile.py
from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy

class Quake3eConan(ConanFile):
    name = "quake3e-modern"
    version = "1.0.0"
    
    # Package metadata
    license = "GPL-2.0"
    description = "Modernized id Tech 3 Engine"
    url = "https://github.com/yourorg/quake3e-modern"
    
    # Build settings
    settings = "os", "compiler", "build_type", "arch"
    
    # Options for conditional dependencies
    options = {
        "imgui": [True, False],
        "tracy": [True, False], 
        "networking": [True, False],
        "audio_codecs": [True, False],
        "shared": [True, False],
        "fPIC": [True, False]
    }
    
    default_options = {
        "imgui": True,
        "tracy": False,
        "networking": True, 
        "audio_codecs": True,
        "shared": False,
        "fPIC": True
    }
    
    # Core dependencies
    requires = [
        "sdl/2.28.3",
        "openal/1.22.2",
        "vulkan-headers/1.3.268",
        "vulkan-memory-allocator/3.0.1", 
        "glm/0.9.9.8",
        "stb/cci.20230920",
        "nlohmann_json/3.11.2",
        "zlib/1.2.13",
        "libpng/1.6.40",
        "libjpeg/9e"
    ]
    
    def requirements(self):
        if self.options.imgui:
            self.requires("imgui/1.89.9")
            self.requires("glfw/3.3.8")
            
        if self.options.tracy:
            self.requires("tracy/0.9.1")
            
        if self.options.networking:
            self.requires("libcurl/8.2.1")
            self.requires("openssl/3.1.1")
            
        if self.options.audio_codecs:
            self.requires("vorbis/1.3.7")
            self.requires("opus/1.4")
            self.requires("flac/1.4.3")
    
    def configure(self):
        # Configure package options
        self.options["sdl"].shared = False
        self.options["openal"].shared = False
        
        if self.options.imgui:
            self.options["imgui"].shared = False
            self.options["glfw"].shared = False
            
    def generate(self):
        # Generate CMake integration files
        deps = CMakeDeps(self)
        deps.generate()
        
        tc = CMakeToolchain(self)
        
        # Pass feature flags to CMake
        tc.variables["FEATURE_IMGUI"] = self.options.imgui
        tc.variables["FEATURE_TRACY"] = self.options.tracy
        tc.variables["FEATURE_NETWORKING"] = self.options.networking
        tc.variables["FEATURE_AUDIO_CODECS"] = self.options.audio_codecs
        
        tc.generate()
    
    def layout(self):
        cmake_layout(self)</code></pre>
    </div>
    
    <h3>Conan Profile Configuration</h3>
    <div class="code-block">
        <pre><code># profiles/windows-release
[settings]
os=Windows
arch=x86_64
compiler=Visual Studio
compiler.version=17
compiler.runtime=dynamic
build_type=Release

[options]
*:shared=False

[conf]
tools.cmake.cmaketoolchain:generator=Visual Studio 17 2022

# profiles/linux-debug  
[settings]
os=Linux
arch=x86_64
compiler=gcc
compiler.version=11
compiler.libcxx=libstdc++11
build_type=Debug

[options]
*:shared=False
*:fPIC=True

# Usage commands
# conan create . --profile:build=default --profile:host=windows-release
# conan install . --build=missing --profile=linux-debug</code></pre>
    </div>
</div>

<div class="section">
    <h2>System Package Managers</h2>
    
    <h3>Linux Distribution Packages</h3>
    <div class="code-block">
        <pre><code># Ubuntu/Debian dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libsdl2-dev \
    libopenal-dev \
    libvulkan-dev \
    vulkan-tools \
    vulkan-validationlayers-dev \
    libglm-dev \
    libpng-dev \
    libjpeg-dev \
    zlib1g-dev \
    libasound2-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev

# Fedora/CentOS/RHEL
sudo dnf install -y \
    gcc-c++ \
    cmake \
    git \
    pkgconfig \
    SDL2-devel \
    openal-soft-devel \
    vulkan-headers \
    vulkan-loader-devel \
    vulkan-tools \
    glm-devel \
    libpng-devel \
    libjpeg-turbo-devel \
    zlib-devel \
    alsa-lib-devel \
    libX11-devel \
    libXrandr-devel</code></pre>
    </div>
    
    <h3>macOS Homebrew Setup</h3>
    <div class="code-block">
        <pre><code># Install Homebrew if not present
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew update
brew install \
    cmake \
    git \
    pkg-config \
    sdl2 \
    openal-soft \
    vulkan-headers \
    vulkan-loader \
    molten-vk \
    glm \
    libpng \
    jpeg-turbo \
    zlib

# Optional packages
brew install \
    tracy \
    nlohmann-json \
    curl \
    openssl@3

# CMake integration
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
export CMAKE_PREFIX_PATH="/opt/homebrew:$CMAKE_PREFIX_PATH"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Custom Package Repository</h2>
    
    <h3>Private vcpkg Registry</h3>
    <div class="code-block">
        <pre><code># vcpkg-configuration.json for custom registry
{
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/Microsoft/vcpkg",
    "baseline": "a42af01b72c28a8e1d7b48107b33e4f286a55ef6"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/yourorg/quake3e-vcpkg-registry",
      "baseline": "main",
      "packages": [
        "quake3e-assets",
        "idtech3-tools",
        "custom-shaders"
      ]
    }
  ]
}

# Custom port for game assets
# ports/quake3e-assets/portfile.cmake
vcpkg_download_distfile(ARCHIVE
    URLS "https://releases.yourorg.com/quake3e-assets-v1.0.zip"
    FILENAME "quake3e-assets-1.0.zip"
    SHA512 "your-sha512-hash-here"
)

vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH
    ARCHIVE ${ARCHIVE}
)

file(INSTALL ${SOURCE_PATH}/pak0.pk3 DESTINATION ${CURRENT_PACKAGES_DIR}/share/quake3e/baseq3)
file(INSTALL ${SOURCE_PATH}/pak1.pk3 DESTINATION ${CURRENT_PACKAGES_DIR}/share/quake3e/baseq3)
file(INSTALL ${SOURCE_PATH}/pak2.pk3 DESTINATION ${CURRENT_PACKAGES_DIR}/share/quake3e/baseq3)</code></pre>
    </div>
    
    <h3>Conan Package Recipe</h3>
    <div class="code-block">
        <pre><code># Custom package: idtech3-tools/conanfile.py
from conan import ConanFile
from conan.tools.files import copy, get
from conan.tools.cmake import CMake, cmake_layout

class IdTech3ToolsConan(ConanFile):
    name = "idtech3-tools"
    version = "1.0.0"
    
    settings = "os", "compiler", "build_type", "arch"
    
    def source(self):
        get(self, "https://github.com/yourorg/idtech3-tools/archive/v1.0.0.tar.gz",
            destination=self.source_folder, strip_root=True)
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
    def package(self):
        copy(self, "*.exe", 
             dst=os.path.join(self.package_folder, "bin"),
             src=os.path.join(self.build_folder, "bin"))
        copy(self, "*.dll",
             dst=os.path.join(self.package_folder, "bin"), 
             src=os.path.join(self.build_folder, "bin"))
             
    def package_info(self):
        self.cpp_info.bindirs = ["bin"]</code></pre>
    </div>
</div>

<div class="section">
    <h2>Dependency Version Management</h2>
    
    <h3>Version Locking</h3>
    <div class="code-block">
        <pre><code># vcpkg.lock for reproducible builds
{
  "version": 1,
  "dependencies": {
    "sdl2": {
      "version": "2.28.3",
      "port-hash": "a1b2c3d4e5f6...",
      "features": ["vulkan"]
    },
    "openal-soft": {
      "version": "1.22.2", 
      "port-hash": "f6e5d4c3b2a1..."
    },
    "vulkan-headers": {
      "version": "1.3.268",
      "port-hash": "1a2b3c4d5e6f..."
    }
  }
}

# Conan lock file (conan.lock)
{
    "graph_lock": {
        "nodes": {
            "0": {
                "ref": "quake3e-modern/1.0.0@",
                "requires": ["1", "2", "3"]
            },
            "1": {
                "ref": "sdl/2.28.3",
                "package_id": "abc123...",
                "prev": "def456..."
            },
            "2": {
                "ref": "openal/1.22.2", 
                "package_id": "ghi789...",
                "prev": "jkl012..."
            }
        }
    }
}</code></pre>
    </div>
    
    <h3>Compatibility Matrix</h3>
    <div class="code-block">
        <pre><code># dependencies.yaml - Version compatibility matrix
compatibility_matrix:
  platforms:
    windows:
      minimum_versions:
        cmake: "3.20"
        visual_studio: "2019.16.11"
        windows_sdk: "10.0.19041"
        
    linux:
      minimum_versions:
        cmake: "3.20"
        gcc: "10.0"
        glibc: "2.31"
        
    macos:
      minimum_versions:
        cmake: "3.20"
        xcode: "13.0"
        macos: "11.0"

  dependencies:
    sdl2:
      versions: ["2.28.0", "2.28.1", "2.28.2", "2.28.3"]
      conflicts:
        - name: "old-opengl"
          reason: "Incompatible OpenGL context creation"
          
    vulkan-headers:
      versions: ["1.3.268", "1.3.269"]
      requires:
        vulkan-loader: ">=1.3.268"
        
    imgui:
      versions: ["1.89.9", "1.90.0"]
      features:
        vulkan-binding:
          requires:
            vulkan-headers: ">=1.3.260"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Build Caching and Optimization</h2>
    
    <h3>Shared Package Cache</h3>
    <div class="code-block">
        <pre><code># vcpkg binary caching
set(VCPKG_BINARY_CACHING ON)
set(VCPKG_BINARY_CACHE_PATH "${CMAKE_SOURCE_DIR}/.vcpkg-cache")

# Environment variables for CI
export VCPKG_DEFAULT_BINARY_CACHE="/shared/vcpkg-cache"
export VCPKG_DOWNLOADS_PATH="/shared/vcpkg-downloads"

# Conan cache configuration  
[conf]
core.cache:global_conf_cache_dir=/shared/conan-cache
tools.system.package_manager:mode=install
tools.system.package_manager:sudo=True

# GitHub Actions cache integration
- name: Cache vcpkg packages
  uses: actions/cache@v3
  with:
    path: |
      vcpkg_installed/
      .vcpkg-cache/
    key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}
    restore-keys: |
      vcpkg-${{ runner.os }}-</code></pre>
    </div>
    
    <h3>Parallel Builds</h3>
    <div class="code-block">
        <pre><code># vcpkg parallel builds
set(VCPKG_MAX_CONCURRENCY 8)
export VCPKG_MAX_CONCURRENCY=8

# Conan parallel builds
[conf]
tools.cmake.cmake:jobs=8
tools.build:jobs=8

# CMake parallel builds
cmake --build build --parallel 8

# MSBuild specific
cmake --build build -- /maxcpucount:8</code></pre>
    </div>
</div>

<div class="section">
    <h2>Security and Validation</h2>
    
    <h3>Package Verification</h3>
    <div class="code-block">
        <pre><code># vcpkg hash verification
{
  "name": "custom-package",
  "version": "1.0.0", 
  "port-hash": "sha256:a1b2c3d4e5f6...",
  "source": {
    "url": "https://trusted-source.com/package.tar.gz",
    "sha256": "f6e5d4c3b2a1..."
  }
}

# Conan package verification
def export(self):
    # Verify source integrity
    tools.check_sha256("package.tar.gz", "expected-hash")
    
def source(self):
    # Download from trusted sources only
    get(self, "https://verified-source.com/package.tar.gz",
        sha256="verified-hash")
        
# Security scanning integration
- name: Security Scan
  run: |
    # Scan dependencies for known vulnerabilities
    conan audit --profile=release
    
    # Check for license compliance
    vcpkg export --scan-licenses</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/modernization/build-systems">Modern Build Systems</a></li>
        <li><a href="/modernization/ci-cd">CI/CD Pipeline</a></li>
        <li><a href="/external/libraries">External Libraries</a></li>
        <li><a href="/getting-started/installation">Installation Guide</a></li>
    </ul>
</div>