<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>id Tech 6 Engine Features</title>
    <style>
        @font-face {
            font-family: 'Fusion';
            src: url('/fonts/fusion.ttf') format('truetype');
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
            font-family: 'Fusion', sans-serif;
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

        .formula {
            font-family: 'Consolas', monospace;
            background-color: rgba(0, 0, 0, 0.5);
            padding: 10px;
            border-radius: 3px;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <h1>id Tech 6 Engine Features</h1>

    <h2>Core Technology</h2>
    <ul>
        <li>Graphics API Support:
            <ul>
                <li>Vulkan API (Primary)</li>
                <li>OpenGL API</li>
                <li>Multi-platform support (Windows, PS4, Xbox One, Switch, Stadia)</li>
            </ul>
        </li>
        <li>Rendering Features:
            <ul>
                <li>Physically based rendering</li>
                <li>Motion blur and bokeh depth of field</li>
                <li>HDR bloom and shadow mapping</li>
                <li>Screen space reflections</li>
                <li>Volumetric lighting and smoke</li>
                <li>Skin sub-surface scattering</li>
                <li>Chromatic aberration</li>
                <li>Normal maps</li>
            </ul>
        </li>
    </ul>

    <h2>Advanced Visual Effects</h2>
    <ul>
        <li>Anti-aliasing Solutions:
            <ul>
                <li>FXAA</li>
                <li>SMAA</li>
                <li>TSSAA</li>
            </ul>
        </li>
        <li>Lighting Systems:
            <ul>
                <li>Dynamic lighting and shadows</li>
                <li>Lightmaps and irradiance volumes</li>
                <li>Image-based lighting</li>
                <li>Directional occlusion</li>
            </ul>
        </li>
    </ul>

    <h2>Environment Features</h2>
    <ul>
        <li>World Systems:
            <ul>
                <li>Destructible environments</li>
                <li>Advanced water physics</li>
                <li>Unified volumetric fog</li>
                <li>Dynamic water caustics</li>
                <li>Tessellated water surface (on-the-fly without GPU tessellation)</li>
            </ul>
        </li>
        <li>Particle Systems:
            <ul>
                <li>GPU accelerated particles</li>
                <li>Dynamic lighting and shadowing</li>
                <li>Advanced particle effects</li>
            </ul>
        </li>
    </ul>

    <h2>Performance Optimization</h2>
    <ul>
        <li>Technical Features:
            <ul>
                <li>Triple buffer v-sync (fast sync)</li>
                <li>Dynamic resolution scaling</li>
                <li>Optimized texture streaming</li>
                <li>Efficient memory management</li>
            </ul>
        </li>
    </ul>

    <h2>Development History</h2>
    <ul>
        <li>Engine Evolution:
            <ul>
                <li>Originally planned with voxel-based raycasting approach</li>
                <li>Later shifted to conventional mesh-based rasterization</li>
                <li>Developed after Carmack's departure from id Software</li>
                <li>Led by Tiago Sousa (former Crytek engineer)</li>
            </ul>
        </li>
    </ul>

    <h2>SIGGRAPH 2016 Presentation Highlights</h2>
    <ul>
        <li>Rendering Pipeline:
            <ul>
                <li>Hybrid Forward Rendering optimized for modern GPUs</li>
                <li>Thin G-Buffer for screen-space effects</li>
                <li>Clustered Forward Shading with 3D grid system</li>
            </ul>
        </li>
        <li>Advanced Shading:
            <ul>
                <li>Physically-Based Shading with energy conservation</li>
                <li>Material parameters: albedo, metallic, roughness, normal maps</li>
                <li>Voxel-based volumetric lighting implementation</li>
            </ul>
        </li>
        <li>Technical Innovations:
            <ul>
                <li>Multi-threaded command buffer generation</li>
                <li>Asynchronous compute for post-processing</li>
                <li>Advanced temporal techniques including TAA</li>
                <li>Motion vector tracking for improved motion blur</li>
            </ul>
        </li>
    </ul>

    <div class="note">
        <strong>Note:</strong> id Tech 6 represents a significant advancement in game engine technology, focusing on high-performance rendering at 1080p/60fps while maintaining visual fidelity. The engine reintroduces real-time dynamic lighting and improves upon the virtual texturing system from previous versions. The engine was first used in the 2016 Doom game and later in Wolfenstein II: The New Colossus, Doom VFR, Wolfenstein: Youngblood, and Wolfenstein: Cyberpilot.
    </div>
</body>
</html>
