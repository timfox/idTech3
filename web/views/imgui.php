<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Dear ImGui Implementation in id Tech 3</title>
    <style>
        @font-face {
            font-family: 'FX300';
            src: url('fonts/FX300 Angular.ttf') format('truetype');
        }
        body {
            font-family: 'Helvetica', monospace;
            line-height: 1.6;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background-color: #000;
            color: #0f0;
        }
        code {
            background-color: #111;
            padding: 2px 5px;
            border-radius: 3px;
            color: #0ff;
        }
        pre {
            background-color: #111;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
            color: #0ff;
            border: 1px solid #0f0;
        }
        h1, h2, h3 {
            font-family: 'FX300', monospace;
            color: #f0f;
            text-shadow: 2px 2px #0f0;
        }
        a {
            color: #0ff;
        }
        a:hover {
            color: #f0f;
        }
        .note {
            background-color: #111;
            border-left: 4px solid #f0f;
            padding: 10px;
            margin: 10px 0;
        }
        .warning {
            background-color: #111;
            border-left: 4px solid #f00;
            padding: 10px;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <h1>Dear ImGui Implementation in id Tech 3</h1>
    
    <div class="note">
        <strong>Note:</strong> This implementation requires Vulkan support and integrates with existing HDR, ACES tonemapping, and LUT systems.
    </div>

    <h2>Overview</h2>
    <p>This guide explains how to implement Dear ImGui in id Tech 3 using Vulkan, providing an in-game debug interface for controlling various engine features including HDR, tonemapping, LUTs, music, fonts, and PBR materials.</p>

    <h2>Prerequisites</h2>
    <ul>
        <li>id Tech 3 Source Code with Vulkan support</li>
        <li>Dear ImGui source code</li>
        <li>Vulkan SDK</li>
        <li>Development Environment (C/C++, CMake)</li>
        <li>Basic understanding of Vulkan pipeline setup</li>
    </ul>

    <div class="warning">
        <strong>Warning:</strong> Make sure to backup your source code before implementing ImGui.
    </div>

    <h2>Implementation Steps</h2>
    <ol>
        <li><strong>Add ImGui Dependencies:</strong> Add to CMakeLists.txt:
            <pre>
# Add ImGui
add_subdirectory(external/imgui)
target_link_libraries(quake3e PRIVATE imgui)</pre>
        </li>
        <li><strong>Create ImGui Integration Files:</strong> Create imgui_integration.h:
            <pre>
#pragma once

#include "tr_local.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"

// ImGui state
extern bool g_imguiEnabled;
extern bool g_imguiShowDemo;
extern bool g_imguiShowConsole;
extern bool g_imguiShowGraphics;
extern bool g_imguiShowAudio;
extern bool g_imguiShowFonts;
extern bool g_imguiShowPBR;

// ImGui functions
void ImGui_Init(void);
void ImGui_Shutdown(void);
void ImGui_NewFrame(void);
void ImGui_Render(void);
void ImGui_UpdateInput(void);</pre>
        </li>
        <li><strong>Implement ImGui Integration:</strong> Create imgui_integration.c:
            <pre>
#include "imgui_integration.h"

// ImGui state
bool g_imguiEnabled = false;
bool g_imguiShowDemo = false;
bool g_imguiShowConsole = false;
bool g_imguiShowGraphics = false;
bool g_imguiShowAudio = false;
bool g_imguiShowFonts = false;
bool g_imguiShowPBR = false;

void ImGui_Init(void) {
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup Vulkan backend
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vk.instance;
    init_info.PhysicalDevice = vk.physical_device;
    init_info.Device = vk.device;
    init_info.QueueFamily = vk.queue_family;
    init_info.Queue = vk.queue;
    init_info.PipelineCache = vk.pipeline_cache;
    init_info.DescriptorPool = vk.descriptor_pool;
    init_info.Allocator = NULL;
    init_info.MinImageCount = vk.swapchain_image_count;
    init_info.ImageCount = vk.swapchain_image_count;
    init_info.CheckVkResultFn = NULL;
    
    ImGui_ImplVulkan_Init(&init_info, vk.render_pass);
    
    // Load fonts
    ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/cousine-regular.ttf", 16.0f);
    io.Fonts->Build();
    
    // Upload font textures
    VkCommandBuffer cmd_buffer = vk_begin_single_time_commands();
    ImGui_ImplVulkan_CreateFontsTexture(cmd_buffer);
    vk_end_single_time_commands(cmd_buffer);
}

void ImGui_Shutdown(void) {
    ImGui_ImplVulkan_Shutdown();
    ImGui::DestroyContext();
}

void ImGui_NewFrame(void) {
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
}

void ImGui_Render(void) {
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(draw_data, vk.command_buffer);
}

void ImGui_UpdateInput(void) {
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(mouse_x, mouse_y);
    io.MouseDown[0] = mouse_buttons & 1;
    io.MouseDown[1] = mouse_buttons & 2;
    io.MouseDown[2] = mouse_buttons & 4;
    io.MouseWheel = mouse_wheel;
    
    // Update keyboard state
    for (int i = 0; i < 512; i++) {
        io.KeysDown[i] = keys[i];
    }
    io.KeyCtrl = keys[K_CTRL];
    io.KeyShift = keys[K_SHIFT];
    io.KeyAlt = keys[K_ALT];
    io.KeySuper = keys[K_SUPER];
}</pre>
        </li>
        <li><strong>Add ImGui Windows:</strong> Add to imgui_integration.c:
            <pre>
void ImGui_ShowMainMenu(void) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Demo Window", NULL, &g_imguiShowDemo);
            ImGui::MenuItem("Console", NULL, &g_imguiShowConsole);
            ImGui::MenuItem("Graphics", NULL, &g_imguiShowGraphics);
            ImGui::MenuItem("Audio", NULL, &g_imguiShowAudio);
            ImGui::MenuItem("Fonts", NULL, &g_imguiShowFonts);
            ImGui::MenuItem("PBR", NULL, &g_imguiShowPBR);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    
    if (g_imguiShowDemo) {
        ImGui::ShowDemoWindow(&g_imguiShowDemo);
    }
    
    if (g_imguiShowConsole) {
        ImGui_ShowConsoleWindow();
    }
    
    if (g_imguiShowGraphics) {
        ImGui_ShowGraphicsWindow();
    }
    
    if (g_imguiShowAudio) {
        ImGui_ShowAudioWindow();
    }
    
    if (g_imguiShowFonts) {
        ImGui_ShowFontsWindow();
    }
    
    if (g_imguiShowPBR) {
        ImGui_ShowPBRWindow();
    }
}

