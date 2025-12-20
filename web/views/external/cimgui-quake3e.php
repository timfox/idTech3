<?php
/**
 * CimGui Integration Walkthrough for Quake3e
 */
$title = 'CimGui + Quake3e Walkthrough - id Tech 3 Documentation';
$breadcrumbs = [
    '/external' => 'External Libraries',
    '/external/cimgui-quake3e' => 'CimGui + Quake3e Walkthrough'
];
?>

<h1>CimGui Integration Walkthrough for Quake3e</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Complete step-by-step guide to integrate CimGui (C bindings for Dear ImGui) into JKSunny's Quake3e fork. This walkthrough provides a simple debug interface overlay that works with both Vulkan and OpenGL renderers while staying in pure C.</p>
    
    <div class="feature-list">
        <h3>What You'll Get</h3>
        <ul>
            <li><strong>Debug Console:</strong> In-game overlay with engine statistics</li>
            <li><strong>Performance Monitor:</strong> Real-time FPS and frame time graphs</li>
            <li><strong>Renderer Info:</strong> GPU details and render statistics</li>
            <li><strong>Cvar Editor:</strong> Live modification of engine variables</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    
    <h3>Required Software</h3>
    <div class="code-block">
        <pre><code># Git for source control
git --version

# CMake 3.16 or newer
cmake --version

# C compiler (GCC, Clang, or MSVC)
gcc --version  # or clang --version

# Quake3e dependencies (SDL2, OpenAL, etc.)
# These should already be installed if you've built Quake3e before</code></pre>
    </div>
    
    <h3>Get JKSunny's Quake3e Fork</h3>
    <div class="code-block">
        <pre><code># Clone the repository
git clone https://github.com/jksunnny/Quake3e.git
cd Quake3e

# Create a feature branch for CimGui integration
git checkout -b feature/cimgui-integration

# Verify it builds without modifications first
mkdir build
cd build
cmake ..
make -j$(nproc)  # Linux/macOS
# OR
cmake --build . --parallel  # Cross-platform</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 1: Add CimGui Submodule</h2>
    
    <h3>Download CimGui Source</h3>
    <div class="code-block">
        <pre><code># From the root Quake3e directory
git submodule add https://github.com/cimgui/cimgui.git external/cimgui

# Initialize and update submodule
git submodule update --init --recursive

# The cimgui directory structure should look like:
# external/cimgui/
# ├── cimgui.cpp
# ├── cimgui.h  
# ├── imgui/          (Dear ImGui C++ source)
# ├── generator/      (Binding generator)
# └── CMakeLists.txt</code></pre>
    </div>
    
    <h3>Verify CimGui Structure</h3>
    <div class="code-block">
        <pre><code># Check that essential files exist
ls external/cimgui/cimgui.h          # C header
ls external/cimgui/cimgui.cpp        # C++ implementation  
ls external/cimgui/imgui/imgui.h     # Dear ImGui header
ls external/cimgui/CMakeLists.txt    # Build configuration</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 2: Modify Build System</h2>
    
    <h3>Update Root CMakeLists.txt</h3>
    <div class="code-block">
        <pre><code># Edit CMakeLists.txt in the root directory
# Add after existing options but before add_subdirectory calls

# Add CimGui option
option(USE_CIMGUI "Enable CimGui debug interface" ON)

if(USE_CIMGUI)
    # Add CimGui subdirectory
    add_subdirectory(external/cimgui)
    
    # Define preprocessor flag
    add_compile_definitions(USE_CIMGUI=1)
    
    message(STATUS "CimGui support enabled")
endif()

# Find your existing executable target (usually something like quake3e)
# and add this after it's defined:
if(USE_CIMGUI)
    target_link_libraries(quake3e PRIVATE cimgui)
    
    # Include CimGui headers
    target_include_directories(quake3e PRIVATE 
        external/cimgui
        external/cimgui/imgui
    )
endif()</code></pre>
    </div>
    
    <h3>Configure CimGui Build Options</h3>
    <div class="code-block">
        <pre><code># Create external/cimgui/CMakeLists.txt if it doesn't exist
# or modify existing one to ensure proper configuration

set(IMGUI_STATIC ON)
set(IMGUI_DEMO OFF)       # Disable demo to save space
set(IMGUI_EXAMPLES OFF)   # Disable examples

# Enable backend support based on Quake3e configuration
if(USE_RENDERER_OPENGL)
    set(IMGUI_IMPL_OPENGL3 ON)
endif()

if(USE_RENDERER_VULKAN) 
    set(IMGUI_IMPL_VULKAN ON)
endif()

# Enable SDL2 backend (Quake3e uses SDL2)
set(IMGUI_IMPL_SDL2 ON)

add_library(cimgui STATIC
    cimgui.cpp
    cimgui.h
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)

# Include directories
target_include_directories(cimgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
)

# Link with backend implementations
if(IMGUI_IMPL_SDL2)
    target_sources(cimgui PRIVATE
        imgui/backends/imgui_impl_sdl2.cpp
    )
endif()

if(IMGUI_IMPL_OPENGL3)
    target_sources(cimgui PRIVATE
        imgui/backends/imgui_impl_opengl3.cpp
    )
    find_package(OpenGL REQUIRED)
    target_link_libraries(cimgui PUBLIC ${OPENGL_LIBRARIES})
endif()

if(IMGUI_IMPL_VULKAN)
    target_sources(cimgui PRIVATE
        imgui/backends/imgui_impl_vulkan.cpp
    )
    find_package(Vulkan REQUIRED)
    target_link_libraries(cimgui PUBLIC Vulkan::Vulkan)
endif()</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 3: Create CimGui Integration Header</h2>
    
    <h3>Create src/cimgui_integration.h</h3>
    <div class="code-block">
        <pre><code>#ifndef CIMGUI_INTEGRATION_H
#define CIMGUI_INTEGRATION_H

#ifdef USE_CIMGUI

#include "cimgui.h"
#include "q_shared.h"

// Forward declarations
struct SDL_Window;
typedef union SDL_Event SDL_Event;

// CimGui integration state
typedef struct {
    qboolean initialized;
    qboolean visible;
    qboolean capture_mouse;
    qboolean capture_keyboard;
    
    // Performance tracking
    float frame_times[120];
    int frame_index;
    float current_fps;
    
    // UI state
    qboolean show_demo;
    qboolean show_performance;
    qboolean show_renderer_info;
    qboolean show_console;
    qboolean show_cvars;
    qboolean show_map_loader;
    qboolean show_quick_settings;
    qboolean show_asset_browser;
} cimgui_state_t;

// Main functions
qboolean CImGui_Init(struct SDL_Window* window);
void CImGui_Shutdown(void);
void CImGui_NewFrame(void);
void CImGui_Render(void);
qboolean CImGui_ProcessEvent(SDL_Event* event);

// UI Windows
void CImGui_ShowPerformanceWindow(void);
void CImGui_ShowRendererInfoWindow(void);
void CImGui_ShowConsoleWindow(void);
void CImGui_ShowCvarEditorWindow(void);
void CImGui_ShowEnhancedConsoleWindow(void);
void CImGui_ShowAssetBrowserWindow(void);
void CImGui_ShowMapLoaderWindow(void);
void CImGui_ShowQuickSettingsWindow(void);

