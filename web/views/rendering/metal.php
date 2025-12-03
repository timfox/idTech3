<?php
$title = "Metal Renderer Support";
?>

<h1>Metal Renderer Support</h1>

<p>The engine provides integration with Apple's Metal graphics API, enabling native rendering on both macOS and iOS. This renderer delivers a modern GPU-accelerated experience for Apple hardware, with a unified codebase across desktop and mobile.</p>

<h2>Overview</h2>

<p>The Metal renderer provides:</p>
<ul>
    <li>Native Metal API integration for macOS and iOS</li>
    <li>Modern GPU-accelerated rendering</li>
    <li>Unified codebase for desktop and mobile</li>
    <li>Support for Metal 1.0, 2.0, and 3.0 features</li>
</p>

<h2>Implementation Status</h2>

<table>
    <thead>
        <tr>
            <th>Feature</th>
            <th>Status</th>
        </tr>
    </thead>
    <tbody>
        <tr>
            <td>Metal device/context creation</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>Swap chain (CAMetalLayer)</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>Render pipeline setup</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>Command buffer management</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>Triple buffering</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>Metal shader compilation</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>UI/2D render path & shaders</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>2D projection matrix</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>Resource management (basic)</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>State tracking (color, depth)</td>
            <td>✅ Complete</td>
        </tr>
        <tr>
            <td>Vertex buffer pooling</td>
            <td>🚧 In Progress</td>
        </tr>
        <tr>
            <td>Texture management/loading</td>
            <td>🚧 In Progress</td>
        </tr>
        <tr>
            <td>Shader system integration</td>
            <td>🚧 In Progress</td>
        </tr>
        <tr>
            <td>Model/world rendering</td>
            <td>⏳ Planned</td>
        </tr>
        <tr>
            <td>Ray tracing (Metal 3.0)</td>
            <td>⏳ Planned</td>
        </tr>
    </tbody>
</table>

<h2>Recent Improvements</h2>

<ul>
    <li>UI render pipeline state (<code>uiPipelineState</code>)</li>
    <li>Dynamic orthographic projection for 2D rendering</li>
    <li>Separate depth-stencil state for 2D/UI</li>
    <li>Enhanced UI shaders (attribute layout, color modulation)</li>
    <li>State tracking (color, projection, buffer clearing)</li>
    <li>Drawable management (<code>currentDrawable</code>) and improved frame presentation</li>
</ul>

<h2>Current Development Focus</h2>

<ul>
    <li>Vertex buffer management for 2D/3D pipelines</li>
    <li>Texture support (format conversion, image loading, binding)</li>
    <li>Shader system connection/integration</li>
    <li>Support for structured draw calls & simple models</li>
    <li>Extended resource tracking</li>
</ul>

<h2>Future/Planned Features</h2>

<ul>
    <li>Full mesh and model rendering</li>
    <li>BSP/world rendering pipeline</li>
    <li>Lighting, post-processing, and advanced effects</li>
    <li>Metal 3 ray tracing and acceleration structures</li>
</ul>

<h2>Files</h2>

<h3>Core Renderer</h3>
<ul>
    <li><code>src/renderermetal/metal.h</code> / <code>metal.mm</code> - Metal device and context management</li>
    <li><code>src/renderermetal/tr_local.h</code> - Renderer local definitions</li>
    <li><code>src/renderermetal/tr_init.c</code> - Renderer initialization</li>
    <li><code>src/renderermetal/tr_main.c</code> - Main renderer interface</li>
</ul>

<h3>Shaders</h3>
<ul>
    <li><code>src/renderermetal/shaders/default.metal</code> - Metal Shading Language shaders</li>
</ul>

<h3>Platform Integration</h3>
<ul>
    <li><code>src/unix/macos_platform.h</code> / <code>macos_platform.mm</code> - Platform abstraction</li>
    <li><code>src/unix/ios_appdelegate.mm</code> - iOS app lifecycle</li>
    <li><code>src/unix/ios_integration.mm</code> - iOS integration</li>
</ul>

<h2>Building</h2>

<h3>macOS</h3>
<pre><code>cmake .. -DUSE_METAL=ON
make</code></pre>

<h3>iOS</h3>
<p>Requires Xcode and iOS SDK. See <a href="platform/mobile-console">Mobile/Console Platform</a> for details.</p>

<h2>Configuration</h2>

<h3>CVars</h3>
<ul>
    <li><code>r_renderer</code> - Set to "metal" to use Metal renderer</li>
    <li><code>r_metalVersion</code> - Metal version to use (1.0, 2.0, 3.0)</li>
</ul>

<h2>Performance</h2>

<p>The Metal renderer provides:</p>
<ul>
    <li>Low CPU overhead through command buffer batching</li>
    <li>Efficient GPU resource management</li>
    <li>Optimized for Apple Silicon and Intel GPUs</li>
    <li>Triple buffering for smooth frame presentation</li>
</ul>

<h2>Platform Support</h2>

<ul>
    <li><strong>macOS</strong>: Full support for macOS 10.13+</li>
    <li><strong>iOS</strong>: Full support for iOS 11.0+</li>
    <li><strong>tvOS</strong>: Planned support</li>
</ul>

<h2>See Also</h2>

<ul>
    <li><a href="platform/mobile-console">Mobile/Console Platform Support</a></li>
    <li><a href="rendering/complete-renderer">Complete Renderer Guide</a></li>
</ul>