void ImGui_ShowConsoleWindow(void) {
    if (ImGui::Begin("Console", &g_imguiShowConsole)) {
        static char input[256] = "";
        if (ImGui::InputText("Command", input, sizeof(input), 
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            Cbuf_AddText(input);
            input[0] = '\0';
        }
        
        // Display console history
        ImGui::BeginChild("ConsoleHistory", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        for (int i = 0; i < consoleHistory.size(); i++) {
            ImGui::TextWrapped("%s", consoleHistory[i].c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
        
        // Add clear button
        if (ImGui::Button("Clear")) {
            consoleHistory.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            std::string history;
            for (const auto& line : consoleHistory) {
                history += line + "\n";
            }
            ImGui::SetClipboardText(history.c_str());
        }
        ImGui::End();
    }
}

void ImGui_ShowGraphicsWindow(void) {
    if (ImGui::Begin("Graphics", &g_imguiShowGraphics)) {
        // HDR and Tonemapping Settings
        ImGui::Checkbox("HDR", &r_hdr->integer);
        ImGui::Checkbox("ACES Tonemapping", &r_acesTonemapping->integer);
        
        if (r_hdr->integer) {
            ImGui::SliderFloat("Exposure", &r_exposure->value, 0.1f, 4.0f, "%.2f");
        }

        // LUT Settings
        ImGui::Separator();
        ImGui::Text("Look-Up Table Settings");
        ImGui::Checkbox("Enable LUT", &r_lutEnable->integer);
        
        if (r_lutEnable->integer) {
            ImGui::SliderFloat("LUT Intensity", &r_lutIntensity->value, 0.0f, 1.0f, "%.2f");
            ImGui::Combo("Blend Mode", &r_lutBlendMode->integer, 
                "Normal\0Multiply\0Screen\0Overlay\0");
            ImGui::SliderFloat("Animation Speed", &r_lutAnimationSpeed->value, 0.0f, 2.0f, "%.2f");
            ImGui::Combo("Quality", &r_lutQuality->integer, "Low\0Medium\0High\0");
        }

        // Post-Processing Settings
        ImGui::Separator();
        ImGui::Text("Post-Processing");
        ImGui::Checkbox("Enable Post-Processing", &r_postProcess->integer);
        
        if (r_postProcess->integer) {
            ImGui::SliderFloat("Bloom Intensity", &r_bloomIntensity->value, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Vignette Intensity", &r_vignetteIntensity->value, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Chromatic Aberration", &r_chromaticAberration->value, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Film Grain", &r_filmGrain->value, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Motion Blur", &r_motionBlur->value, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Depth of Field", &r_dof->value, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Sharpness", &r_sharpness->value, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Contrast", &r_contrast->value, 0.5f, 1.5f, "%.2f");
            ImGui::SliderFloat("Saturation", &r_saturation->value, 0.0f, 2.0f, "%.2f");
        }
    }
    ImGui::End();
}

void ImGui_ShowPerformanceWindow(void) {
    if (ImGui::Begin("Performance", &g_imguiShowPerformance)) {
        // Quality Preset
        ImGui::Text("Quality Preset");
        ImGui::Combo("##QualityPreset", &r_qualityPreset->integer, 
            "Low\0Medium\0High\0Ultra\0");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Automatically adjusts graphics settings for optimal performance");
        }

        // Frame Rate Settings
        ImGui::Separator();
        ImGui::Text("Frame Rate Settings");
        
        ImGui::Checkbox("VSync", &r_vsync->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Synchronizes frame rate with monitor refresh rate");
        }
        
        ImGui::SliderInt("FPS Limit", &r_fpsLimit->integer, 30, 300, "%d FPS");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Maximum frames per second (0 = unlimited)");
        }
        
        ImGui::Checkbox("Triple Buffering", &r_tripleBuffering->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reduces screen tearing at the cost of higher latency");
        }

        // Quality Settings
        ImGui::Separator();
        ImGui::Text("Quality Settings");
        
        ImGui::SliderInt("Texture Quality", &r_textureQuality->integer, 0, 3, "%d");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0 = Low, 1 = Medium, 2 = High, 3 = Ultra");
        }
        
        ImGui::SliderInt("Shadow Quality", &r_shadowQuality->integer, 0, 3, "%d");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0 = Low, 1 = Medium, 2 = High, 3 = Ultra");
        }
        
        ImGui::SliderInt("Anti-Aliasing", &r_antialiasing->integer, 0, 4, "%d");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0 = Off, 1 = FXAA, 2 = SMAA, 3 = TAA, 4 = MSAA 4x");
        }

        // Texture Filtering
        ImGui::Separator();
        ImGui::Text("Texture Filtering");
        
        ImGui::Checkbox("Anisotropic Filtering", &r_anisotropic->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Improves texture quality at oblique angles");
        }
        
        if (r_anisotropic->integer) {
            ImGui::SliderInt("AF Level", &r_anisotropicLevel->integer, 1, 16, "%d");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Higher values provide better quality but use more memory");
            }
        }
    }
    ImGui::End();
}