// Utility functions
void CImGui_UpdatePerformanceStats(float frame_time);
void CImGui_ToggleVisibility(void);
qboolean CImGui_WantCaptureMouse(void);
qboolean CImGui_WantCaptureKeyboard(void);

// Global state (extern declaration)
extern cimgui_state_t cimgui_state;

#else

// Stub functions when CimGui is disabled
#define CImGui_Init(window) qfalse
#define CImGui_Shutdown() ((void)0)
#define CImGui_NewFrame() ((void)0)
#define CImGui_Render() ((void)0)
#define CImGui_ProcessEvent(event) qfalse
#define CImGui_UpdatePerformanceStats(time) ((void)0)
#define CImGui_ToggleVisibility() ((void)0)
#define CImGui_WantCaptureMouse() qfalse
#define CImGui_WantCaptureKeyboard() qfalse

#endif // USE_CIMGUI

#endif // CIMGUI_INTEGRATION_H</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 4: Create Configuration System</h2>
    
    <h3>Create src/cimgui_config.h</h3>
    <div class="code-block">
        <pre><code>#ifndef CIMGUI_CONFIG_H
#define CIMGUI_CONFIG_H

#ifdef USE_CIMGUI

// Configuration structure for persistent settings
typedef struct {
    // Window visibility
    qboolean show_performance;
    qboolean show_renderer_info;
    qboolean show_console;
    qboolean show_cvars;
    qboolean show_map_loader;
    qboolean show_quick_settings;
    qboolean show_asset_browser;
    qboolean show_demo;
    
    // Window positions and sizes
    float performance_pos[2];
    float performance_size[2];
    float console_pos[2];
    float console_size[2];
    
    // UI preferences
    float ui_scale;
    int ui_style; // 0=Dark, 1=Light, 2=Classic
    qboolean show_menu_bar;
    
    // Console settings
    int max_console_lines;
    qboolean auto_scroll_console;
    
    // Performance settings
    int frame_history_size;
    qboolean show_detailed_stats;
} cimgui_config_t;

// Configuration functions
void CImGui_LoadConfig(void);
void CImGui_SaveConfig(void);
void CImGui_ResetConfig(void);
cimgui_config_t* CImGui_GetConfig(void);

#endif // USE_CIMGUI
#endif // CIMGUI_CONFIG_H</code></pre>
    </div>
    
    <h3>Create src/cimgui_config.c</h3>
    <div class="code-block">
        <pre><code>#include "cimgui_config.h"

#ifdef USE_CIMGUI

#include "q_shared.h"
#include "qcommon.h"
#include <stdio.h>

static cimgui_config_t config;
static qboolean config_loaded = qfalse;

void CImGui_ResetConfig(void) {
    // Set default values
    config.show_performance = qtrue;
    config.show_renderer_info = qfalse;
    config.show_console = qfalse;
    config.show_cvars = qfalse;
    config.show_map_loader = qfalse;
    config.show_quick_settings = qfalse;
    config.show_asset_browser = qfalse;
    config.show_demo = qfalse;
    
    // Default window positions
    config.performance_pos[0] = 50.0f;
    config.performance_pos[1] = 50.0f;
    config.performance_size[0] = 300.0f;
    config.performance_size[1] = 200.0f;
    
    config.console_pos[0] = 50.0f;
    config.console_pos[1] = 300.0f;
    config.console_size[0] = 600.0f;
    config.console_size[1] = 300.0f;
    
    // UI settings
    config.ui_scale = 1.0f;
    config.ui_style = 0; // Dark theme
    config.show_menu_bar = qtrue;
    
    // Console settings
    config.max_console_lines = 1000;
    config.auto_scroll_console = qtrue;
    
    // Performance settings
    config.frame_history_size = 120;
    config.show_detailed_stats = qfalse;
    
    config_loaded = qtrue;
}

void CImGui_LoadConfig(void) {
    char path[MAX_OSPATH];
    fileHandle_t f;
    int len;
    
    Com_sprintf(path, sizeof(path), "%s/cimgui.cfg", FS_GetCurrentGameDir());
    
    len = FS_FOpenFileRead(path, &f, qfalse);
    if (!f) {
        Com_Printf("CImGui: No config file found, using defaults\n");
        CImGui_ResetConfig();
        return;
    }
    
    if (len == sizeof(cimgui_config_t)) {
        FS_Read(&config, sizeof(config), f);
        config_loaded = qtrue;
        Com_Printf("CImGui: Config loaded successfully\n");
    } else {
        Com_Printf("CImGui: Config file size mismatch, using defaults\n");
        CImGui_ResetConfig();
    }
    
    FS_FCloseFile(f);
}

void CImGui_SaveConfig(void) {
    if (!config_loaded) {
        return;
    }
    
    char path[MAX_OSPATH];
    fileHandle_t f;
    
    Com_sprintf(path, sizeof(path), "%s/cimgui.cfg", FS_GetCurrentGameDir());
    
    f = FS_FOpenFileWrite(path);
    if (!f) {
        Com_Printf("CImGui: Failed to save config file\n");
        return;
    }
    
    FS_Write(&config, sizeof(config), f);
    FS_FCloseFile(f);
    
    Com_Printf("CImGui: Config saved\n");
}

cimgui_config_t* CImGui_GetConfig(void) {
    if (!config_loaded) {
        CImGui_LoadConfig();
    }
    return &config;
}

#endif // USE_CIMGUI</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 5: Implement CimGui Integration</h2>
    
    <h3>Create src/cimgui_integration.c</h3>
    <div class="code-block">
        <pre><code>#include "cimgui_integration.h"
#include "cimgui_config.h"

#ifdef USE_CIMGUI

#include "tr_local.h"
#include "client.h"
#include <SDL.h>
#include <string.h>

// Include backend implementations
#include "imgui_impl_sdl2.h"
#ifdef USE_RENDERER_OPENGL
#include "imgui_impl_opengl3.h"
#endif
#ifdef USE_RENDERER_VULKAN  
#include "imgui_impl_vulkan.h"
#endif

// Global state
cimgui_state_t cimgui_state = {0};

// Console integration
#define MAX_CONSOLE_LINES 1000
typedef struct {
    char text[512];
    int type; // 0=normal, 1=warning, 2=error
    int timestamp;
} console_line_t;

static console_line_t console_lines[MAX_CONSOLE_LINES];
static int console_head = 0;
static int console_count = 0;

// Asset browser data
typedef struct {
    char name[64];
    int size;
    int type; // 0=texture, 1=model, 2=sound
    qboolean loaded;
} asset_info_t;

static asset_info_t* asset_list = NULL;
static int asset_count = 0;

// Forward declarations for missing functions
void CImGui_InitConsoleCapture(void);
void CImGui_AddConsoleMessage(const char* text, int type);
void CImGui_RefreshAssetList(void);
void CImGui_InitVulkanBackend(void);

