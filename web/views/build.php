<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>id Tech 3 Engine Build Guide</title>
    <style>
        @font-face {
            font-family: 'Wipeout';
            src: url('/fonts/2097.ttf') format('truetype');
        }

        body {
            font-family: 'Helvetica', sans-serif;
            line-height: 1.6;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            color: #e0e0e0;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
        }

        h1, h2, h3 {
            font-family: 'Wipeout', sans-serif;
            color: #00f7ff;
            text-transform: uppercase;
            letter-spacing: 2px;
            text-shadow: 0 0 10px rgba(0, 247, 255, 0.5);
        }

        pre {
            background-color: rgba(0, 0, 0, 0.7);
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
            border: 1px solid #00f7ff;
            box-shadow: 0 0 15px rgba(0, 247, 255, 0.2);
        }

        code {
            font-family: 'Consolas', monospace;
            color: #00f7ff;
        }

        .note {
            background-color: rgba(0, 247, 255, 0.1);
            border-left: 4px solid #00f7ff;
            padding: 15px;
            margin: 15px 0;
            box-shadow: 0 0 10px rgba(0, 247, 255, 0.1);
        }
    </style>
</head>
<body>
    <h1>id Tech 3 Engine Build Guide</h1>

    <h2>Table of Contents</h2>
    <ul>
        <li><a href="#windows-msvc">Windows (MSVC)</a></li>
        <li><a href="#windows-mingw">Windows (MinGW)</a></li>
        <li><a href="#linux">Linux/Ubuntu/BSD</a></li>
        <li><a href="#arch-linux">Arch Linux</a></li>
        <li><a href="#raspberry-pi">Raspberry Pi OS</a></li>
        <li><a href="#macos">macOS</a></li>
        <li><a href="#android">Android</a></li>
        <li><a href="#build-options">Build Options</a></li>
        <li><a href="#gui-builder">GUI Builder Tool</a></li>
    </ul>

    <h2 id="windows-msvc">Windows (MSVC)</h2>
    <ol>
        <li>Install Visual Studio Community Edition 2017 or later</li>
        <li>Open the solution file: <code>code/win32/msvc2017/quake3e.sln</code></li>
        <li>Compile the <code>quake3e</code> project</li>
        <li>Find the resulting executable in <code>code/win32/msvc2017/output</code></li>
    </ol>
    <div class="note">
        <strong>Vulkan Backend:</strong> To compile with Vulkan support, clean the solution, right-click on the <code>quake3e</code> project, select "Project Dependencies" and choose <code>renderervk</code> instead of <code>renderer</code>.
    </div>

    <h2 id="windows-mingw">Windows (MinGW)</h2>
    <p>All build dependencies are included in the repository.</p>
    <pre><code>make ARCH=x86_64  # For 64-bit systems
make ARCH=x86     # For 32-bit systems</code></pre>
    <p>Copy the resulting binaries from the <code>build</code> directory or use:</p>
    <pre><code>make install DESTDIR=&lt;path_to_game_files&gt;</code></pre>

    <h2 id="linux">Linux/Ubuntu/BSD</h2>
    <p>Install required packages:</p>
    <pre><code>sudo apt install make gcc libcurl4-openssl-dev mesa-common-dev
sudo apt install libxxf86dga-dev libxrandr-dev libxxf86vm-dev libasound-dev
sudo apt install libsdl2-dev</code></pre>
    <p>Build the engine:</p>
    <pre><code>make</code></pre>
    <p>Install to a specific directory:</p>
    <pre><code>make install DESTDIR=&lt;path_to_game_files&gt;</code></pre>

    <h2 id="arch-linux">Arch Linux</h2>
    <p>Install using AUR:</p>
    <pre><code># Download the snapshot
curl -O https://aur.archlinux.org/cgit/aur.git/snapshot/quake3e-git.tar.gz

# Extract
tar xfz quake3e-git.tar.gz

# Enter directory
cd quake3e-git