void ImGui_ShowPostProcessWindow(void) {
    if (ImGui::Begin("Post-Processing", &g_imguiShowPostProcess)) {
        // Bloom Settings
        ImGui::Text("Bloom");
        ImGui::Checkbox("Enable Bloom", &r_bloom->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adds a bloom effect to bright areas of the scene");
        }
        
        if (r_bloom->integer) {
            ImGui::SliderFloat("Bloom Intensity", &r_bloomIntensity->value, 0.0f, 2.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls the strength of the bloom effect");
            }
            
            ImGui::SliderFloat("Bloom Threshold", &r_bloomThreshold->value, 0.0f, 3.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Minimum brightness required for bloom to occur");
            }
            
            ImGui::SliderInt("Bloom Quality", &r_bloomQuality->integer, 0, 2, "%d");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("0 = Low, 1 = Medium, 2 = High");
            }
            
            ImGui::ColorEdit3("Bloom Tint", r_bloomTint->value);
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Adjusts the color tint of the bloom effect");
            }
        }

        // HDR Settings
        ImGui::Separator();
        ImGui::Text("HDR Settings");
        ImGui::Checkbox("Enable HDR", &r_hdr->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enables High Dynamic Range rendering");
        }

        if (r_hdr->integer) {
            ImGui::SliderFloat("Exposure", &r_exposure->value, 0.1f, 4.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls the overall scene brightness");
            }

            ImGui::Checkbox("ACES Tonemapping", &r_acesTonemapping->integer);
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Applies ACES filmic tonemapping to HDR content");
            }
        }

        // LUT Settings
        ImGui::Separator();
        ImGui::Text("Color Grading");
        ImGui::Checkbox("Enable LUT", &r_lutEnable->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Applies color grading using a Look-Up Table");
        }

        if (r_lutEnable->integer) {
            ImGui::SliderFloat("LUT Intensity", &r_lutIntensity->value, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls the strength of the color grading effect");
            }

            ImGui::Combo("Blend Mode", &r_lutBlendMode->integer, "Normal\0Multiply\0Screen\0Overlay\0");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Selects how the LUT is blended with the original image");
            }

            ImGui::SliderFloat("Animation Speed", &r_lutAnimationSpeed->value, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls the speed of animated LUT effects");
            }
        }
    }
    ImGui::End();
}