qboolean CImGui_Init(SDL_Window* window) {
    if (cimgui_state.initialized) {
        return qtrue;
    }
    
    Com_Printf("CImGui: Starting initialization...\n");
    
    // Load configuration first
    CImGui_LoadConfig();
    cimgui_config_t* config = CImGui_GetConfig();
    
    // Create ImGui context
    struct ImGuiContext* ctx = igCreateContext(NULL);
    if (!ctx) {
        Com_Printf("^1CImGui: Failed to create context\n");
        return qfalse;
    }
    
    // Setup ImGui configuration
    struct ImGuiIO* io = igGetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Apply UI scaling
    if (config->ui_scale != 1.0f) {
        struct ImGuiStyle* style = igGetStyle();
        igStyleScaleAllSizes(style, config->ui_scale);
        io->FontGlobalScale = config->ui_scale;
    }
    
    // Apply theme
    switch (config->ui_style) {
        case 0: igStyleColorsDark(NULL); break;
        case 1: igStyleColorsLight(NULL); break;
        case 2: igStyleColorsClassic(NULL); break;
    }
    
    // Initialize SDL2 backend
    qboolean sdl_backend_gl = qfalse;
    
#ifdef USE_RENDERER_OPENGL
    if (r_renderer->integer == 0) {
        if (ImGui_ImplSDL2_InitForOpenGL(window, NULL)) {
            sdl_backend_gl = qtrue;
        }
    }
#endif

#ifdef USE_RENDERER_VULKAN
    if (r_renderer->integer == 1) {
        if (ImGui_ImplSDL2_InitForVulkan(window)) {
            // SDL backend initialized for Vulkan
        }
    }
#endif

    if (!sdl_backend_gl && r_renderer->integer == 0) {
        Com_Printf("^1CImGui: Failed to initialize SDL2 backend\n");
        igDestroyContext(ctx);
        return qfalse;
    }
    
    // Initialize renderer backend
    qboolean renderer_init = qfalse;
    
#ifdef USE_RENDERER_OPENGL
    if (r_renderer->integer == 0) {
        const char* glsl_version = "#version 150";
        if (ImGui_ImplOpenGL3_Init(glsl_version)) {
            renderer_init = qtrue;
            Com_Printf("CImGui: OpenGL3 backend initialized\n");
        }
    }
#endif

#ifdef USE_RENDERER_VULKAN
    if (r_renderer->integer == 1) {
        CImGui_InitVulkanBackend();
        renderer_init = qtrue;
        Com_Printf("CImGui: Vulkan backend initialized\n");
    }
#endif

    if (!renderer_init) {
        Com_Printf("^1CImGui: Failed to initialize renderer backend\n");
        ImGui_ImplSDL2_Shutdown();
        igDestroyContext(ctx);
        return qfalse;
    }
    
    // Initialize console capture
    CImGui_InitConsoleCapture();
    
    // Initialize asset browser
    CImGui_RefreshAssetList();
    
    // Copy config values to state
    cimgui_state.show_performance = config->show_performance;
    cimgui_state.show_renderer_info = config->show_renderer_info;
    cimgui_state.show_console = config->show_console;
    cimgui_state.show_cvars = config->show_cvars;
    
    // Initialize state
    cimgui_state.initialized = qtrue;
    cimgui_state.visible = qtrue;
    cimgui_state.frame_index = 0;
    
    Com_Printf("^2CImGui: Initialization successful\n");
    return qtrue;
}

void CImGui_Shutdown(void) {
    if (!cimgui_state.initialized) {
        return;
    }
    
    // Shutdown backends
#ifdef USE_RENDERER_OPENGL
    ImGui_ImplOpenGL3_Shutdown();
#endif
#ifdef USE_RENDERER_VULKAN
    // ImGui_ImplVulkan_Shutdown();
#endif
    ImGui_ImplSDL2_Shutdown();
    
    // Destroy context
    igDestroyContext(NULL);
    
    cimgui_state.initialized = qfalse;
    Com_Printf("CImGui: Shutdown complete\n");
}

void CImGui_NewFrame(void) {
    if (!cimgui_state.initialized || !cimgui_state.visible) {
        return;
    }
    
    // Start the frame
#ifdef USE_RENDERER_OPENGL
    if (r_renderer->integer == 0) {
        ImGui_ImplOpenGL3_NewFrame();
    }
#endif
#ifdef USE_RENDERER_VULKAN
    if (r_renderer->integer == 1) {
        // ImGui_ImplVulkan_NewFrame();
    }
#endif
    
    ImGui_ImplSDL2_NewFrame();
    igNewFrame();
}

void CImGui_Render(void) {
    if (!cimgui_state.initialized || !cimgui_state.visible) {
        return;
    }
    
    // Show UI windows
    if (cimgui_state.show_performance) {
        CImGui_ShowPerformanceWindow();
    }
    
    if (cimgui_state.show_renderer_info) {
        CImGui_ShowRendererInfoWindow();
    }
    
    if (cimgui_state.show_console) {
        CImGui_ShowConsoleWindow();
    }
    
    if (cimgui_state.show_cvars) {
        CImGui_ShowCvarEditorWindow();
    }
    
    if (cimgui_state.show_demo) {
        igShowDemoWindow(&cimgui_state.show_demo);
    }
    
    // Main menu bar
    if (igBeginMainMenuBar()) {
        if (igBeginMenu("Windows", true)) {
            igMenuItem_BoolPtr("Performance", NULL, &cimgui_state.show_performance, true);
            igMenuItem_BoolPtr("Renderer Info", NULL, &cimgui_state.show_renderer_info, true);
            igMenuItem_BoolPtr("Console", NULL, &cimgui_state.show_console, true);
            igMenuItem_BoolPtr("CVars", NULL, &cimgui_state.show_cvars, true);
            igSeparator();
            igMenuItem_BoolPtr("ImGui Demo", NULL, &cimgui_state.show_demo, true);
            igEndMenu();
        }
        igEndMainMenuBar();
    }
    
    // Render
    igRender();
    
    struct ImDrawData* draw_data = igGetDrawData();
    
#ifdef USE_RENDERER_OPENGL
    if (r_renderer->integer == 0) {
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    }
#endif
#ifdef USE_RENDERER_VULKAN
    if (r_renderer->integer == 1) {
        // ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
    }
#endif
}

qboolean CImGui_ProcessEvent(SDL_Event* event) {
    if (!cimgui_state.initialized) {
        return qfalse;
    }
    
    // Toggle visibility with F1
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_F1) {
        CImGui_ToggleVisibility();
        return qtrue; // Consume the event
    }
    
    if (!cimgui_state.visible) {
        return qfalse;
    }
    
    ImGui_ImplSDL2_ProcessEvent(event);
    
    // Return true if ImGui wants to capture this event
    struct ImGuiIO* io = igGetIO();
    return (io->WantCaptureMouse && (event->type == SDL_MOUSEBUTTONDOWN || 
                                    event->type == SDL_MOUSEBUTTONUP ||
                                    event->type == SDL_MOUSEMOTION)) ||
           (io->WantCaptureKeyboard && (event->type == SDL_KEYDOWN || 
                                       event->type == SDL_KEYUP));
}

void CImGui_UpdatePerformanceStats(float frame_time) {
    if (!cimgui_state.initialized) {
        return;
    }
    
    cimgui_state.frame_times[cimgui_state.frame_index] = frame_time;
    cimgui_state.frame_index = (cimgui_state.frame_index + 1) % 120;
    cimgui_state.current_fps = 1000.0f / frame_time;
}

