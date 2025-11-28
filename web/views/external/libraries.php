<?php
/**
 * External Libraries Documentation
 */
$title = 'External Libraries - id Tech 3 Documentation';
$breadcrumbs = [
    '/external' => 'External Libraries',
    '/external/libraries' => 'Libraries Overview'
];
?>

<h1>External Libraries</h1>

<div class="section">
    <h2>Overview</h2>
    <p>id Tech 3 and its modern derivatives utilize various external libraries to provide enhanced functionality, from graphics rendering to audio processing and utility functions.</p>
    
    <div class="feature-list">
        <h3>Library Categories</h3>
        <ul>
            <li><strong>Graphics:</strong> Vulkan, OpenGL extensions, image processing</li>
            <li><strong>Audio:</strong> Sound processing and 3D audio</li>
            <li><strong>Networking:</strong> Protocol and communication libraries</li>
            <li><strong>Utility:</strong> Math, compression, and system utilities</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Graphics Libraries</h2>
    
    <h3>Vulkan SDK</h3>
    <p>Modern Vulkan rendering support in Quake3e:</p>
    <div class="code-block">
        <pre><code># Vulkan library dependencies
vulkan-1.lib        # Vulkan loader
vulkan-headers      # Vulkan headers
SPIRV-Cross         # Shader cross-compilation
glslang            # GLSL to SPIRV compiler</code></pre>
    </div>
    
    <h3>OpenGL Extensions</h3>
    <ul>
        <li><strong>GLEW:</strong> OpenGL Extension Wrangler</li>
        <li><strong>GLU:</strong> OpenGL Utility Library</li>
        <li><strong>GLFW:</strong> OpenGL Framework (alternative to SDL)</li>
        <li><strong>FreeGLUT:</strong> OpenGL Utility Toolkit</li>
    </ul>
    
    <h3>Image Processing</h3>
    <div class="code-block">
        <pre><code>// Image format support libraries
stb_image.h         // STB image loading
DevIL               // Developer's Image Library  
FreeImage           // Free image processing
libjpeg             // JPEG support
libpng              // PNG support
libtiff             // TIFF support</code></pre>
    </div>
</div>

<div class="section">
    <h2>Audio Libraries</h2>
    
    <h3>Sound Processing</h3>
    <ul>
        <li><strong>OpenAL:</strong> 3D positional audio</li>
        <li><strong>SDL Audio:</strong> Cross-platform audio support</li>
        <li><strong>FMOD:</strong> Advanced audio engine (commercial)</li>
        <li><strong>PortAudio:</strong> Cross-platform audio I/O</li>
    </ul>
    
    <div class="code-block">
        <pre><code># Audio library configuration
USE_OPENAL=1           # Enable OpenAL support
USE_SDL_AUDIO=1        # Enable SDL audio
USE_CODEC_VORBIS=1     # OGG Vorbis support
USE_CODEC_OPUS=1       # Opus codec support</code></pre>
    </div>
    
    <h3>Audio Codecs</h3>
    <ul>
        <li><strong>libvorbis:</strong> OGG Vorbis decoding</li>
        <li><strong>libopus:</strong> Opus audio codec</li>
        <li><strong>libmad:</strong> MPEG audio decoder</li>
        <li><strong>FLAC:</strong> Free Lossless Audio Codec</li>
    </ul>
</div>

<div class="section">
    <h2>Networking Libraries</h2>
    
    <h3>Network Protocol</h3>
    <div class="code-block">
        <pre><code>// Networking dependencies
Winsock2.h          // Windows networking
sys/socket.h        // Unix networking
netinet/in.h        // Internet protocols
arpa/inet.h         // Address manipulation</code></pre>
    </div>
    
    <h3>Protocol Libraries</h3>
    <ul>
        <li><strong>libcurl:</strong> HTTP/HTTPS client library</li>
        <li><strong>OpenSSL:</strong> Cryptography and SSL/TLS</li>
        <li><strong>zlib:</strong> Compression library</li>
        <li><strong>miniupnpc:</strong> UPnP port mapping</li>
    </ul>
</div>

<div class="section">
    <h2>Utility Libraries</h2>
    
    <h3>Math Libraries</h3>
    <div class="code-block">
        <pre><code>// Math library usage
#include <math.h>       // Standard math functions
#include "q_math.h"     // Quake math utilities

// Vector math optimizations
#ifdef USE_SSE
#include <xmmintrin.h>  // SSE intrinsics
#endif

#ifdef USE_NEON
#include <arm_neon.h>   // ARM NEON intrinsics
#endif</code></pre>
    </div>
    
    <h3>System Utilities</h3>
    <ul>
        <li><strong>SDL2:</strong> Cross-platform system layer</li>
        <li><strong>pthread:</strong> Threading support</li>
        <li><strong>iconv:</strong> Character encoding conversion</li>
        <li><strong>getopt:</strong> Command-line argument parsing</li>
    </ul>
    
    <h3>Compression Libraries</h3>
    <ul>
        <li><strong>zlib:</strong> General purpose compression</li>
        <li><strong>lz4:</strong> Fast compression algorithm</li>
        <li><strong>bzip2:</strong> High compression ratio</li>
        <li><strong>minizip:</strong> ZIP archive support</li>
    </ul>
