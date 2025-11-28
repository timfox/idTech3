<?php
/**
 * ImGui Integration through External Libraries
 */
$title = 'ImGui Integration - id Tech 3 Documentation';
$breadcrumbs = [
    '/external' => 'External Libraries',
    '/external/imgui-integration' => 'ImGui Integration'
];
?>

<h1>Dear ImGui Integration through External Libraries</h1>

<div class="section">
    <h2>Overview</h2>
    <p>This guide focuses on integrating Dear ImGui into id Tech 3 through proper external library management, providing a modern debug interface and tools integration for Quake III Arena modernization.</p>
    
    <div class="feature-list">
        <h3>Integration Benefits</h3>
        <ul>
            <li><strong>Real-time Debugging:</strong> Live engine state inspection and modification</li>
            <li><strong>Performance Profiling:</strong> Built-in profiler integration with Tracy</li>
            <li><strong>Asset Management:</strong> Live shader reloading and asset inspection</li>
            <li><strong>Development Tools:</strong> Console, memory viewer, and system metrics</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Library Dependencies</h2>
    
    <h3>Core ImGui Package</h3>
    <div class="code-block">
        <pre><code># vcpkg installation
vcpkg install imgui[core,glfw-binding,opengl3-binding,vulkan-binding]

# Conan installation  
[requires]
imgui/1.89.9

# CMake FetchContent
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
)</code></pre>
    </div>
    
    <h3>Platform Backend Libraries</h3>
    <div class="code-block">
        <pre><code># Required backend dependencies
glfw3           # Window/input handling
vulkan-headers  # Vulkan support
opengl          # OpenGL support
sdl2           # Alternative to GLFW

# Optional profiling integration
tracy          # Tracy profiler support
spdlog         # Logging library</code></pre>
    </div>
</div>

<div class="section">
    <h2>CMake Integration</h2>
    
    <h3>Modern CMake Setup</h3>
    <div class="code-block">
        <pre><code># CMakeLists.txt for ImGui integration
cmake_minimum_required(VERSION 3.20)
project(Quake3e-Modern)

# Find packages
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(imgui REQUIRED)

# ImGui component configuration
set(IMGUI_BACKENDS
    imgui::imgui_glfw
    imgui::imgui_vulkan
    imgui::imgui_opengl3
)

# Link to main target
target_link_libraries(quake3e PRIVATE
    imgui::imgui
    ${IMGUI_BACKENDS}
    glfw
    Vulkan::Vulkan
)</code></pre>
    </div>
    
    <h3>Conditional Compilation</h3>
    <div class="code-block">
        <pre><code># Feature toggles
option(USE_IMGUI "Enable Dear ImGui integration" ON)
option(USE_TRACY "Enable Tracy profiler" OFF)
option(USE_IMGUI_VULKAN "Enable Vulkan backend" ON)
option(USE_IMGUI_OPENGL "Enable OpenGL backend" ON)

if(USE_IMGUI)
    target_compile_definitions(quake3e PRIVATE USE_IMGUI=1)
    if(USE_TRACY)
        target_compile_definitions(quake3e PRIVATE USE_TRACY=1)
    endif()
endif()</code></pre>
    </div>
</div>

<div class="section">
    <h2>Engine Integration Points</h2>
    
    <h3>Initialization System</h3>
    <div class="code-block">
        <pre><code>// imgui_integration.h
#pragma once

#ifdef USE_IMGUI
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_opengl3.h"

class ImGuiManager {
public:
    static void Initialize();
    static void Shutdown();
    static void NewFrame();
    static void Render();
    
    // Engine integration
    static void ShowEngineStats();
    static void ShowRenderingDebug();
    static void ShowConsole();
    static void ShowMemoryProfile();
    
private:
    static bool initialized;
    static bool showDemo;
    static bool showEngineDebug;
};

// Global access
extern ImGuiManager* g_imguiManager;
#endif // USE_IMGUI</code></pre>
    </div>
    
    <h3>Renderer Integration</h3>
    <div class="code-block">
        <pre><code>// In tr_init.c or equivalent