void CImGui_ToggleVisibility(void) {
    cimgui_state.visible = !cimgui_state.visible;
    Com_Printf("CImGui: %s\n", cimgui_state.visible ? "Enabled" : "Disabled");
}

qboolean CImGui_WantCaptureMouse(void) {
    if (!cimgui_state.initialized || !cimgui_state.visible) {
        return qfalse;
    }
    
    struct ImGuiIO* io = igGetIO();
    return io->WantCaptureMouse;
}

qboolean CImGui_WantCaptureKeyboard(void) {
    if (!cimgui_state.initialized || !cimgui_state.visible) {
        return qfalse;
    }
    
    struct ImGuiIO* io = igGetIO();
    return io->WantCaptureKeyboard;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 5: Implement UI Windows</h2>
    
    <h3>Add to cimgui_integration.c (continued)</h3>
    <div class="code-block">
        <pre><code>void CImGui_ShowPerformanceWindow(void) {
    if (!igBegin("Performance", &cimgui_state.show_performance, 0)) {
        igEnd();
        return;
    }
    
    // FPS display
    igText("FPS: %.1f", cimgui_state.current_fps);
    igText("Frame Time: %.2f ms", 1000.0f / cimgui_state.current_fps);
    
    // Frame time graph
    igPlotLines_FloatPtr("Frame Times", cimgui_state.frame_times, 120, 
                        cimgui_state.frame_index, NULL, 0.0f, 33.33f, 
                        (ImVec2){0, 80}, sizeof(float));
    
    igSeparator();
    
    // Memory usage (if available)
    if (Hunk_MemoryRemaining) {
        int used = Hunk_MemoryRemaining() / (1024 * 1024);
        igText("Hunk Memory: %d MB used", used);
    }
    
    igEnd();
}

void CImGui_ShowRendererInfoWindow(void) {
    if (!igBegin("Renderer Info", &cimgui_state.show_renderer_info, 0)) {
        igEnd();
        return;
    }
    
    igText("Renderer: %s", r_renderer->integer == 0 ? "OpenGL" : "Vulkan");
    
    if (glConfig.renderer_string) {
        igText("GPU: %s", glConfig.renderer_string);
    }
    
    if (glConfig.vendor_string) {
        igText("Vendor: %s", glConfig.vendor_string);
    }
    
    if (glConfig.version_string) {
        igText("Version: %s", glConfig.version_string);
    }
    
    igSeparator();
    igText("Screen: %dx%d", glConfig.vidWidth, glConfig.vidHeight);
    
    // Render stats (if available in tr.stats)
    if (tr.frameCount > 0) {
        igSeparator();
        igText("Draw Calls: %d", tr.pc.c_drawCalls);
        igText("Vertices: %d", tr.pc.c_vertexes);
        igText("Indexes: %d", tr.pc.c_indexes);
    }
    
    igEnd();
}

void CImGui_ShowConsoleWindow(void) {
    if (!igBegin("Console", &cimgui_state.show_console, 0)) {
        igEnd();
        return;
    }
    
    igText("Quake3e Console Integration");
    igSeparator();
    
    // Simple console output area
    igBeginChild_Str("ConsoleOutput", (ImVec2){0, -25}, true, 0);
    
    // TODO: Hook into Quake3e's console system to display recent messages
    igText("Console integration requires hooking into");
    igText("the game's console message system.");
    igText("Press ~ to access the game console.");
    
    igEndChild();
    
    // Command input
    static char input_buffer[256] = "";
    if (igInputText("##input", input_buffer, sizeof(input_buffer), 
                   ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL)) {
        if (strlen(input_buffer) > 0) {
            // Execute command
            Cbuf_AddText(input_buffer);
            Cbuf_AddText("\n");
            Cbuf_Execute();
            
            // Clear input
            input_buffer[0] = '\0';
            igSetKeyboardFocusHere(-1);
        }
    }
    
    igEnd();
}

void CImGui_ShowCvarEditorWindow(void) {
    if (!igBegin("CVar Editor", &cimgui_state.show_cvars, 0)) {
        igEnd();
        return;
    }
    
    igText("Common CVars:");
    igSeparator();
    
    // Some common cvars to edit
    static float r_gamma_val = 1.0f;
    static int r_mode_val = 3;
    static int com_maxfps_val = 125;
    
    // Get actual values
    if (r_gamma) r_gamma_val = r_gamma->value;
    if (r_mode) r_mode_val = r_mode->integer;
    if (com_maxfps) com_maxfps_val = com_maxfps->integer;
    
    // Edit gamma
    if (igSliderFloat("r_gamma", &r_gamma_val, 0.5f, 3.0f, "%.2f", 0)) {
        if (r_gamma) {
            Cvar_SetValue("r_gamma", r_gamma_val);
        }
    }
    
    // Edit video mode
    if (igSliderInt("r_mode", &r_mode_val, 0, 11, "%d", 0)) {
        if (r_mode) {
            Cvar_SetValue("r_mode", r_mode_val);
        }
    }
    
    // Edit max FPS
    if (igSliderInt("com_maxfps", &com_maxfps_val, 30, 1000, "%d", 0)) {
        if (com_maxfps) {
            Cvar_SetValue("com_maxfps", com_maxfps_val);
        }
    }
    
    igSeparator();
    
    if (igButton("Apply Video Changes", (ImVec2){0, 0})) {
        Cbuf_AddText("vid_restart\n");
        Cbuf_Execute();
    }
    
    igEnd();
}

// Console capture implementation
void CImGui_InitConsoleCapture(void) {
    console_head = 0;
    console_count = 0;
    Com_Printf("CImGui: Console capture initialized\n");
}

void CImGui_AddConsoleMessage(const char* text, int type) {
    if (!text || console_count >= MAX_CONSOLE_LINES) {
        return;
    }
    
    console_line_t* line = &console_lines[console_head];
    Q_strncpyz(line->text, text, sizeof(line->text));
    line->type = type;
    line->timestamp = Sys_Milliseconds();
    
    console_head = (console_head + 1) % MAX_CONSOLE_LINES;
    if (console_count < MAX_CONSOLE_LINES) {
        console_count++;
    }
}

// Asset browser implementation  
void CImGui_RefreshAssetList(void) {
    if (asset_list) {
        Z_Free(asset_list);
        asset_list = NULL;
        asset_count = 0;
    }
    
    int texture_count = 0;
    for (int i = 0; i < tr.numTextures; i++) {
        if (tr.textures[i].imgName[0]) {
            texture_count++;
        }
    }
    
    asset_count = texture_count;
    if (asset_count > 0) {
        asset_list = Z_Malloc(asset_count * sizeof(asset_info_t));
        
        int index = 0;
        for (int i = 0; i < tr.numTextures && index < asset_count; i++) {
            if (tr.textures[i].imgName[0]) {
                asset_info_t* asset = &asset_list[index];
                Q_strncpyz(asset->name, tr.textures[i].imgName, sizeof(asset->name));
                asset->size = tr.textures[i].uploadWidth * tr.textures[i].uploadHeight * 4;
                asset->type = 0;
                asset->loaded = qtrue;
                index++;
            }
        }
    }
}

// Vulkan backend implementation
void CImGui_InitVulkanBackend(void) {
#ifdef USE_RENDERER_VULKAN
    Com_Printf("CImGui: Vulkan backend initialization\n");
    // Full implementation would require integration with tr_vk.c
#endif
}

// Enhanced console window with proper integration
void CImGui_ShowEnhancedConsoleWindow(void) {
    cimgui_config_t* config = CImGui_GetConfig();
    
    igSetNextWindowPos((ImVec2){config->console_pos[0], config->console_pos[1]}, ImGuiCond_FirstUseEver, (ImVec2){0,0});
    igSetNextWindowSize((ImVec2){config->console_size[0], config->console_size[1]}, ImGuiCond_FirstUseEver);
    
    if (!igBegin("Enhanced Console", &cimgui_state.show_console, 0)) {
        igEnd();
        return;
    }
    
    // Console output area
    const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    
    if (igBeginChild_Str("ConsoleOutput", (ImVec2){0, -footer_height}, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        
        // Display console history
        for (int i = 0; i < console_count; i++) {
            int idx = (console_head + i) % MAX_CONSOLE_LINES;
            console_line_t* line = &console_lines[idx];
            
            ImVec4 color;
            switch (line->type) {
                case 1: color = (ImVec4){1.0f, 1.0f, 0.0f, 1.0f}; break; // Warning
                case 2: color = (ImVec4){1.0f, 0.0f, 0.0f, 1.0f}; break; // Error
                default: color = (ImVec4){1.0f, 1.0f, 1.0f, 1.0f}; break; // Normal
            }
            
            igTextColored(color, "%s", line->text);
        }
        
        if (config->auto_scroll_console && igGetScrollY() >= igGetScrollMaxY()) {
            igSetScrollHereY(1.0f);
        }
    }
    igEndChild();
    
    // Command input
    static char input_buffer[256] = "";
    igPushItemWidth(-1);
    if (igInputText("##ConsoleInput", input_buffer, sizeof(input_buffer), 
                   ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL)) {
        if (strlen(input_buffer) > 0) {
            // Add to console history
            CImGui_AddConsoleMessage(va("] %s", input_buffer), 0);
            
            // Execute command
            Cbuf_AddText(input_buffer);
            Cbuf_AddText("\n");
            Cbuf_Execute();
            
            input_buffer[0] = '\0';
            igSetKeyboardFocusHere(-1);
        }
    }
    igPopItemWidth();
    
    igEnd();
}

// Asset browser window
void CImGui_ShowAssetBrowserWindow(void) {
    if (!igBegin("Asset Browser", &cimgui_state.show_asset_browser, 0)) {
        igEnd();
        return;
    }
    
    if (igButton("Refresh", (ImVec2){0, 0})) {
        CImGui_RefreshAssetList();
    }
    
    igSameLine(0, 10);
    igText("Assets: %d", asset_count);
    
    igSeparator();
    
    // Filter
    static char filter[64] = "";
    igInputText("Filter", filter, sizeof(filter), 0, NULL, NULL);
    
    // Asset list
    if (igBeginChild_Str("AssetList", (ImVec2){0, 0}, true, 0)) {
        
        for (int i = 0; i < asset_count; i++) {
            asset_info_t* asset = &asset_list[i];
            
            // Apply filter
            if (filter[0] && !Q_stristr(asset->name, filter)) {
                continue;
            }
            
            // Asset type icon
            const char* type_str = "TEX";
            if (asset->type == 1) type_str = "MDL";
            else if (asset->type == 2) type_str = "SND";
            
            igText("[%s] %s (%d KB)", type_str, asset->name, asset->size / 1024);
            
            if (igIsItemClicked(0)) {
                Com_Printf("Selected asset: %s\n", asset->name);
            }
        }
    }
    igEndChild();
    
    igEnd();
}

#endif // USE_CIMGUI</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 6: Build Scripts and Automation</h2>
    
    <h3>Create build-cimgui.sh (Linux/macOS)</h3>
    <div class="code-block">
        <pre><code>#!/bin/bash
# build-cimgui.sh - Automated build script for CimGui integration

set -e

echo "Quake3e CimGui Integration Build Script"
echo "======================================"

# Check for required tools
command -v git >/dev/null 2>&1 || { echo "Git is required but not installed."; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "CMake is required but not installed."; exit 1; }

# Configuration
BUILD_TYPE=${1:-Release}
BUILD_DIR="build-cimgui"
PARALLEL_JOBS=$(nproc)

echo "Build type: $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo "Parallel jobs: $PARALLEL_JOBS"

# Initialize submodules
echo "Initializing CimGui submodule..."
git submodule update --init --recursive external/cimgui

# Create build directory
echo "Creating build directory..."
rm -rf $BUILD_DIR
mkdir $BUILD_DIR
cd $BUILD_DIR

# Configure
echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DUSE_CIMGUI=ON \
    -DUSE_RENDERER_OPENGL=ON \
    -DUSE_RENDERER_VULKAN=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo "Building..."
cmake --build . --parallel $PARALLEL_JOBS

# Success message
echo ""
echo "Build completed successfully!"
echo "Binary location: $BUILD_DIR/quake3e"
echo ""
echo "To run with CimGui:"
echo "  cd $BUILD_DIR"
echo "  ./quake3e"
echo ""
echo "Press F1 in-game to toggle the CimGui interface"</code></pre>
    </div>
    
    <h3>Create build-cimgui.bat (Windows)</h3>
    <div class="code-block">
        <pre><code>@echo off
REM build-cimgui.bat - Windows build script for CimGui integration

echo Quake3e CimGui Integration Build Script
echo ======================================

REM Check for required tools
where git >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Git is required but not found in PATH
    exit /b 1
)

where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo CMake is required but not found in PATH
    exit /b 1
)

