    <div class="ascii-art">
  _     _   _______        _       ____  
 (_)   | | |__   __|      | |     |___ \ 
  _  __| |    | | ___  ___| |__     __) |
 | |/ _` |    | |/ _ \/ __| '_ \   |__ < 
 | | (_| |    | |  __| (__| | | |  ___) |
 |_|\__,_|    |_|\___|\___|_| |_| |____/ 
    </div>

    <h1>Android Implementation Guide</h1>

    <div class="section">
        <h2>Overview</h2>
        <p>This guide outlines the process of implementing Quake3e with Vulkan support for Android platforms, following successful patterns from other Quake3e forks.</p>
    </div>

    <div class="section">
        <h2>Prerequisites</h2>
        <ul>
            <li>Android NDK (r25c or later)</li>
            <li>Android SDK with build tools</li>
            <li>Vulkan SDK for Android</li>
            <li>CMake 3.22 or later</li>
            <li>Android Studio (recommended)</li>
        </ul>
    </div>

    <div class="section">
        <h2>Build System Setup</h2>
        <pre><code># CMakeLists.txt additions
set(ANDROID_ABI "arm64-v8a")
set(ANDROID_PLATFORM android-24)
set(ANDROID_STL c++_shared)
set(VULKAN_SDK $ENV{VULKAN_SDK})</code></pre>
    </div>

    <div class="section">
        <h2>Key Implementation Areas</h2>
        <ul>
            <li>Android-specific input handling</li>
            <li>Touch screen controls and UI</li>
            <li>Vulkan surface creation for Android</li>
            <li>Asset loading from APK</li>
            <li>Audio system integration</li>
            <li>Performance optimization for mobile GPUs</li>
        </ul>
    </div>

    <div class="section">
        <h2>Vulkan Implementation</h2>
        <pre><code>// Android Vulkan initialization
VkAndroidSurfaceCreateInfoKHR surfaceInfo = {
    .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
    .window = android_window
};
vkCreateAndroidSurfaceKHR(instance, &surfaceInfo, nullptr, &surface);</code></pre>
    </div>

    <div class="section">
        <h2>Performance Considerations</h2>
        <ul>
            <li>Implement dynamic resolution scaling</li>
            <li>Optimize shader complexity for mobile GPUs</li>
            <li>Use appropriate texture compression formats</li>
            <li>Implement battery-aware performance modes</li>
            <li>Optimize memory usage for mobile devices</li>
        </ul>
    </div>

    <div class="section">
        <h2>Implementation Steps</h2>
        <ol>
            <li>Set up Android project structure</li>
            <li>Implement Android-specific platform layer</li>
            <li>Port Vulkan renderer to Android</li>
            <li>Add touch controls and UI</li>
            <li>Implement asset loading system</li>
            <li>Add performance optimization features</li>
            <li>Test on various Android devices</li>
        </ol>
    </div>