</div>

<div class="section">
    <h2>Development Libraries</h2>
    
    <h3>Debug and Profiling</h3>
    <div class="code-block">
        <pre><code>// Debug library integration
#ifdef USE_DEAR_IMGUI
#include "imgui.h"              // Dear ImGui
#include "imgui_impl_opengl3.h" // OpenGL backend
#include "imgui_impl_sdl.h"     // SDL backend
#endif

#ifdef USE_TRACY
#include "tracy/Tracy.hpp"      // Tracy profiler
#endif</code></pre>
    </div>
    
    <h3>Memory Management</h3>
    <ul>
        <li><strong>jemalloc:</strong> Advanced memory allocator</li>
        <li><strong>tcmalloc:</strong> Thread-caching malloc</li>
        <li><strong>Valgrind:</strong> Memory debugging (Linux)</li>
        <li><strong>AddressSanitizer:</strong> Memory error detection</li>
    </ul>
</div>

<div class="section">
    <h2>Platform-Specific Libraries</h2>
    
    <h3>Windows</h3>
    <div class="code-block">
        <pre><code>// Windows-specific libraries
kernel32.lib        // Core Windows API
user32.lib          // User interface
gdi32.lib           // Graphics Device Interface
winmm.lib           // Multimedia API
ws2_32.lib          // Winsock networking
ole32.lib           // Object Linking & Embedding
dinput8.lib         // DirectInput
xinput.lib          // XInput gamepad support</code></pre>
    </div>
    
    <h3>Linux</h3>
    <div class="code-block">
        <pre><code># Linux library dependencies
libGL               # OpenGL
libX11              # X Window System
libXext             # X extensions
libXxf86vm          # XFree86 video mode
libasound           # ALSA audio
libpulse            # PulseAudio
libudev             # Device management</code></pre>
    </div>
    
    <h3>macOS</h3>
    <div class="code-block">
        <pre><code>// macOS frameworks
-framework OpenGL       # OpenGL framework
-framework Cocoa        # Cocoa GUI framework
-framework CoreAudio    # Audio framework
-framework AudioUnit    # Audio processing
-framework IOKit        # I/O Kit framework
-framework ForceFeedback # Force feedback
-framework Carbon       # Carbon framework</code></pre>
    </div>
</div>

<div class="section">
    <h2>Build System Integration</h2>
    
    <h3>CMake Configuration</h3>
    <div class="code-block">
        <pre><code># CMake library finding
find_package(SDL2 REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(CURL REQUIRED)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${SDL2_LIBRARIES}
    ${OPENAL_LIBRARY}
    ${Vulkan_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${PNG_LIBRARIES}
    ${CURL_LIBRARIES}
)</code></pre>
    </div>
    
    <h3>Package Managers</h3>
    <ul>
        <li><strong>vcpkg:</strong> C++ package manager (Windows)</li>
        <li><strong>Conan:</strong> Cross-platform package manager</li>
        <li><strong>apt/yum:</strong> Linux package managers</li>
        <li><strong>Homebrew:</strong> macOS package manager</li>
    </ul>
</div>

<div class="section">
    <h2>Version Management</h2>
    
    <h3>Library Versioning</h3>
    <div class="code-block">
        <pre><code># Version compatibility matrix
SDL2 >= 2.0.10          # Minimum SDL2 version
OpenAL >= 1.18.0        # OpenAL version
Vulkan >= 1.1.0         # Vulkan API version
Dear ImGui >= 1.80      # ImGui version</code></pre>
    </div>
    
    <h3>Compatibility Issues</h3>
    <ul>
        <li><strong>ABI Changes:</strong> Binary compatibility between versions</li>
        <li><strong>API Deprecation:</strong> Handling deprecated functions</li>
        <li><strong>Platform Support:</strong> Library availability per platform</li>
        <li><strong>License Compatibility:</strong> License requirements</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Common Library Issues</h3>
    <div class="troubleshooting">
        <h4>Library not found</h4>
        <ul>
            <li>Check library installation paths</li>
            <li>Verify environment variables</li>
            <li>Update package manager</li>
        </ul>
        
        <h4>Version conflicts</h4>
        <ul>
            <li>Check library version compatibility</li>
            <li>Update to compatible versions</li>
            <li>Use static linking if necessary</li>
        </ul>
        
        <h4>Linking errors</h4>
        <ul>
            <li>Verify library order in linker</li>
            <li>Check for missing dependencies</li>
            <li>Use appropriate library variants (debug/release)</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="external/opengl-extensions">OpenGL Extensions</a></li>
        <li><a href="external/audio-libraries">Audio Libraries</a></li>
        <li><a href="external/utility-libraries">Utility Libraries</a></li>
        <li><a href="getting-started/installation">Installation Guide</a></li>
    </ul>
</div> 