REM Configuration
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
set BUILD_DIR=build-cimgui

echo Build type: %BUILD_TYPE%
echo Build directory: %BUILD_DIR%

REM Initialize submodules
echo Initializing CimGui submodule...
git submodule update --init --recursive external/cimgui

REM Create build directory
echo Creating build directory...
if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
mkdir %BUILD_DIR%
cd %BUILD_DIR%

REM Configure
echo Configuring with CMake...
cmake .. ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DUSE_CIMGUI=ON ^
    -DUSE_RENDERER_OPENGL=ON ^
    -DUSE_RENDERER_VULKAN=ON ^
    -A x64

REM Build
echo Building...
cmake --build . --config %BUILD_TYPE% --parallel

REM Success message
echo.
echo Build completed successfully!
echo Binary location: %BUILD_DIR%\%BUILD_TYPE%\quake3e.exe
echo.
echo To run with CimGui:
echo   cd %BUILD_DIR%\%BUILD_TYPE%
echo   quake3e.exe
echo.
echo Press F1 in-game to toggle the CimGui interface

pause</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 7: Console Integration Hook</h2>
    
    <h3>Modify src/common/common.c</h3>
    <div class="code-block">
        <pre><code>// Add include for CimGui
#include "cimgui_integration.h"

// In Com_Printf function, add console capture:
void QDECL Com_Printf( const char *fmt, ... ) {
    va_list     argptr;
    char        msg[MAXPRINTMSG];
    
    va_start(argptr, fmt);
    Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);
    
    // Existing console output code...
    
