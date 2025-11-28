<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Look-Up Tables (LUTs) Implementation in id Tech 3</title>
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
    <h1>Look-Up Tables (LUTs) Implementation in id Tech 3</h1>
    
    <div class="note">
        <strong>Note:</strong> This implementation requires Vulkan support and works alongside the existing HDR and ACES tonemapping features.
    </div>

    <h2>Overview</h2>
    <p>This guide explains how to implement Look-Up Tables (LUTs) in id Tech 3 using Vulkan, providing advanced color grading capabilities that work in conjunction with HDR and ACES tonemapping.</p>

    <h2>Modern LUT Features</h2>
    <ul>
        <li>Real-time LUT switching and blending</li>
        <li>Per-scene LUT assignments</li>
        <li>LUT layering and blending modes</li>
        <li>LUT animation support</li>
        <li>LUT preview in editor</li>
        <li>LUT baking for performance optimization</li>
        <li>LUT streaming for large files</li>
    </ul>

    <h2>Prerequisites</h2>
    <ul>
        <li>id Tech 3 Source Code with HDR and ACES support</li>
        <li>Vulkan SDK</li>
        <li>Basic understanding of color grading</li>
        <li>Familiarity with 3D textures in Vulkan</li>
        <li>Development Environment (C/C++, CMake)</li>
    </ul>

    <div class="warning">
        <strong>Warning:</strong> Make sure to backup your source code before implementing LUTs.
    </div>

    <h2>Implementation Steps</h2>
    <ol>
        <li><strong>Add CVars:</strong> Add to tr_local.h:
            <pre>
cvar_t *r_lutEnable;
cvar_t *r_lutIntensity;
cvar_t *r_lutBlendMode;    // 0: Normal, 1: Multiply, 2: Screen, 3: Overlay
cvar_t *r_lutAnimationSpeed;
cvar_t *r_lutStreaming;    // Enable/disable LUT streaming
cvar_t *r_lutQuality;      // Quality preset for LUT processing</pre>
        </li>
        <li><strong>Initialize CVars:</strong> Add to tr_init.c:
            <pre>
r_lutEnable = ri.Cvar_Get("r_lutEnable", "0", CVAR_ARCHIVE);
r_lutIntensity = ri.Cvar_Get("r_lutIntensity", "1.0", CVAR_ARCHIVE);
r_lutBlendMode = ri.Cvar_Get("r_lutBlendMode", "0", CVAR_ARCHIVE);
r_lutAnimationSpeed = ri.Cvar_Get("r_lutAnimationSpeed", "1.0", CVAR_ARCHIVE);
r_lutStreaming = ri.Cvar_Get("r_lutStreaming", "1", CVAR_ARCHIVE);
r_lutQuality = ri.Cvar_Get("r_lutQuality", "2", CVAR_ARCHIVE);</pre>
        </li>
        <li><strong>Add LUT Texture Loading:</strong> Add to tr_image.c:
            <pre>
void R_LoadLUT(const char *name) {
    image_t *image;
    byte *data;
    int width, height, depth;
    
    // Load 3D LUT texture with streaming support
    if (r_lutStreaming->integer) {
        data = R_LoadImageStreamed(name, &width, &height, &depth);
    } else {
        data = R_LoadImage(name, &width, &height, &depth);
    }
    
    if (!data) {
        ri.Error(ERR_DROP, "Failed to load LUT: %s", name);
        return;
    }
    
    // Create 3D texture with quality settings
    int flags = IF_NOMIPMAP | IF_NOPICMIP | IF_LUT;
    if (r_lutQuality->integer > 1) {
        flags |= IF_HIGHQUALITY;
    }
    
    image = R_CreateImage(name, data, width, height, depth, flags);
    ri.Free(data);
}</pre>
        </li>
        <li><strong>Add LUT Sampling Function:</strong> Add to tr_shader.c:
            <pre>
