<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>id Tech 3 Engine Features</title>
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
    </style>
</head>
<body>
    <h1>id Tech 3 Engine Features</h1>

    <h2>Core Features</h2>
    <ul>
        <li>Advanced 3D rendering engine with support for:
            <ul>
                <li>Dynamic lighting and shadows</li>
                <li>Curved surfaces and bezier patches</li>
                <li>Particle systems and effects</li>
                <li>Dynamic skyboxes and environment mapping</li>
            </ul>
        </li>
        <li>Cross-platform support:
            <ul>
                <li>Windows (32/64-bit)</li>
                <li>Linux/Unix</li>
                <li>macOS</li>
                <li>Android</li>
            </ul>
        </li>
        <li>Multiple renderer backends:
            <ul>
                <li>Vulkan (default)</li>
                <li>OpenGL</li>
                <li>OpenGL 2.0</li>
            </ul>
        </li>
    </ul>

    <h2>Technical Capabilities</h2>
    <div class="note">
        <strong>Note:</strong> The engine supports both client and dedicated server modes, with optional unified client/server builds.
    </div>
    <ul>
        <li>Network Architecture:
            <ul>
                <li>Client-server architecture</li>
                <li>UDP-based networking</li>
                <li>Lag compensation</li>
                <li>Server-side hit detection</li>
            </ul>
        </li>
        <li>Physics System:
            <ul>
                <li>BSP-based collision detection</li>
                <li>Dynamic object physics</li>
                <li>Vehicle physics support</li>
                <li>Water and liquid simulation</li>
            </ul>
        </li>
        <li>Audio System:
            <ul>
                <li>3D positional audio</li>
                <li>Environmental audio effects</li>
                <li>Multiple audio backends (OpenAL, SDL)</li>
            </ul>
        </li>
    </ul>

    <h2>Development Features</h2>
    <ul>
        <li>Mod Support:
            <ul>
                <li>QVM-based mod system</li>
                <li>Dynamic library loading</li>
                <li>Custom shader support</li>
                <li>Map and asset loading</li>
            </ul>
        </li>
        <li>Development Tools:
            <ul>
                <li>Integrated console system</li>
                <li>Debug visualization tools</li>
                <li>Performance monitoring</li>
                <li>Network debugging tools</li>
            </ul>
        </li>
    </ul>

    <h2>Performance Optimizations</h2>
    <ul>
        <li>Rendering:
            <ul>
                <li>Level of detail (LOD) system</li>
                <li>Occlusion culling</li>
                <li>Frustum culling</li>
                <li>Dynamic light optimization</li>
            </ul>
        </li>
        <li>Memory Management:
            <ul>
                <li>Efficient memory allocation</li>
                <li>Resource streaming</li>
                <li>Texture compression</li>
                <li>Asset caching</li>
            </ul>
        </li>
    </ul>

    <div class="note">
        <strong>Note:</strong> The engine is designed to be highly configurable through console variables and configuration files, allowing for extensive customization of both visual and gameplay features.
    </div>
</body>
</html>