#ifdef USE_CIMGUI
    // Capture for CimGui console
    int msg_type = 0; // Normal message
    if (strstr(msg, "WARNING") || strstr(msg, "^3")) {
        msg_type = 1; // Warning
    } else if (strstr(msg, "ERROR") || strstr(msg, "^1")) {
        msg_type = 2; // Error
    }
    
    CImGui_AddConsoleMessage(msg, msg_type);
#endif
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 8: Integrate with Quake3e Main Loop</h2>
    
    <h3>Modify src/client/cl_main.c</h3>
    <div class="code-block">
        <pre><code>// Add near the top with other includes
#include "cimgui_integration.h"

// In CL_Init() function, add after SDL initialization:
void CL_Init( void ) {
    // ... existing initialization code ...
    
#ifdef USE_CIMGUI
    // Initialize CimGui after SDL and renderer are set up
    if (cls.glw_state.window) {
        if (CImGui_Init(cls.glw_state.window)) {
            Com_Printf("CimGui initialization successful\n");
        } else {
            Com_Printf("CimGui initialization failed\n");
        }
    }
#endif

    // ... rest of existing code ...
}

// In CL_Shutdown() function:
void CL_Shutdown( void ) {
#ifdef USE_CIMGUI
    CImGui_Shutdown();
#endif

    // ... existing shutdown code ...
}</code></pre>
    </div>
    
    <h3>Modify src/client/cl_input.c</h3>
    <div class="code-block">
        <pre><code>// Add include at top
#include "cimgui_integration.h"

// In CL_ProcessEvent() function, add at the beginning:
void CL_ProcessEvent( SDL_Event *event ) {
#ifdef USE_CIMGUI
    // Let CimGui process the event first
    if (CImGui_ProcessEvent(event)) {
        return; // Event was consumed by CimGui
    }
#endif

    // ... existing event processing code ...
}</code></pre>
    </div>
    
    <h3>Modify src/renderercommon/tr_common.c (or similar render file)</h3>
    <div class="code-block">
        <pre><code>// Add include
#include "cimgui_integration.h"

