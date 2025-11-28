<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>id Tech 7 Engine Features</title>
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
    <h1>id Tech 7 Engine Features</h1>

    <h2>Rendering Architecture & Performance</h2>
    <ul>
        <li>Graphics API:
            <ul>
                <li>Vulkan-only backend on PC</li>
                <li>Improved multi-threading support</li>
                <li>Enhanced GPU resource management</li>
            </ul>
        </li>
        <li>Rendering Pipeline:
            <ul>
                <li>Fully forward rendering pipeline</li>
                <li>Job-based multithreading system</                <li>Support for up to 1000 FPS</li>
            </ul>
        </li>
    </ul>

    <h2>Visual Enhancements</h2>
    <ul>
        <li>Dynamic Features:
            <ul>
                <li>Destructible demon models</li>
                <li>10x geometric detail increase</li>
                <li>Enhanced texture fidelity</li>
            </ul>
        </li>
        <li>Rendering Systems:
            <ul>
                <li>Multi-layered PBR material compositing</li>
                <li>Unified HDR lighting and shadowing</li>
                <li>Advanced GPU-accelerated particle system</li>
            </ul>
        </li>
    </ul>

    <h2>System-Level Improvements</h2>
    <ul>
        <li>Core Systems:
            <ul>
                <li>Removed MegaTexture pipeline</li>
                <li>Enhanced LOD management</li>
                <li>Expanded decal system</li>
                <li>Optimized image streaming</li>
            </ul>
        </li>
    </ul>

    <h2>Advanced Graphics Features</h2>
    <ul>
        <li>Modern Technologies:
            <ul>
                <li>Ray-traced reflections (post-launch)</li>
                <li>NVIDIA DLSS integration</li>
                <li>Variable Rate Shading (VRS)</li>
            </ul>
        </li>
    </ul>

    <h2>Games Using id Tech 7</h2>
    <ul>
        <li>Released Titles:
            <ul>
                <li>DOOM Eternal (2020)</li>
                <li>Indiana Jones and the Great Circle (2024)</li>
            </ul>
        </li>
    </ul>

    <div class="note">
        <strong>Note:</strong> id Tech 7 represents a significant evolution from id Tech 6, first showcased in DOOM Eternal. The engine emphasizes high performance, visual fidelity, and scalability across platforms. Key improvements include the removal of MegaTexture, implementation of a fully forward rendering pipeline, and support for modern graphics technologies like ray tracing and DLSS.
    </div>
</body>
</html>
