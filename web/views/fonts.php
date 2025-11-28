<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>id Tech 3 Font System</title>
    <style>
        :root {
            --primary-color: #0f0;
            --background-color: #000;
            --warning-color: #f00;
            --max-width: 80ch;
        }

        body {
            font-family: "Courier New", monospace;
            background: var(--background-color);
            color: var(--primary-color);
            max-width: var(--max-width);
            margin: 0 auto;
            padding: 2rem;
            white-space: pre-wrap;
            width: var(--max-width);
        }

        p {
            max-width: var(--max-width);
            margin-bottom: 1rem;
        }

        .ascii-art {
            color: var(--primary-color);
            text-align: center;
            margin: 2rem 0;
            font-size: 1.2rem;
            letter-spacing: 0.5px;
        }

        .group {
            color: var(--primary-color);
            font-weight: bold;
            display: block;
            margin: 1.5rem 0 1rem;
            font-size: 1.1rem;
        }

        .info {
            color: var(--primary-color);
            padding: 0.5rem;
            border-left: 2px solid var(--primary-color);
            margin: 1rem 0;
        }

        .warning {
            color: var(--warning-color);
            padding: 0.5rem;
            border-left: 2px solid var(--warning-color);
            margin: 1rem 0;
        }
    </style>
    </style>
</head>
<body>
<div class="ascii-art">
    ███████╗ ██████╗ ███╗   ██╗████████╗███████╗
    ██╔════╝██╔═══██╗████╗  ██║╚══██╔══╝██╔════╝
    █████╗  ██║   ██║██╔██╗ ██║   ██║   ███████╗
    ██╔══╝  ██║   ██║██║╚██╗██║   ██║   ╚════██║
    ██║     ╚██████╔╝██║ ╚████║   ██║   ███████║
    ╚═╝      ╚═════╝ ╚═╝  ╚═══╝   ╚═╝   ╚══════╝
</div>

<span class="group">[RELEASE INFO]</span>
Title: id Tech 3 Font System Documentation
Type: Technical Documentation

<h2>Overview</h2>
<p>id Tech 3 supports custom fonts through its UI system. The engine uses a font management system that allows for different font sizes and styles to be used throughout the game interface.</p>

<h2>Font Structure</h2>
<p>The font system is defined in the UI code with the following key components:</p>
<ul>
    <li><code>fontInfo_t</code> - Structure containing font information</li>
    <li><code>textFont</code> - Main text font</li>
    <li><code>smallFont</code> - Smaller text font</li>
    <li><code>bigFont</code> - Larger text font</li>
</ul>

<h2>Font Properties</h2>
<p>Fonts in id Tech 3 can be customized with the following properties:</p>
<ul>
    <li>Font name/path</li>
    <li>Font size</li>
    <li>Font style (normal, shadowed)</li>
    <li>Text alignment</li>
    <li>Text scale</li>
</ul>

<h2>Implementation</h2>
<p>To use custom fonts in your id Tech 3 UI:</p>
<ol>
    <li>Place your font files in the <code>fonts/</code> directory of your game's base folder</li>
    <li>Register the font using the UI system</li>
    <li>Set font properties in your menu definitions</li>
</ol>

<span class="group">[EXAMPLE CODE]</span>
{
    font "fonts/customfont.ttf"
    textscale 1.0
    textStyle 0  // 0 for normal, 1 for shadowed
    textalignment 0  // 0 for left, 1 for center, 2 for right
}
    </pre>

<h2>Font Registration</h2>
<p>The engine automatically registers fonts when they are first used. The registration process includes:</p>
<ul>
    <li>Loading the font file</li>
    <li>Creating font textures</li>
    <li>Setting up font metrics</li>
</ul>

<h2>Best Practices</h2>
<ul>
    <li>Use standard font formats (TTF, OTF)</li>
    <li>Keep font file sizes reasonable</li>
    <li>Test fonts at different scales</li>
    <li>Consider using different fonts for different UI elements</li>
</ul>

<h2>Limitations</h2>
<ul>
    <li>Font files must be compatible with the engine's font renderer</li>
    <li>Maximum font size is limited by texture memory</li>
    <li>Some special characters may not render correctly</li>
</ul>
</body>
</html>