// In the main render function (varies by renderer), add:
void R_RenderView( void ) {
    // ... existing render code ...
    
#ifdef USE_CIMGUI
    // Start CimGui frame
    CImGui_NewFrame();
    
    // Update performance stats
    static int lastTime = 0;
    int currentTime = Sys_Milliseconds();
    if (lastTime > 0) {
        float frameTime = currentTime - lastTime;
        CImGui_UpdatePerformanceStats(frameTime);
    }
    lastTime = currentTime;
    
    // Render CimGui
    CImGui_Render();
#endif

    // ... rest of existing render code ...
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 7: Build and Test</h2>
    
    <h3>Clean Build</h3>
    <div class="code-block">
        <pre><code># Clean previous build
rm -rf build
mkdir build
cd build

# Configure with CimGui enabled
cmake .. -DUSE_CIMGUI=ON

# Build
make -j$(nproc)  # Linux/macOS
# OR
cmake --build . --parallel  # Cross-platform

# The binary should be larger due to CimGui inclusion
ls -la quake3e*</code></pre>
    </div>
    
    <h3>Test the Integration</h3>
    <div class="code-block">
        <pre><code># Run Quake3e
./quake3e

# You should see:
# 1. "CimGui initialization successful" in console
# 2. Menu bar at top of screen
# 3. Performance window showing FPS
# 4. Press F1 to toggle CimGui visibility

# Test functionality:
# - F1 key toggles the UI
# - Performance window shows real-time FPS
# - Renderer info shows GPU details
# - CVar editor allows live adjustments
# - All windows are draggable and resizable</code></pre>
    </div>
    
    <h3>Troubleshooting</h3>
    <div class="troubleshooting">
        <h4>Build Errors</h4>
        <ul>
            <li><strong>Missing cimgui.h:</strong> Ensure submodule was initialized: `git submodule update --init --recursive`</li>
            <li><strong>Linker errors:</strong> Check that cimgui is properly linked in CMakeLists.txt</li>
            <li><strong>OpenGL errors:</strong> Verify OpenGL context is created before CImGui_Init()</li>
        </ul>
        
        <h4>Runtime Issues</h4>
        <ul>
            <li><strong>UI not appearing:</strong> Check console for "CimGui initialization successful"</li>
            <li><strong>Crash on startup:</strong> Ensure SDL2 window is valid before initialization</li>
            <li><strong>F1 not working:</strong> Verify event processing order in CL_ProcessEvent()</li>
        </ul>
        
        <h4>Performance Issues</h4>
        <ul>
            <li><strong>FPS drop:</strong> CimGui adds ~0.1-1ms overhead, disable if needed</li>
            <li><strong>Memory usage:</strong> UI allocates ~2-5MB, monitor with performance window</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Step 8: Adding Custom Features</h2>
    
    <h3>Add a Map Loader Window</h3>
    <div class="code-block">
        <pre><code>// Add to cimgui_integration.c
void CImGui_ShowMapLoaderWindow(void) {
    if (!igBegin("Map Loader", &cimgui_state.show_map_loader, 0)) {
        igEnd();
        return;
    }
    
    // List of common maps
    const char* maps[] = {
        "q3dm1", "q3dm2", "q3dm3", "q3dm4", "q3dm5", "q3dm6",
        "q3dm7", "q3dm8", "q3dm9", "q3dm10", "q3dm11", "q3dm12",
        "q3dm13", "q3dm14", "q3dm15", "q3dm16", "q3dm17", "q3dm18"
    };
    
    igText("Select a map to load:");
    igSeparator();
    
    for (int i = 0; i < sizeof(maps)/sizeof(maps[0]); i++) {
        if (igButton(maps[i], (ImVec2){100, 25})) {
            char command[64];
            Com_sprintf(command, sizeof(command), "map %s\n", maps[i]);
            Cbuf_AddText(command);
            Cbuf_Execute();
        }
        
        // 3 buttons per row
        if ((i + 1) % 3 != 0) {
            igSameLine(0, 5);
        }
    }
    
    igEnd();
}</code></pre>
    </div>
    
    <h3>Add Quick Settings Panel</h3>
    <div class="code-block">
        <pre><code>// Add to cimgui_integration.c  
void CImGui_ShowQuickSettingsWindow(void) {
    if (!igBegin("Quick Settings", &cimgui_state.show_quick_settings, 0)) {
        igEnd();
        return;
    }
    
    // Graphics quality presets
    igText("Graphics Presets:");
    if (igButton("Low Quality", (ImVec2){0, 0})) {
        Cbuf_AddText("r_picmip 2; r_subdivisions 20; r_simpleMipMaps 1\n");
        Cbuf_Execute();
    }
    
    if (igButton("Medium Quality", (ImVec2){0, 0})) {
        Cbuf_AddText("r_picmip 1; r_subdivisions 12; r_simpleMipMaps 0\n");
        Cbuf_Execute();
    }
    
    if (igButton("High Quality", (ImVec2){0, 0})) {
        Cbuf_AddText("r_picmip 0; r_subdivisions 4; r_simpleMipMaps 0\n");
        Cbuf_Execute();
    }
    
    igSeparator();
    
    // Quick toggles
    igText("Quick Toggles:");
    
    if (igButton("Toggle Fullscreen", (ImVec2){0, 0})) {
        Cbuf_AddText("toggle r_fullscreen; vid_restart\n");
        Cbuf_Execute();
    }
    
    if (igButton("Toggle VSync", (ImVec2){0, 0})) {
        Cbuf_AddText("toggle r_swapInterval\n");
        Cbuf_Execute();
    }
    
    igEnd();
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Step 9: Final Integration</h2>
    
    <h3>Update Main Menu Bar</h3>
    <div class="code-block">
        <pre><code>// In CImGui_Render() function, update the menu bar:
if (igBeginMainMenuBar()) {
    if (igBeginMenu("Windows", true)) {
        igMenuItem_BoolPtr("Performance", NULL, &cimgui_state.show_performance, true);
        igMenuItem_BoolPtr("Renderer Info", NULL, &cimgui_state.show_renderer_info, true);
        igMenuItem_BoolPtr("Console", NULL, &cimgui_state.show_console, true);
        igMenuItem_BoolPtr("CVars", NULL, &cimgui_state.show_cvars, true);
        igSeparator();
        igMenuItem_BoolPtr("Map Loader", NULL, &cimgui_state.show_map_loader, true);
        igMenuItem_BoolPtr("Quick Settings", NULL, &cimgui_state.show_quick_settings, true);
        igSeparator();
        igMenuItem_BoolPtr("ImGui Demo", NULL, &cimgui_state.show_demo, true);
        igEndMenu();
    }
    
    if (igBeginMenu("Help", true)) {
        igMenuItem_Bool("About CimGui", NULL, false, true);
        if (igMenuItem_Bool("Toggle with F1", NULL, false, true)) {
            // Show help text
        }
        igEndMenu();
    }
    
    igEndMainMenuBar();
}</code></pre>
    </div>
    
    <h3>Add Keyboard Shortcuts</h3>
    <div class="code-block">
        <pre><code>// In CImGui_ProcessEvent(), add more shortcuts:
qboolean CImGui_ProcessEvent(SDL_Event* event) {
    if (!cimgui_state.initialized) {
        return qfalse;
    }
    
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_F1:
                CImGui_ToggleVisibility();
                return qtrue;
                
            case SDLK_F2:
                if (cimgui_state.visible) {
                    cimgui_state.show_performance = !cimgui_state.show_performance;
                    return qtrue;
                }
                break;
                
            case SDLK_F3:
                if (cimgui_state.visible) {
                    cimgui_state.show_console = !cimgui_state.show_console;
                    return qtrue;
                }
                break;
        }
    }
    
    // ... rest of function
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Commit Your Changes</h2>
    
    <h3>Final Build and Test</h3>
    <div class="code-block">
        <pre><code># Final clean build
cd build
make clean
make -j$(nproc)

# Test all features:
# - F1: Toggle UI
# - F2: Toggle Performance window
# - F3: Toggle Console window
# - Menu bar navigation
# - Window dragging/resizing
# - CVar editing
# - Map loading</code></pre>
    </div>
    
    <h3>Commit to Git</h3>
    <div class="code-block">
        <pre><code># Add all new files
git add external/cimgui
git add src/cimgui_integration.h
git add src/cimgui_integration.c
git add CMakeLists.txt

# Commit changes
git commit -m "Add CimGui integration for debug interface

- Added CimGui as submodule
- Implemented basic UI with performance monitoring
- Added renderer info, console, and cvar editor windows
- Integrated with main game loop and event system
- F1 toggles UI visibility
- Pure C implementation, no C++ required"

# Push to your fork
git push origin feature/cimgui-integration</code></pre>
    </div>
</div>

<div class="section">
    <h2>Testing and Validation</h2>
    
    <h3>Complete Test Checklist</h3>
    <div class="code-block">
        <pre><code># 1. Basic Functionality Test
./quake3e
# - Check console for "CimGui initialization successful"
# - Press F1 - UI should appear/disappear
# - Performance window should show real-time FPS

# 2. Window Management Test
# - Drag windows around
# - Resize windows
# - Close and reopen windows from menu
# - Check that positions are saved on restart

# 3. Console Integration Test
# - Open Enhanced Console window
# - Type commands and verify they execute
# - Check that engine messages appear with color coding
# - Test command history and auto-scroll

# 4. Asset Browser Test
# - Open Asset Browser
# - Click "Refresh" - should show loaded textures
# - Test filtering functionality
# - Verify asset information is accurate

# 5. Performance Impact Test
# - Record FPS without CimGui (F1 to hide)
# - Record FPS with CimGui visible
# - Overhead should be < 2-3ms
# - Memory usage increase should be < 10MB

# 6. Configuration Persistence Test
# - Arrange windows in custom layout
# - Change UI theme and scale
# - Restart game - settings should be preserved
# - Check for cimgui.cfg file creation</code></pre>
    </div>
    
    <h3>Performance Benchmarks</h3>
    <div class="troubleshooting">
        <h4>Expected Performance Impact</h4>
        <ul>
            <li><strong>Frame Time:</strong> +0.1-1.0ms overhead</li>
            <li><strong>Memory Usage:</strong> +2-8MB depending on windows open</li>
            <li><strong>Binary Size:</strong> +2-4MB due to CimGui library</li>
            <li><strong>Startup Time:</strong> +50-100ms for initialization</li>
        </ul>
        
        <h4>Performance Optimizations</h4>
        <ul>
            <li>Hide unused windows to reduce draw calls</li>
            <li>Reduce frame history size for performance window</li>
            <li>Disable asset browser auto-refresh on large projects</li>
            <li>Use release builds for final testing</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Advanced Configuration</h2>
    
    <h3>CVar Integration</h3>
    <div class="code-block">
        <pre><code>// Add to cimgui_integration.c initialization
// Register CVars for runtime control
cvar_t *cimgui_enable;
cvar_t *cimgui_scale;
cvar_t *cimgui_theme;
cvar_t *cimgui_show_menu;

void CImGui_RegisterCvars(void) {
    cimgui_enable = Cvar_Get("cimgui_enable", "1", CVAR_ARCHIVE);
    cimgui_scale = Cvar_Get("cimgui_scale", "1.0", CVAR_ARCHIVE);
    cimgui_theme = Cvar_Get("cimgui_theme", "0", CVAR_ARCHIVE);
    cimgui_show_menu = Cvar_Get("cimgui_show_menu", "1", CVAR_ARCHIVE);
}

// Console commands
void CImGui_Toggle_f(void) {
    CImGui_ToggleVisibility();
}

void CImGui_Reset_f(void) {
    CImGui_ResetConfig();
    Com_Printf("CimGui configuration reset to defaults\n");
}

void CImGui_Save_f(void) {
    CImGui_SaveConfig();
    Com_Printf("CimGui configuration saved\n");
}

// Register commands in initialization
Cmd_AddCommand("cimgui_toggle", CImGui_Toggle_f);
Cmd_AddCommand("cimgui_reset", CImGui_Reset_f);
Cmd_AddCommand("cimgui_save", CImGui_Save_f);</code></pre>
    </div>
    
    <h3>Plugin Architecture</h3>
    <div class="code-block">
        <pre><code>// Plugin system for extending CimGui functionality
typedef struct cimgui_plugin_s {
    char name[64];
    void (*init)(void);
    void (*shutdown)(void);
    void (*draw)(void);
    qboolean (*handle_event)(SDL_Event* event);
    struct cimgui_plugin_s* next;
} cimgui_plugin_t;

static cimgui_plugin_t* plugin_list = NULL;

void CImGui_RegisterPlugin(cimgui_plugin_t* plugin) {
    plugin->next = plugin_list;
    plugin_list = plugin;
    
    if (plugin->init) {
        plugin->init();
    }
    
    Com_Printf("CimGui: Registered plugin '%s'\n", plugin->name);
}

void CImGui_DrawPlugins(void) {
    cimgui_plugin_t* plugin = plugin_list;
    while (plugin) {
        if (plugin->draw) {
            plugin->draw();
        }
        plugin = plugin->next;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Complete Build Reference</h2>
    
    <h3>File Checklist</h3>
    <div class="code-block">
        <pre><code># Required new files:
src/cimgui_integration.h    # Main header file
src/cimgui_integration.c    # Main implementation  
src/cimgui_config.h         # Configuration header
src/cimgui_config.c         # Configuration implementation

# Modified files:
CMakeLists.txt              # Build system integration
src/client/cl_main.c        # Initialization/shutdown
src/client/cl_input.c       # Event processing
src/common/common.c        # Console capture
src/renderercommon/tr_*.c   # Render integration

# Build scripts:
build-cimgui.sh            # Linux/macOS build automation
build-cimgui.bat           # Windows build automation

# External dependency:
external/cimgui/           # CimGui submodule

# Generated files:
baseq3/cimgui.cfg          # User configuration (created at runtime)</code></pre>
    </div>
    
    <h3>Common Build Issues and Solutions</h3>
    <div class="troubleshooting">
        <h4>Compilation Errors</h4>
        <ul>
            <li><strong>cimgui.h not found:</strong> Run `git submodule update --init --recursive`</li>
            <li><strong>Undefined references:</strong> Check CMakeLists.txt linkage, ensure cimgui target is built</li>
            <li><strong>SDL2 backend errors:</strong> Verify SDL2 development headers are installed</li>
            <li><strong>OpenGL/Vulkan errors:</strong> Check renderer-specific preprocessor flags</li>
        </ul>
        
        <h4>Runtime Errors</h4>
        <ul>
            <li><strong>Initialization failed:</strong> Check OpenGL context creation order</li>
            <li><strong>UI not responding:</strong> Verify event processing integration</li>
            <li><strong>Performance issues:</strong> Profile with release build, check overdraw</li>
            <li><strong>Memory leaks:</strong> Ensure proper shutdown sequence</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Extension Examples</h2>
    
    <h3>Network Statistics Window</h3>
    <div class="code-block">
        <pre><code>void CImGui_ShowNetworkStatsWindow(void) {
    if (!igBegin("Network Statistics", &cimgui_state.show_network, 0)) {
        igEnd();
        return;
    }
    
    // Connection info
    if (cls.state >= CA_CONNECTED) {
        igText("Server: %s", cls.servername);
        igText("Ping: %d ms", cls.ping);
        igText("Packet Loss: %.1f%%", cls.packetloss * 100.0f);
        
        igSeparator();
        
        // Bandwidth usage
        igText("Incoming: %.1f KB/s", cls.incoming_bytes / 1024.0f);
        igText("Outgoing: %.1f KB/s", cls.outgoing_bytes / 1024.0f);
        
        igSeparator();
        
        // Connection quality
        float quality = (100.0f - cls.packetloss * 100.0f) * (cls.ping < 50 ? 1.0f : 50.0f / cls.ping);
        ImVec4 color = quality > 75 ? (ImVec4){0,1,0,1} : quality > 50 ? (ImVec4){1,1,0,1} : (ImVec4){1,0,0,1};
        igTextColored(color, "Connection Quality: %.0f%%", quality);
    } else {
        igText("Not connected to server");
    }
    
    igEnd();
}</code></pre>
    </div>
    
    <h3>Demo Playback Controls</h3>
    <div class="code-block">
        <pre><code>void CImGui_ShowDemoControlsWindow(void) {
    if (!igBegin("Demo Controls", &cimgui_state.show_demo_controls, 0)) {
        igEnd();
        return;
    }
    
    if (cls.demoplayback) {
        igText("Demo: %s", cls.demoname);
        igText("Time: %.1f / %.1f seconds", cls.demolength_current, cls.demolength_total);
        
        // Progress bar
        float progress = cls.demolength_total > 0 ? cls.demolength_current / cls.demolength_total : 0.0f;
        igProgressBar(progress, (ImVec2){-1, 0}, NULL);
        
        // Playback controls
        if (igButton("Pause", (ImVec2){60, 0})) {
            Cbuf_AddText("demo_pause\n");
        }
        igSameLine(0, 5);
        
        if (igButton("Step", (ImVec2){60, 0})) {
            Cbuf_AddText("demo_step\n");
        }
        igSameLine(0, 5);
        
        if (igButton("Restart", (ImVec2){60, 0})) {
            Cbuf_AddText("demo_restart\n");
        }
        
        // Speed control
        static float speed = 1.0f;
        if (igSliderFloat("Speed", &speed, 0.1f, 4.0f, "%.1fx", 0)) {
            Cbuf_AddText(va("timescale %.1f\n", speed));
        }
        
    } else {
        igText("No demo playing");
        
        if (igButton("Load Demo", (ImVec2){0, 0})) {
            // Could implement file browser here
            igOpenPopup("Load Demo File");
        }
    }
    
    igEnd();
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Next Steps and Future Enhancements</h2>
    
    <h3>Immediate Improvements</h3>
    <ul>
        <li><strong>Full Vulkan Backend:</strong> Complete Vulkan renderer integration with proper command buffer handling</li>
        <li><strong>Texture Viewer:</strong> Display loaded textures with preview thumbnails</li>
        <li><strong>Model Inspector:</strong> View model geometry, animations, and LOD levels</li>
        <li><strong>Shader Debugger:</strong> Live shader editing with hot-reload capabilities</li>
    </ul>
    
    <h3>Advanced Features</h3>
    <ul>
        <li><strong>Scripting Integration:</strong> Lua or JavaScript bindings for custom tools</li>
        <li><strong>Remote Debugging:</strong> TCP/IP interface for external editor integration</li>
        <li><strong>Performance Profiler:</strong> Detailed CPU/GPU timing with flamegraph visualization</li>
        <li><strong>Asset Pipeline:</strong> Content browser with import/export functionality</li>
    </ul>
    
    <h3>Community Contributions</h3>
    <ul>
        <li><strong>Plugin System:</strong> Allow community-developed debug tools</li>
        <li><strong>Theme Support:</strong> Custom UI themes and layouts</li>
        <li><strong>Documentation:</strong> In-game help system and tutorials</li>
        <li><strong>Mod Support:</strong> CimGui integration for game modifications</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="external/imgui-integration">ImGui Integration (C++)</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
        <li><a href="modernization/build-systems">Modern Build Systems</a></li>
        <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
    </ul>
</div>