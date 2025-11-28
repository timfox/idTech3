<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ACES Tonemapping Implementation in id Tech 3</title>
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
    <h1>ACES Tonemapping Implementation in id Tech 3</h1>
    
    <div class="note">
        <strong>Note:</strong> This implementation requires Vulkan support and HDR-capable hardware.
    </div>

    <h2>Overview</h2>
    <p>This guide explains how to implement ACES (Academy Color Encoding System) tonemapping in id Tech 3 using Vulkan, working alongside the existing HDR support through r_hdr. This implementation allows for independent control of HDR and tonemapping.</p>

    <h2>Prerequisites</h2>
    <ul>
        <li>id Tech 3 Source Code</li>
        <li>Vulkan SDK</li>
        <li>ACES Reference Implementation</li>
        <li>Development Environment (C/C++, CMake)</li>
        <li>GLSL Knowledge</li>
        <li>Basic understanding of color spaces and HDR</li>
        <li>Familiarity with Vulkan pipeline setup</li>
        <li>HDR-capable display</li>
        <li>Vulkan-compatible GPU</li>
    </ul>

    <div class="warning">
        <strong>Warning:</strong> Make sure to backup your source code before making any changes.
    </div>

    <h3>1. ACES Tonemapping Function</h3>
    <p>Basic ACES tonemapping implementation in GLSL:</p>
    <pre>
vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}</pre>

    <h3>2. Fragment Shader Integration</h3>
    <p>Example fragment shader with ACES tonemapping, working alongside r_hdr:</p>
    <pre>
#version 450
in vec2 v_texCoord;
out vec4 fragColor;
uniform sampler2D u_texture;
uniform int u_hdr;  // Using existing r_hdr cvar
uniform int u_acesTonemapping;  // Separate tonemapping control
uniform float u_exposure;

vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 color = texture(u_texture, v_texCoord).rgb;
    if (u_hdr == 1) {
        color *= u_exposure;
        if (u_acesTonemapping == 1) {
            color = ACESFilm(color);
        }
    }
    fragColor = vec4(color, 1.0);
}</pre>

    <h3>3. CVar Implementation</h3>
    <p>Using existing r_hdr cvar and adding separate tonemapping control:</p>
    <pre>
// r_hdr is already defined in tr_local.h
cvar_t *r_acesTonemapping = Cvar_Get("r_acesTonemapping", "0", CVAR_ARCHIVE);
cvar_t *r_exposure = Cvar_Get("r_exposure", "1.0", CVAR_ARCHIVE);</pre>

    <h2>Best Practices</h2>
    <ul>
        <li>Use the simplified ACESFilm function for better performance</li>
        <li>Enable Vulkan validation layers during development</li>
        <li>Test on various maps and lighting conditions</li>
        <li>Adjust parameters based on your specific needs</li>
        <li>Keep HDR and tonemapping as separate controls</li>
        <li>Implement exposure control for fine-tuning</li>
        <li>Add gamma correction support</li>
        <li>Consider adding color grading options</li>
    </ul>

    <h2>Implementation Steps</h2>
    <ol>
        <li><strong>Add CVars:</strong> Add to tr_local.h:
            <pre>
cvar_t *r_acesTonemapping;
cvar_t *r_exposure;</pre>
        </li>
        <li><strong>Initialize CVars:</strong> Add to tr_init.c:
            <pre>
r_acesTonemapping = ri.Cvar_Get("r_acesTonemapping", "0", CVAR_ARCHIVE);
r_exposure = ri.Cvar_Get("r_exposure", "1.0", CVAR_ARCHIVE);</pre>
        </li>
        <li><strong>Add Shader Function:</strong> Add to tr_shader.c:
            <pre>
vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}</pre>
        </li>
        <li><strong>Modify Fragment Shader:</strong> Update tr_shader.c to include tonemapping:
            <pre>
void main() {
    vec3 color = texture(u_texture, v_texCoord).rgb;
    if (r_hdr->integer) {
        color *= r_exposure->value;
        if (r_acesTonemapping->integer) {
            color = ACESFilm(color);
        }
    }
    fragColor = vec4(color, 1.0);
}</pre>
        </li>
    </ol>

    <h2>Testing</h2>
    <ul>
        <li>Compile with: <code>make</code></li>
        <li>Test HDR with: <code>/r_hdr 1</code></li>
        <li>Test tonemapping with: <code>/r_acesTonemapping 1</code></li>
        <li>Adjust exposure with: <code>/r_exposure 1.5</code></li>
        <li>Verify HDR scenes work with and without tonemapping</li>
        <li>Test with different exposure values</li>
        <li>Test with different HDR displays</li>
        <li>Verify color accuracy across different scenes</li>
    </ul>

    <h2>Performance Considerations</h2>
    <ul>
        <li>Monitor GPU usage during tonemapping</li>
        <li>Test performance impact on different hardware</li>
        <li>Consider adding quality presets for different hardware</li>
        <li>Profile shader performance</li>
    </ul>

    <h2>Additional Resources</h2>
    <ul>
        <li><a href="https://github.com/ampas/aces-core">ACES Core Repository</a></li>
        <li><a href="https://github.com/timfox/idtech3">id Tech 3 Source</a></li>
        <li><a href="https://vulkan.lunarg.com">Vulkan SDK</a></li>
        <li><a href="https://acescentral.com">ACES Central</a></li>
        <li><a href="https://www.khronos.org/vulkan/">Vulkan Documentation</a></li>
        <li><a href="https://www.khronos.org/opengl/wiki/High_Dynamic_Range">OpenGL HDR Guide</a></li>
        <li><a href="https://www.khronos.org/blog/hdr-display-apis">HDR Display APIs</a></li>
    </ul>
</body>
</html>
