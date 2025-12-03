<?php
$title = "iOS and macOS Platform Support";
?>

<h1>Complete iOS and macOS Platform Support</h1>

<p>This document summarizes all the components added for complete iOS and macOS platform support in the engine.</p>

<h2>Overview</h2>

<p>The engine now provides full support for Apple platforms with native Metal rendering, platform abstraction layers, and proper app lifecycle management.</p>

<h2>Components Added</h2>

<h3>1. Metal Renderer</h3>
<p><strong>Files:</strong> <code>src/renderermetal/</code></p>
<ul>
    <li><code>metal.h</code> / <code>metal.mm</code> - Metal device and context management</li>
    <li><code>tr_local.h</code> - Renderer local definitions</li>
    <li><code>tr_init.c</code> - Renderer initialization</li>
    <li><code>tr_main.c</code> - Main renderer interface</li>
    <li><code>shaders/default.metal</code> - Metal Shading Language shaders</li>
</ul>

<h4>Features:</h4>
<ul>
    <li>Metal device creation and management</li>
    <li>CAMetalLayer swap chain setup</li>
    <li>Command buffer and encoder management</li>
    <li>Render pipeline state creation</li>
    <li>Shader library loading</li>
    <li>Triple buffering with semaphore synchronization</li>
    <li>Feature detection (Metal 1.0/2.0/3.0, ray tracing, argument buffers)</li>
</ul>

<h3>2. Platform Abstraction Layer</h3>
<p><strong>Files:</strong> <code>src/unix/macos_platform.*</code></p>
<ul>
    <li><code>macos_platform.h</code> - Platform API definitions</li>
    <li><code>macos_platform.mm</code> - Platform implementation (Objective-C++)</li>
</ul>

<h4>Features:</h4>
<ul>
    <li>Unified API for iOS and macOS</li>
    <li>Window/view management</li>
    <li>File system path utilities</li>
    <li>System information queries</li>
    <li>Metal feature detection</li>
    <li>Platform capability queries</li>
</ul>

<h3>3. iOS App Lifecycle</h3>
<p><strong>Files:</strong> <code>src/unix/ios_appdelegate.mm</code></p>

<h4>Features:</h4>
<ul>
    <li>UIApplicationDelegate implementation</li>
    <li>App lifecycle handling (background/foreground, memory warnings)</li>
    <li>Main entry point for iOS applications</li>
    <li>View controller setup</li>
    <li>Metal view integration</li>
</ul>

<h3>4. iOS Integration</h3>
<p><strong>Files:</strong> <code>src/unix/ios_integration.mm</code></p>

<h4>Features:</h4>
<ul>
    <li>Engine initialization on iOS</li>
    <li>View size change handling</li>
    <li>Frame update loop</li>
    <li>Memory warning handling</li>
    <li>Platform event processing</li>
</ul>

<h3>5. macOS Main Entry</h3>
<p><strong>Files:</strong> <code>src/unix/macos_main.mm</code></p>

<h4>Features:</h4>
<ul>
    <li>NSApplication initialization</li>
    <li>Window creation and management</li>
    <li>Main loop integration</li>
    <li>Platform cleanup</li>
</ul>

<h3>6. macOS Integration</h3>
<p><strong>Files:</strong> <code>src/unix/macos_integration.c</code></p>

<h4>Features:</h4>
<ul>
    <li>Integration with existing engine initialization</li>
    <li>File system path setup</li>
    <li>Platform info queries</li>
    <li>Shutdown handling</li>
</ul>

<h2>Initialization Flow</h2>

<h3>iOS</h3>
<ol>
    <li><code>main()</code> in <code>ios_appdelegate.mm</code> calls <code>UIApplicationMain()</code></li>
    <li><code>application:didFinishLaunchingWithOptions:</code> creates window and view</li>
    <li><code>iOS_InitializeEngine()</code> initializes platform and engine</li>
    <li><code>Com_Init()</code> initializes engine subsystems</li>
    <li>Renderer initialization happens when <code>CL_InitRef()</code> is called</li>
    <li>Metal renderer attaches to view via <code>Metal_InitWindow()</code></li>
</ol>

<h3>macOS</h3>
<ol>
    <li><code>main()</code> in <code>macos_main.mm</code> initializes NSApplication</li>
    <li><code>Platform_Init()</code> initializes platform abstraction</li>
    <li><code>Sys_Init_Apple()</code> sets up file system paths</li>
    <li><code>Com_Init()</code> initializes engine subsystems</li>
    <li>Window created via <code>Platform_CreateWindow()</code></li>
    <li>Metal renderer initialized via <code>Metal_InitWindow()</code></li>
    <li><code>[app run]</code> starts main loop</li>
</ol>

<h2>File System Paths</h2>

<h3>iOS</h3>
<ul>
    <li><strong>Base Path:</strong> <code>/path/to/App.app</code></li>
    <li><strong>User Path:</strong> <code>~/Library/Application Support/AppName</code></li>
    <li><strong>Resource Path:</strong> <code>/path/to/App.app/</code></li>
</ul>

<h3>macOS</h3>
<ul>
    <li><strong>Base Path:</strong> <code>/path/to/App.app/Contents/MacOS</code></li>
    <li><strong>User Path:</strong> <code>~/Library/Application Support/AppName</code></li>
    <li><strong>Resource Path:</strong> <code>/path/to/App.app/Contents/Resources</code></li>
</ul>

<h2>Build Instructions</h2>

<h3>iOS</h3>
<pre><code>cmake .. \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DUSE_METAL=ON
</code></pre>

<h3>macOS</h3>
<pre><code>cmake .. -DUSE_METAL=ON
make</code></pre>

<h2>Requirements</h2>

<h3>macOS</h3>
<ul>
    <li>macOS 10.13 (High Sierra) or later (11.0 recommended)</li>
    <li>Xcode 9.0+ with Metal tools</li>
    <li>Metal-compatible Mac GPU</li>
</ul>

<h3>iOS</h3>
<ul>
    <li>iOS 11.0 or later (13.0+ recommended)</li>
    <li>Xcode 9.0+ (with iOS SDK)</li>
    <li>Metal-compatible iPhone/iPad</li>
</ul>

<h2>See Also</h2>

<ul>
    <li><a href="rendering/metal">Metal Renderer Support</a></li>
    <li><a href="platform/mobile-console">Mobile/Console Platform</a></li>
    <li><a href="getting-started/build-instructions">Build Instructions</a></li>
</ul>