void ImGui_ShowMainMenuBar(void) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Settings")) {
                // Save settings implementation
            }
            if (ImGui::MenuItem("Load Settings")) {
                // Load settings implementation
    // Main loop
    while (!done) {
        // Process input
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                done = true;
            }
        }

        // Start ImGui frame
        ImGui_NewFrame();

        // Render game
        R_RenderFrame();

        // Render ImGui
        ImGui_Render();

        // Present frame
        vkQueuePresentKHR(vk.queue, &presentInfo);
    }

    // Cleanup
    ImGui_Shutdown();
    vkDestroySwapchainKHR(vk.device, vk.swapchain, nullptr);
    vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
    vkDestroyDevice(vk.device, nullptr);
    vkDestroyInstance(vk.instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

// Helper function to create ImGui window for HDR/Tonemapping settings
void ImGui_ShowHDRSettings(void) {
    if (ImGui::Begin("HDR & Tonemapping Settings")) {
        ImGui::Checkbox("Enable HDR", (bool*)&r_hdr->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enables High Dynamic Range rendering");
        }

        if (r_hdr->integer) {
            ImGui::SliderFloat("Exposure", &r_exposure->value, 0.1f, 4.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls the overall brightness of HDR scenes");
            }

            ImGui::Checkbox("ACES Tonemapping", (bool*)&r_acesTonemapping->integer);
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Applies ACES filmic tonemapping to HDR content");
            }
        }
    }
    ImGui::End();
}

