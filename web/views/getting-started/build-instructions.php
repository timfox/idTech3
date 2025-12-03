<?php
$title = "Build Instructions";
?>

<h1>Build Instructions</h1>

<p>This guide covers building the engine on various platforms.</p>

<h2>Windows/MSVC</h2>

<ol>
    <li>Install Visual Studio Community Edition 2017 or later</li>
    <li>Open <code>code/win32/msvc2017/quake3e.sln</code></li>
    <li>Compile the <code>quake3e</code> project</li>
    <li>Copy resulting exe from <code>code/win32/msvc2017/output</code> directory</li>
</ol>

<h3>Vulkan Backend</h3>
<p>To compile with Vulkan backend:</p>
<ol>
    <li>Clean solution</li>
    <li>Right click on <code>quake3e</code> project</li>
    <li>Find <code>Project Dependencies</code></li>
    <li>Select <code>renderervk</code> instead of <code>renderer</code></li>
</ol>

<h2>Windows/MSYS2</h2>

<ol>
    <li>Install MSYS2</li>
    <li>In <code>MSYS2 MSYS</code> terminal:
        <pre><code>pacman -Syu
pacman -S make mingw-w64-x86_64-gcc mingw-w64-i686-gcc</code></pre>
    </li>
    <li>Use <code>MSYS2 MINGW32</code> or <code>MSYS2 MINGW64</code> depending on your target system</li>
    <li>Copy resulting binaries from created <code>build</code> directory or use:
        <pre><code>make install DESTDIR=&lt;path_to_game_files&gt;</code></pre>
    </li>
</ol>

<h2>Windows/MinGW</h2>

<p>All build dependencies (libraries, headers) are bundled-in.</p>

<p>Build with either:</p>
<pre><code>make ARCH=x86</code></pre>
<p>or</p>
<pre><code>make ARCH=x86_64</code></pre>

<p>Copy resulting binaries from created <code>build</code> directory or use:</p>
<pre><code>make install DESTDIR=&lt;path_to_game_files&gt;</code></pre>

<h2>Generic/Ubuntu Linux/BSD</h2>

<h3>Install Dependencies</h3>
<p>Using fresh Ubuntu 18.04 installation as example:</p>
<pre><code>sudo apt install make gcc libcurl4-openssl-dev mesa-common-dev
sudo apt install libxxf86dga-dev libxrandr-dev libxxf86vm-dev libasound-dev
sudo apt install libsdl2-dev</code></pre>

<h3>Build</h3>
<pre><code>make</code></pre>

<p>Copy the resulting binaries from created <code>build</code> directory or use:</p>
<pre><code>make install DESTDIR=&lt;path_to_game_files&gt;</code></pre>

<h2>CMake Build System</h2>

<h3>Basic Build</h3>
<pre><code>mkdir build
cd build
cmake ..
make</code></pre>

<h3>CMake Options</h3>
<ul>
    <li><code>USE_VULKAN</code> - Enable Vulkan renderer (default: ON)</li>
    <li><code>USE_D3D12</code> - Enable DirectX 12 renderer (default: OFF, Windows only)</li>
    <li><code>USE_METAL</code> - Enable Metal renderer (default: OFF, macOS/iOS only)</li>
    <li><code>USE_THEORA</code> - Enable Theora codec support (default: ON)</li>
    <li><code>USE_VPX</code> - Enable VP8/VP9 codec support (default: ON)</li>
    <li><code>ENABLE_ASAN</code> - Enable AddressSanitizer (default: OFF)</li>
    <li><code>ENABLE_UBSAN</code> - Enable UndefinedBehaviorSanitizer (default: OFF)</li>
</ul>

<h3>Example: Build with Vulkan and Sanitizers</h3>
<pre><code>cmake .. -DUSE_VULKAN=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
make</code></pre>

<h2>macOS</h2>

<h3>Requirements</h3>
<ul>
    <li>Xcode Command Line Tools</li>
    <li>CMake 3.10 or later</li>
</ul>

<h3>Build</h3>
<pre><code>mkdir build
cd build
cmake .. -DUSE_METAL=ON
make</code></pre>

<h2>iOS</h2>

<p>See <a href="platform/mobile-console">Mobile/Console Platform</a> for iOS build instructions.</p>

<h2>Dependencies</h2>

<h3>Common Dependencies</h3>
<ul>
    <li>SDL2 - Window management and input</li>
    <li>libcurl - HTTP/HTTPS support</li>
    <li>OpenGL/Vulkan/Metal - Graphics API (depending on renderer)</li>
</ul>

<h3>Optional Dependencies</h3>
<ul>
    <li>libtheora - Theora video codec</li>
    <li>libvpx - VP8/VP9 video codec</li>
    <li>zlib - Compression</li>
</ul>

<h2>Troubleshooting</h2>

<h3>Missing Libraries</h3>
<p>If you encounter missing library errors, ensure all dependencies are installed. On Linux, use your distribution's package manager. On macOS, use Homebrew.</p>

<h3>CMake Configuration Errors</h3>
<p>If CMake fails to find dependencies, you may need to set <code>CMAKE_PREFIX_PATH</code>:</p>
<pre><code>cmake .. -DCMAKE_PREFIX_PATH=/usr/local</code></pre>

<h3>Build Errors</h3>
<p>Ensure you have the latest version of your compiler and build tools. Some features require C++17 or later.</p>

<h2>See Also</h2>

<ul>
    <li><a href="getting-started/installation">Installation Guide</a></li>
    <li><a href="modernization/build-systems">Build Systems</a></li>
    <li><a href="modernization/ci-cd">CI/CD Pipeline</a></li>
</ul>