# Build and install
makepkg -risc</code></pre>

    <h2 id="raspberry-pi">Raspberry Pi OS</h2>
    <p>Install dependencies:</p>
    <pre><code>apt install libsdl2-dev libxxf86dga-dev libcurl4-openssl-dev</code></pre>
    <p>Build and install:</p>
    <pre><code>make
make install DESTDIR=&lt;path_to_game_files&gt;</code></pre>

    <h2 id="macos">macOS</h2>
    <ol>
        <li>Install SDL2 framework to <code>/Library/Frameworks</code></li>
        <li>Install Vulkan support:
            <pre><code>brew install molten-vk</code></pre>
            or install the Vulkan SDK
        </li>
        <li>Build:
            <pre><code>make</code></pre>
        </li>
    </ol>
    <h2 id="android">Android</h2>
    <ol>
        <li>Install Android Studio and Android NDK:
            <pre><code># Download Android Studio from https://developer.android.com/studio
# During installation, select "Android SDK" and "Android NDK" components</code></pre>
        </li>
        <li>Set environment variables:
            <pre><code># Add to your shell profile (.bashrc, .zshrc, etc.)
export ANDROID_HOME=$HOME/Android/Sdk
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/25.2.9519653
export PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools
export ANDROID_ABI=arm64-v8a</code></pre>
        </li>
        <li>Install build dependencies:
            <pre><code># For Ubuntu/Debian
sudo apt install build-essential python3 cmake ninja-build</code></pre>
        </li>
        <li>Build the engine:
            <pre><code># Clean any previous builds
make clean

# Build with Android target
make ANDROID=1</code></pre>
        </li>
        <li>Install to Android device:
            <pre><code># Enable USB debugging on your Android device
# Connect device via USB
adb install build/android/idtech3.apk</code></pre>
        </li>
    </ol>

    <h2 id="build-options">Build Options</h2>
    <p>Available Makefile options for Linux/MinGW/macOS builds:</p>
    <ul>
        <li><code>BUILD_CLIENT=1</code> - Build unified client/server executable (default)</li>
        <li><code>BUILD_SERVER=1</code> - Build dedicated server executable (default)</li>
        <li><code>USE_SDL=0</code> - Use SDL2 backend (default, required for macOS)</li>
        <li><code>USE_VULKAN=1</code> - Build Vulkan renderer (default)</li>
        <li><code>USE_OPENGL=0</code> - Build OpenGL renderer (default)</li>
        <li><code>USE_OPENGL2=0</code> - Build OpenGL2 renderer (disabled by default)</li>
        <li><code>USE_RENDERER_DLOPEN=1</code> - Compile renderers as dynamic libraries (default)</li>
        <li><code>RENDERER_DEFAULT=vulkan</code> - Set default renderer (options: opengl, opengl2, vulkan)</li>
        <li><code>USE_SYSTEM_JPEG=0</code> - Use system JPEG library (disabled by default)</li>
    </ul>

    <h2 id="gui-builder">GUI Builder Tool</h2>
    <p>The repository includes a Python-based GUI builder tool (<code>build.pyw</code>) that provides a user-friendly interface for building the engine on Windows.</p>
    
    <h3>Features</h3>
    <ul>
        <li>Modern GUI interface with retro styling</li>
        <li>Automatic dependency checking and installation</li>
        <li>Real-time build output display</li>
        <li>Progress tracking</li>
        <li>Automatic installation to Xbox Games directory</li>
    </ul>

    <h3>Requirements</h3>
    <ul>
        <li>Python 3.x</li>
        <li>PyQt6</li>
        <li>MSYS2 (for MinGW toolchain)</li>
    </ul>

    <h3>Usage</h3>
    <ol>
        <li>Ensure MSYS2 is installed at <code>C:\msys64</code></li>
        <li>Run <code>build.pyw</code> from the engine source directory</li>
        <li>Click "Build id Tech 3 Engine" to start the build process</li>
        <li>Monitor the build progress in the output window</li>
    </ol>

    <div class="note">
        <strong>Note:</strong> The GUI builder automatically installs the engine to <code>C:\XboxGames\Quake 3\Content\EN</code> upon successful build.
    </div>
</body>
</html>