// Helper function to create ImGui window for LUT settings
void ImGui_ShowLUTSettings(void) {
    if (ImGui::Begin("LUT Settings")) {
        ImGui::Checkbox("Enable LUTs", (bool*)&r_lutEnable->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enables Look-Up Table color grading");
        }

        if (r_lutEnable->integer) {
            ImGui::SliderFloat("LUT Intensity", &r_lutIntensity->value, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls the strength of the color grading effect");
            }

            ImGui::Combo("Blend Mode", &r_lutBlendMode->integer, "Normal\0Multiply\0Screen\0Overlay\0");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Selects how the LUT is blended with the original image");
            }

            ImGui::SliderFloat("Animation Speed", &r_lutAnimationSpeed->value, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Controls the speed of animated LUT effects");
            }

            ImGui::Checkbox("LUT Streaming", (bool*)&r_lutStreaming->integer);
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enables streaming of large LUT files");
            }

            ImGui::SliderInt("LUT Quality", &r_lutQuality->integer, 1, 3, "%d");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Adjusts the quality of LUT processing (higher = better quality)");
            }
        }
    }
    ImGui::End();
}

// Main ImGui rendering function
void ImGui_Render(void) {
    // Start new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Show main menu bar with file, edit, view options
    ImGui_ShowMainMenuBar();

    // Show HDR settings window
    ImGui_ShowHDRSettings();

    // Show LUT settings window
    ImGui_ShowLUTSettings();

    // Show performance metrics window
    ImGui_ShowPerformanceMetrics();

    // Show debug console window
    ImGui_ShowDebugConsole();

    // Show about window if requested
    if (show_about_window) {
        ImGui_ShowAboutWindow();
    }

    // Render ImGui
    ImGui::Render();
    
    // Record ImGui draw commands
    VkCommandBuffer cmd = vk.commandBuffers[vk.currentFrame];
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);
    
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    
    vkEndCommandBuffer(cmd);
}

// Helper function to show performance metrics
void ImGui_ShowPerformanceMetrics(void) {
    if (ImGui::Begin("Performance Metrics")) {
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("Draw Calls: %d", tr.drawCalls);
        ImGui::Text("Triangle Count: %d", tr.triangleCount);
        
        if (ImGui::CollapsingHeader("GPU Memory")) {
            ImGui::Text("Texture Memory: %.2f MB", tr.textureMemory / (1024.0f * 1024.0f));
            ImGui::Text("Buffer Memory: %.2f MB", tr.bufferMemory / (1024.0f * 1024.0f));
            ImGui::Text("LUT Memory: %.2f MB", tr.lutMemory / (1024.0f * 1024.0f));
        }

        if (ImGui::CollapsingHeader("Vulkan Info")) {
            ImGui::Text("Vulkan Version: %s", tr.vulkanVersion);
            ImGui::Text("Device: %s", tr.deviceName);
            ImGui::Text("Driver Version: %s", tr.driverVersion);
        }
    }
    ImGui::End();
}