void R_InitImGui(void) {
#ifdef USE_IMGUI
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Enable features
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Setup style
    ImGui::StyleColorsDark();
    
    // Platform/renderer bindings
    ImGui_ImplGlfw_InitForVulkan(tr.window, true);
    ImGui_ImplVulkan_Init(&imguiInitInfo, tr.renderPass);
    
    // Load fonts
    ImGui_ImplVulkan_CreateFontsTexture(tr.commandBuffer);
#endif
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Debug Interface Components</h2>
    
    <h3>Engine Statistics Panel</h3>
    <div class="code-block">
        <pre><code>void ImGuiManager::ShowEngineStats() {
    if (!ImGui::Begin("Engine Statistics")) {
        ImGui::End();
        return;
    }
    
    // Performance metrics
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
    
    // Memory usage
    ImGui::Separator();
    ImGui::Text("Memory Usage:");
    ImGui::Text("  Textures: %d MB", tr.memoryUsage.textures / (1024*1024));
    ImGui::Text("  Geometry: %d MB", tr.memoryUsage.geometry / (1024*1024));
    ImGui::Text("  Shaders: %d KB", tr.memoryUsage.shaders / 1024);
    
    // Rendering stats
    ImGui::Separator();
    ImGui::Text("Rendering:");
    ImGui::Text("  Draw Calls: %d", tr.stats.drawCalls);
    ImGui::Text("  Triangles: %d", tr.stats.triangles);
    ImGui::Text("  Texture Switches: %d", tr.stats.textureSwitches);
    
    ImGui::End();
}</code></pre>
    </div>
    
    <h3>Live Shader Editor</h3>
    <div class="code-block">
        <pre><code>void ImGuiManager::ShowShaderEditor() {
    if (!ImGui::Begin("Live Shader Editor")) {
        ImGui::End();
        return;
    }
    
    static char shaderCode[4096] = "";
    static int selectedShader = 0;
    
    // Shader selection
    const char* shaderNames[] = {
        "Basic Vertex", "PBR Fragment", "Shadow Map", "Postprocess"
    };
    ImGui::Combo("Shader", &selectedShader, shaderNames, IM_ARRAYSIZE(shaderNames));
    
    // Code editor
    ImGui::InputTextMultiline("##shader", shaderCode, sizeof(shaderCode), 
                             ImVec2(-1, 300));
    
    if (ImGui::Button("Compile & Reload")) {
        // Hot-reload shader
        R_ReloadShader(selectedShader, shaderCode);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset to Default")) {
        R_LoadDefaultShader(selectedShader, shaderCode, sizeof(shaderCode));
    }
    
    ImGui::End();
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tracy Profiler Integration</h2>
    
    <h3>Performance Profiling Setup</h3>
    <div class="code-block">
        <pre><code>#ifdef USE_TRACY
#include "tracy/Tracy.hpp"

// Macro for easy profiling
#define PROFILE_SCOPE(name) ZoneScoped
#define PROFILE_FUNCTION() ZoneScopedN(__FUNCTION__)
#define PROFILE_FRAME() FrameMark

// GPU profiling (Vulkan)
#define PROFILE_GPU(name) TracyVkZone(tr.tracyCtx, tr.commandBuffer, name)
#else
#define PROFILE_SCOPE(name)
#define PROFILE_FUNCTION()
#define PROFILE_FRAME()
#define PROFILE_GPU(name)
#endif

// Usage in engine code
void R_DrawWorld(void) {
    PROFILE_FUNCTION();
    
    {
        PROFILE_SCOPE("Cull Objects");
        R_CullEntities();
    }
    
    {
        PROFILE_SCOPE("Draw Opaque");
        PROFILE_GPU("Draw Opaque");
        R_DrawOpaqueObjects();
    }
}</code></pre>
    </div>
    
    <h3>ImGui Tracy Integration</h3>
    <div class="code-block">
        <pre><code>void ImGuiManager::ShowProfiler() {
#ifdef USE_TRACY
    if (!ImGui::Begin("Performance Profiler")) {
        ImGui::End();
        return;
    }
    
    // Connection status
    ImGui::Text("Tracy Connection: %s", 
               tracy::GetProfiler().IsConnected() ? "Connected" : "Disconnected");
    
    // Memory profiling
    if (ImGui::CollapsingHeader("Memory Profiling")) {
        ImGui::Text("Allocations: %llu", tracy::GetProfiler().GetFrameAllocs());
        ImGui::Text("Memory Used: %llu MB", 
                   tracy::GetProfiler().GetMemoryUsage() / (1024*1024));
    }
    
    // Frame profiling
    if (ImGui::CollapsingHeader("Frame Profiling")) {
        static float frameTimeHistory[120] = {};
        static int frameIndex = 0;
        
        frameTimeHistory[frameIndex] = ImGui::GetIO().DeltaTime * 1000.0f;
        frameIndex = (frameIndex + 1) % 120;
        
        ImGui::PlotLines("Frame Time (ms)", frameTimeHistory, 120, frameIndex, 
                        nullptr, 0.0f, 33.33f, ImVec2(0, 80));
    }
    
    ImGui::End();
#endif
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Asset Hot-Reloading</h2>
    
    <h3>File Watching System</h3>
    <div class="code-block">
        <pre><code>// Asset hot-reload integration
void ImGuiManager::ShowAssetManager() {
    if (!ImGui::Begin("Asset Manager")) {
        ImGui::End();
        return;
    }
    
    // Texture reloading
    if (ImGui::CollapsingHeader("Textures")) {
        for (int i = 0; i < tr.numTextures; i++) {
            image_t* tex = &tr.textures[i];
            ImGui::Text("%s", tex->imgName);
            ImGui::SameLine();
            
            if (ImGui::SmallButton("Reload")) {
                R_ReloadTexture(tex);
            }
        }
    }
    
    // Model reloading
    if (ImGui::CollapsingHeader("Models")) {
        // Similar interface for models
    }
    
    // Shader hot-reload
    if (ImGui::CollapsingHeader("Shaders")) {
        if (ImGui::Button("Reload All Shaders")) {
            R_ReloadAllShaders();
        }
    }
    
    ImGui::End();
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Console Integration</h2>
    
    <h3>ImGui Console Window</h3>
    <div class="code-block">
        <pre><code>void ImGuiManager::ShowConsole() {
    if (!ImGui::Begin("Console")) {
        ImGui::End();
        return;
    }
    
    // Console output
    const float footer_height = ImGui::GetStyle().ItemSpacing.y + 
                               ImGui::GetFrameHeightWithSpacing();
    
    if (ImGui::BeginChild("ConsoleOutput", ImVec2(0, -footer_height), false, 
                         ImGuiWindowFlags_HorizontalScrollbar)) {
        
        // Display console history
        for (const auto& line : consoleHistory) {
            ImVec4 color = line.type == CON_ERROR ? ImVec4(1,0,0,1) : 
                          line.type == CON_WARNING ? ImVec4(1,1,0,1) : 
                          ImVec4(1,1,1,1);
            ImGui::TextColored(color, "%s", line.text.c_str());
        }
        
        // Auto-scroll
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    
    // Command input
    static char inputBuffer[256] = "";
    if (ImGui::InputText("##Input", inputBuffer, sizeof(inputBuffer), 
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
        Cbuf_AddText(inputBuffer);
        Cbuf_AddText("\n");
        inputBuffer[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
    }
    
    ImGui::End();
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Build Configuration</h2>
    
    <h3>Platform-Specific Setup</h3>
    <div class="code-block">
        <pre><code># Windows (Visual Studio)
# In vcpkg.json
{
  "dependencies": [
    "imgui[core,glfw-binding,opengl3-binding,vulkan-binding]",
    "glfw3",
    "vulkan-headers",
    "tracy"
  ]
}

# Linux package dependencies
sudo apt-get install libglfw3-dev libvulkan-dev
# Or use system package manager

# macOS with Homebrew
brew install glfw vulkan-headers
# Then build ImGui from source</code></pre>
    </div>
    
    <h3>Deployment Considerations</h3>
    <div class="troubleshooting">
        <h4>Release Builds</h4>
        <ul>
            <li>Disable ImGui in release builds with preprocessor flags</li>
            <li>Strip debug symbols and profiling code</li>
            <li>Consider shipping with ImGui for mod developers</li>
        </ul>
        
        <h4>Performance Impact</h4>
        <ul>
            <li>ImGui rendering adds ~0.1-0.5ms overhead</li>
            <li>Tracy profiling: ~0.05ms per zone</li>
            <li>Memory overhead: ~2-5MB for full UI</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="external/libraries">External Libraries Overview</a></li>
        <li><a href="modernization/profiling-tools">Performance Profiling Tools</a></li>
        <li><a href="modernization/build-systems">Modern Build Systems</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
        <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
    </ul>
</div>