vec3 ApplyLUT(vec3 color) {
    if (r_lutEnable->integer == 0) {
        return color;
    }
    
    float intensity = r_lutIntensity->value;
    vec3 lutColor = texture(u_lut, color).rgb;
    
    // Apply blend mode
    switch (r_lutBlendMode->integer) {
        case 1: // Multiply
            lutColor = color * lutColor;
            break;
        case 2: // Screen
            lutColor = 1.0 - (1.0 - color) * (1.0 - lutColor);
            break;
        case 3: // Overlay
            lutColor = color < 0.5 ? 
                2.0 * color * lutColor : 
                1.0 - 2.0 * (1.0 - color) * (1.0 - lutColor);
            break;
    }
    
    // Apply animation if enabled
    if (r_lutAnimationSpeed->value != 0.0) {
        float time = float(gl_FragCoord.x) * 0.01 + float(gl_FragCoord.y) * 0.01;
        time *= r_lutAnimationSpeed->value;
        lutColor = mix(lutColor, texture(u_lutAnimated, color + time).rgb, 0.5);
    }
    
    return mix(color, lutColor, intensity);
}</pre>
        </li>
        <li><strong>Modify Fragment Shader:</strong> Update tr_shader.c:
            <pre>
void main() {
    vec3 color = texture(u_texture, v_texCoord).rgb;
    
    if (r_hdr->integer) {
        color *= r_exposure->value;
        if (r_acesTonemapping->integer) {
            color = ACESFilm(color);
        }
    }
    
    // Apply LUT after tonemapping
    color = ApplyLUT(color);
    
    // Apply post-LUT effects
    if (r_lutQuality->integer > 1) {
        color = ApplyPostProcessing(color);
    }
    
    fragColor = vec4(color, 1.0);
}</pre>
        </li>
    </ol>

    <h2>LUT File Format</h2>
    <p>Supported LUT formats:</p>
    <ul>
        <li>3D LUT (.cube)</li>
        <li>3D LUT (.3dl)</li>
        <li>3D LUT (.png)</li>
        <li>3D LUT (.exr) - HDR support</li>
        <li>3D LUT (.vlt) - Volumetric LUT</li>
    </ul>

    <h2>Testing</h2>
    <ul>
        <li>Compile with: <code>make</code></li>
        <li>Enable LUTs: <code>/r_lutEnable 1</code></li>
        <li>Adjust intensity: <code>/r_lutIntensity 0.5</code></li>
        <li>Test blend modes: <code>/r_lutBlendMode 1</code></li>
        <li>Test animation: <code>/r_lutAnimationSpeed 0.5</code></li>
        <li>Test streaming: <code>/r_lutStreaming 1</code></li>
        <li>Test quality: <code>/r_lutQuality 2</code></li>
        <li>Test with different LUT files</li>
        <li>Verify LUT application with HDR</li>
        <li>Test LUT application with ACES tonemapping</li>
        <li>Verify color accuracy</li>
    </ul>

    <h2>Performance Considerations</h2>
    <ul>
        <li>Use compressed LUT textures when possible</li>
        <li>Consider LUT resolution impact on performance</li>
        <li>Profile shader performance with LUTs enabled</li>
        <li>Monitor memory usage with multiple LUTs</li>
        <li>Use LUT streaming for large files</li>
        <li>Consider LUT baking for static scenes</li>
        <li>Optimize LUT quality settings for target hardware</li>
    </ul>

    <h2>Additional Resources</h2>
    <ul>
        <li><a href="https://www.khronos.org/vulkan/">Vulkan Documentation</a></li>
        <li><a href="https://www.khronos.org/opengl/wiki/Texture_Storage">OpenGL Texture Storage</a></li>
        <li><a href="https://www.khronos.org/opengl/wiki/3D_Textures">OpenGL 3D Textures</a></li>
        <li><a href="https://www.khronos.org/blog/vulkan-memory-management">Vulkan Memory Management</a></li>
    </ul>
</body>
</html>
