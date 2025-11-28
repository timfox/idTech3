<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>FFXV Raymarching Sky Implementation Guide</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            line-height: 1.6;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
        }
        pre {
            background-color: #f4f4f4;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
        }
        code {
            font-family: 'Courier New', Courier, monospace;
        }
        .note {
            background-color: #fff3cd;
            padding: 10px;
            border-left: 4px solid #ffc107;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <h1>Implementing FFXV-Style Raymarching Sky in Quake3e Vulkan</h1>

    <h2>Overview</h2>
    <p>This guide explains how to implement the Final Fantasy XV raymarching sky technique in your Quake3e Vulkan fork. The implementation replaces the traditional skybox with a physically-based atmosphere simulation.</p>

    <h2>Key Features</h2>
    <ul>
        <li>Real-time raymarching in fragment shader</li>
        <li>Dynamic sun position and time-of-day control</li>
        <li>Physical modeling using Rayleigh and Mie scattering</li>
        <li>Temporal reprojection for stability</li>
        <li>Low-res screen-space pass with upsampling</li>
    </ul>

    <h2>Implementation Steps</h2>

    <h3>1. Replace Skybox with Fullscreen Quad</h3>
    <p>In your renderervk, replace the static cubemap skybox with a fullscreen quad that runs the raymarching atmosphere fragment shader.</p>

    <h3>2. Create Vulkan Sky Shader</h3>
    <pre><code>#version 450

layout(location = 0) in vec2 fragCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 invProj;
    mat4 invView;
    vec3 cameraPos;
    vec3 sunDir;
    float timeOfDay;
} ubo;

// Atmosphere constants
const float HR = 8.0;  // Rayleigh scale height
const float HM = 1.2;  // Mie scale height
const float EARTH_RADIUS = 6360.0;
const float ATMOSPHERE_RADIUS = 6460.0;</code></pre>

    <h3>3. Update Vulkan Renderer</h3>
    <p>Add descriptor sets for camera uniforms and create a render pass for the fullscreen quad.</p>

    <h3>4. Time-of-Day and Sun Direction</h3>
    <p>Implement sun direction tracking and update the UBO accordingly.</p>

    <h3>5. Optimization</h3>
    <p>Use low-res rendering with upscaling and consider temporal reprojection.</p>

    <div class="note">
        <strong>Note:</strong> This implementation requires careful consideration of performance and visual quality trade-offs.
    </div>

    <h2>Integration Summary</h2>
    <table>
        <tr>
            <th>Component</th>
            <th>Task</th>
        </tr>
        <tr>
            <td>Renderer</td>
            <td>Replace skybox with fullscreen raymarch shader</td>
        </tr>
        <tr>
            <td>Shader</td>
            <td>Implement scattering from FF15 paper</td>
        </tr>
        <tr>
            <td>Data</td>
            <td>Pass camera and sun data to shader</td>
        </tr>
    </table>

    <h2>Bonus: Atmospheric GI Integration</h2>
    <p>Consider integrating the sky color into your octree voxel GI system for realistic ambient lighting changes.</p>

    <h2>Toggleable Raymarching Sky</h2>
    <p>This section explains how to implement a toggleable raymarching sky feature in your Quake3e fork.</p>

    <h3>1. Add the Cvar (Engine-side)</h3>
    <p>In <code>code/client/cl_cgame.c</code> or <code>code/renderer/tr_init.c</code>, define:</p>
    <pre><code>cvar_t* r_raymarchSky;

void R_RegisterCvars(void) {
    r_raymarchSky = ri.Cvar_Get("r_raymarchSky", "1", CVAR_ARCHIVE);
}</code></pre>
    <p>"1" sets raymarching sky on by default. Users can toggle via console:</p>
    <pre><code>/r_raymarchSky 0
/vid_restart</code></pre>

    <h3>2. Hook into Renderer Logic</h3>
    <p>In your Vulkan renderer's sky-drawing function (renderervk/tr_world.c or similar):</p>
    <pre><code>if (r_raymarchSky->integer) {
    R_DrawRaymarchSky();
} else {
    R_DrawDefaultSkybox();
}</code></pre>
    <p>Define clearly separated functions for clarity and maintainability.</p>

    <h3>3. Per-Map Control via .worldspawn Entity</h3>
    <p>Edit your BSP parsing in <code>code/game/g_spawn.c</code>:</p>
    <pre><code>// In G_SpawnEntitiesFromString():
char* value = G_SpawnString("raymarchSky", "1"); // defaults to on
trap_Cvar_Set("r_raymarchSky", value);</code></pre>
    <p>Now, in your map editor (Radiant), add to the worldspawn entity properties:</p>
    <pre><code>"raymarchSky" "0"</code></pre>
    <p>This disables raymarching for that map, overriding the global cvar.</p>

    <h3>4. Optional Lua Override</h3>
    <p>In your Lua API (g_lua.c):</p>
    <pre><code>static int lua_SetRaymarchSky(lua_State* L) {
    int enabled = lua_toboolean(L, 1);
    trap_Cvar_Set("r_raymarchSky", enabled ? "1" : "0");
    return 0;
}

lua_register(L, "set_raymarch_sky", lua_SetRaymarchSky);</code></pre>
    <p>Use in Lua scripts:</p>
    <pre><code>set_raymarch_sky(true) -- enables sky
set_raymarch_sky(false) -- disables sky</code></pre>

    <h3>5. Example Usage Scenarios</h3>
    <ul>
        <li>Default (All maps): <code>/r_raymarchSky 1</code> (via config or console)</li>
        <li>Specific Map Disable: In Radiant: <code>"raymarchSky" "0"</code> in worldspawn</li>
        <li>Lua-controlled cinematic scene: <code>set_raymarch_sky(false)</code> during cinematics, then restore later.</li>
    </ul>

    <h3>Best Practices and AAA Tips</h3>
    <ul>
        <li>Document clearly for level designers: worldspawn properties, console commands, Lua APIs</li>
        <li>Provide debug UI (ImGui, Console) to toggle visually.</li>
        <li>Implement smooth fade/blend between raymarch and skybox if toggled at runtime.</li>
    </ul>

    <h3>Integration Summary Table</h3>
    <table>
        <tr>
            <th>Component</th>
            <th>Integration Point</th>
        </tr>
        <tr>
            <td>Cvar (r_raymarchSky)</td>
            <td>tr_init.c, initialized in R_RegisterCvars()</td>
        </tr>
        <tr>
            <td>BSP (map) override</td>
            <td>g_spawn.c parsing worldspawn entity</td>
        </tr>
        <tr>
            <td>Renderer logic</td>
            <td>Conditional check in sky rendering code</td>
        </tr>
        <tr>
            <td>Lua API (optional)</td>
            <td>Exposed for scripting runtime control</td>
        </tr>
    </table>

    <p>Now your sky is fully configurable and AAA-grade flexible.</p>
</body>
</html>

