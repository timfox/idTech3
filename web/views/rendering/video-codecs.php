<?php
$title = "Video Codec Support";
?>

<h1>Video Codec Support</h1>

<p>This document describes the video codec support system added to the engine.</p>

<h2>Overview</h2>

<p>The engine now supports multiple GPL 2 compliant video codecs for cinematic playback:</p>
<ul>
    <li><strong>ROQ</strong> - Original Quake 3 codec (always enabled)</li>
    <li><strong>Theora</strong> - Ogg Theora codec (optional, GPL 2 compatible)</li>
    <li><strong>VP8/VP9</strong> - WebM codecs (optional, BSD license, GPL 2 compatible)</li>
</ul>

<h2>Architecture</h2>

<p>The codec system is modular and extensible:</p>

<h3>1. Codec Detection</h3>
<p>Files: <code>cl_cin_codec.h</code>, <code>cl_cin_codec.c</code></p>
<ul>
    <li>Detects video format by magic number and file extension</li>
    <li>Routes to appropriate decoder</li>
</ul>

<h3>2. Main Cinematic System</h3>
<p>File: <code>cl_cin.c</code></p>
<ul>
    <li>Refactored to support multiple codecs</li>
    <li>Routes codec-specific operations to appropriate handlers</li>
</ul>

<h3>3. Codec Implementations</h3>
<ul>
    <li><code>cl_cin_theora.c</code> - Theora decoder</li>
    <li><code>cl_cin_vpx.c</code> - VP8/VP9 decoder</li>
</ul>

<h2>Building</h2>

<h3>Dependencies</h3>

<h4>Theora:</h4>
<pre><code>sudo apt-get install libtheora-dev</code></pre>

<h4>VPX (VP8/VP9):</h4>
<pre><code>sudo apt-get install libvpx-dev</code></pre>

<h3>CMake Options</h3>
<ul>
    <li><code>USE_THEORA</code> - Enable Theora support (default: ON)</li>
    <li><code>USE_VPX</code> - Enable VP8/VP9 support (default: ON)</li>
</ul>

<p>The build system will automatically detect available libraries and enable/disable support accordingly.</p>

<h2>Usage</h2>

<h3>File Formats</h3>
<ul>
    <li><strong>ROQ</strong>: <code>.roq</code> files (original Quake 3 format)</li>
    <li><strong>Theora</strong>: <code>.ogv</code> files (Ogg Theora)</li>
    <li><strong>VP8/VP9</strong>: <code>.webm</code> files (WebM format)</li>
</ul>

<h3>CVars</h3>
<ul>
    <li><code>cl_cinematic</code> - Enable cinematic playback (default: 1)</li>
    <li><code>cl_cinematicForce</code> - Force cinematic playback even if codec not available (default: 0)</li>
</ul>

<h3>Codec Selection</h3>
<p>The engine automatically selects the appropriate codec based on file extension and magic number detection. If multiple codecs are available, it will prefer the most efficient one for the file format.</p>

<h2>Performance</h2>

<p>Codec performance characteristics:</p>
<ul>
    <li><strong>ROQ</strong>: Fastest, but limited quality and resolution</li>
    <li><strong>Theora</strong>: Good quality, moderate CPU usage</li>
    <li><strong>VP8/VP9</strong>: Best quality, efficient compression, modern codec</li>
</ul>

<h2>Implementation Details</h2>

<h3>Codec Interface</h3>
<p>All codecs implement a common interface:</p>
<pre><code>typedef struct {
    qboolean (*init)(void);
    qboolean (*open)(const char *filename);
    void (*close)(void);
    qboolean (*decode_frame)(byte *buffer, int width, int height);
    int (*get_width)(void);
    int (*get_height)(void);
    float (*get_framerate)(void);
} codec_interface_t;</code></pre>

<h3>Error Handling</h3>
<p>The system gracefully falls back to ROQ if other codecs fail to load or decode. This ensures backward compatibility with existing content.</p>

<h2>Future Enhancements</h2>

<ul>
    <li>Hardware acceleration support (GPU decoding)</li>
    <li>Additional codec support (AV1, etc.)</li>
    <li>Streaming support for network cinematics</li>
    <li>Subtitle support</li>
</ul>