// Helper function to show debug console
void ImGui_ShowDebugConsole(void) {
    if (ImGui::Begin("Debug Console")) {
        static char input[256] = "";
        static std::vector<std::string> history;
        
        // Command input
        if (ImGui::InputText("Command", input, IM_ARRAYSIZE(input), 
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (input[0] != '\0') {
                history.push_back(input);
                Cbuf_AddText(input);
                input[0] = '\0';
            }
        }

        // Command history
        ImGui::BeginChild("History", ImVec2(0, 200), true);
        for (const auto& cmd : history) {
            ImGui::TextWrapped("%s", cmd.c_str());
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

// Helper function to show about window
void ImGui_ShowAboutWindow(void) {
    if (ImGui::Begin("About", &show_about_window)) {
        ImGui::Text("id Tech 3 Enhanced");
        ImGui::Text("Version 1.0.0");
        ImGui::Separator();
        ImGui::Text("Features:");
        ImGui::BulletText("Vulkan Rendering");
        ImGui::BulletText("PBR Materials");
        ImGui::BulletText("HDR Rendering");
        ImGui::BulletText("ACES Tonemapping");
        ImGui::BulletText("LUT Color Grading");
        ImGui::BulletText("ImGui Debug Interface");
        ImGui::Separator();
        ImGui::Text("Built with:");
        ImGui::BulletText("Dear ImGui");
        ImGui::BulletText("Vulkan");
        ImGui::BulletText("SDL2");
    }
    ImGui::End();
}

// Helper function to show HDR settings
void ImGui_ShowHDRSettings(void) {
    if (ImGui::Begin("HDR Settings")) {
        ImGui::Checkbox("Enable HDR", (bool*)&r_hdr->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enables High Dynamic Range rendering");
        }

        if (r_hdr->integer) {
            ImGui::SliderFloat("Exposure", &r_exposure->value, 0.1f, 4.0f, "%.2f");
            ImGui::Checkbox("ACES Tonemapping", (bool*)&r_acesTonemapping->integer);
            ImGui::SliderFloat("Gamma", &r_gamma->value, 1.0f, 3.0f, "%.2f");
        }
    }
    ImGui::End();
}

// Helper function to show LUT settings
void ImGui_ShowLUTSettings(void) {
    if (ImGui::Begin("LUT Settings")) {
        ImGui::Checkbox("Enable LUTs", (bool*)&r_lutEnable->integer);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enables Look-Up Table color grading");
        }

        if (r_lutEnable->integer) {
            ImGui::SliderFloat("LUT Intensity", &r_lutIntensity->value, 0.0f, 1.0f, "%.2f");
            ImGui::Combo("Blend Mode", &r_lutBlendMode->integer, "Normal\0Multiply\0Screen\0Overlay\0");
            ImGui::SliderFloat("Animation Speed", &r_lutAnimationSpeed->value, 0.0f, 1.0f, "%.2f");
            ImGui::Checkbox("LUT Streaming", (bool*)&r_lutStreaming->integer);
            ImGui::SliderInt("LUT Quality", &r_lutQuality->integer, 1, 3, "%d");
        }
    }
    ImGui::End();
}

// Helper function to show main menu bar
void ImGui_ShowMainMenuBar(void) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                Cbuf_AddText("quit\n");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("HDR Settings", NULL, &show_hdr_window);
            ImGui::MenuItem("LUT Settings", NULL, &show_lut_window);
            ImGui::MenuItem("Performance Metrics", NULL, &show_performance_window);
            ImGui::MenuItem("Debug Console", NULL, &show_console_window);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                show_about_window = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}
</pre>

<h2>Testing</h2>
<ul>
    <li>Test ImGui integration with: <code>/r_showImgui 1</code></li>
    <li>Verify all windows render correctly</li>
    <li>Test window dragging and resizing</li>
    <li>Verify HDR and LUT settings work through ImGui</li>
    <li>Test performance metrics accuracy</li>
    <li>Verify debug console command execution</li>
</ul>

<h2>Performance Considerations</h2>
<ul>
    <li>Monitor ImGui rendering overhead</li>
    <li>Profile window update frequency</li>
    <li>Consider adding window visibility toggles</li>
    <li>Test with different display resolutions</li>
</ul>

<h2>Additional Resources</h2>
<ul>
    <li><a href="https://github.com/ocornut/imgui">Dear ImGui Repository</a></li>
    <li><a href="https://github.com/ocornut/imgui/wiki">ImGui Wiki</a></li>
    <li><a href="https://github.com/ocornut/imgui/wiki/FAQ">ImGui FAQ</a></li>
    <li><a href="https://github.com/ocornut/imgui/wiki/Getting-Started">ImGui Getting Started</a></li>
</ul>
</body>
</html>