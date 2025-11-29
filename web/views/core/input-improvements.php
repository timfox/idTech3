<?php
/**
 * Input System Improvements
 */
$title = 'Input System Improvements - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/input-improvements' => 'Input System Improvements'
];
?>

<h1>Input System Improvements</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 engine has received significant improvements to the input system, providing better mouse input handling, reduced latency, and enhanced usability features.</p>
</div>

<div class="section">
    <h2>Raw Mouse Input</h2>
    
    <h3>What is Raw Mouse Input?</h3>
    <p>Raw mouse input bypasses the operating system's mouse acceleration and pointer smoothing, providing direct access to mouse movement data. This results in more precise and consistent mouse control, especially important for competitive gaming.</p>
    
    <h3>Automatic Detection</h3>
    <p>Raw mouse input is automatically enabled when available, replacing DirectInput on Windows systems:</p>
    <ul>
        <li>Detects raw input availability at startup</li>
        <li>Falls back to DirectInput if raw input is unavailable</li>
        <li>No configuration required - works automatically</li>
    </ul>
    
    <h3>Benefits</h3>
    <ul>
        <li><strong>Precision:</strong> Direct access to mouse hardware data</li>
        <li><strong>Consistency:</strong> No OS-level acceleration interference</li>
        <li><strong>Lower Latency:</strong> Reduced input processing overhead</li>
        <li><strong>Better Control:</strong> More predictable mouse movement</li>
    </ul>
    
    <h3>Configuration</h3>
    <p>Control mouse input source with the <code>in_mouse</code> CVar:</p>
    <div class="code-block">
        <pre><code># Enable mouse input (default)
/set in_mouse 1

# Values:
#  1 - Raw input / DirectInput (automatic detection)
#  0 - Disable mouse input
# -1 - Win32 mouse (legacy, Windows only)</code></pre>
    </div>
    
    <h3>Platform Support</h3>
    <ul>
        <li><strong>Windows:</strong> Raw Input API (Windows XP+)</li>
        <li><strong>Linux:</strong> evdev direct input</li>
        <li><strong>macOS:</strong> Core Graphics raw input</li>
    </ul>
</div>

<div class="section">
    <h2>Unlagged Mouse Events</h2>
    
    <h3>What is Unlagged Processing?</h3>
    <p>Unlagged mouse event processing processes mouse input before rendering, ensuring the most up-to-date mouse position is used for camera/view calculations. This reduces perceived input latency.</p>
    
    <h3>Default Behavior</h3>
    <p>Unlagged processing is enabled by default:</p>
    <ul>
        <li>Mouse input processed before frame rendering</li>
        <li>Reduces input-to-visual latency</li>
        <li>Provides smoother, more responsive feel</li>
    </ul>
    
    <h3>Configuration</h3>
    <p>Control mouse processing order with <code>in_lagged</code>:</p>
    <div class="code-block">
        <pre><code># Unlagged processing (default, recommended)
/set in_lagged 0

# Lagged processing (before framerate limiter)
/set in_lagged 1</code></pre>
    </div>
    
    <h3>When to Use Lagged Mode</h3>
    <p>Lagged mode may be useful for:</p>
    <ul>
        <li>Compatibility with certain mods</li>
        <li>Specific gameplay preferences</li>
        <li>Troubleshooting input issues</li>
    </ul>
    <p><strong>Note:</strong> Unlagged mode (default) provides better responsiveness and is recommended for most users.</p>
</div>

<div class="section">
    <h2>Window Minimize Hotkey</h2>
    
    <h3>Overview</h3>
    <p>The <code>in_minimize</code> CVar provides a hotkey to minimize/restore the game window, replacing the need for external tools like Q3Minimizer.</p>
    
    <h3>Usage</h3>
    <p>Bind a key to minimize/restore:</p>
    <div class="code-block">
        <pre><code># Bind F10 to minimize/restore
/bind F10 in_minimize

# Or use the CVar directly
/in_minimize</code></pre>
    </div>
    
    <h3>Features</h3>
    <ul>
        <li>Toggle minimize/restore with single keypress</li>
        <li>Works in fullscreen and windowed modes</li>
        <li>Windows-only feature</li>
        <li>Direct replacement for Q3Minimizer</li>
    </ul>
    
    <h3>Use Cases</h3>
    <ul>
        <li>Quickly switch to other applications</li>
        <li>During video recording (can minimize during recording)</li>
        <li>Multi-tasking while in game</li>
    </ul>
</div>

<div class="section">
    <h2>Video Pipe (FFmpeg Integration)</h2>
    
    <h3>Overview</h3>
    <p>The <code>video-pipe</code> command allows using an external FFmpeg binary for video encoding, providing better quality and smaller file sizes compared to the built-in encoder.</p>
    
    <h3>Prerequisites</h3>
    <ul>
        <li>FFmpeg installed and accessible in PATH</li>
        <li>Or specify full path to FFmpeg executable</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Start recording with FFmpeg pipe
/video-pipe demo_name

# Stop recording
/video-pipe

# Specify FFmpeg path (if not in PATH)
/video-pipe demo_name /path/to/ffmpeg</code></pre>
    </div>
    
    <h3>Benefits</h3>
    <ul>
        <li><strong>Better Quality:</strong> Advanced encoding options</li>
        <li><strong>Smaller Files:</strong> Better compression algorithms</li>
        <li><strong>More Formats:</strong> Support for various codecs</li>
        <li><strong>Custom Settings:</strong> Full FFmpeg parameter control</li>
    </ul>
    
    <h3>Configuration</h3>
    <p>FFmpeg encoding parameters can be configured via CVars or environment variables:</p>
    <div class="code-block">
        <pre><code># Set FFmpeg codec
/set cl_video_pipe_codec libx264

# Set quality preset
/set cl_video_pipe_preset medium

# Set bitrate
/set cl_video_pipe_bitrate 5000k</code></pre>
    </div>
    
    <h3>Window Minimization</h3>
    <p>Unlike built-in recording, you can minimize the game window during <code>video-pipe</code> recording without interrupting the capture.</p>
</div>

<div class="section">
    <h2>Mouse Smoothing and Filtering</h2>
    
    <h3>Mouse Filter</h3>
    <p>Enable mouse smoothing for reduced jitter:</p>
    <div class="code-block">
        <pre><code># Enable mouse filtering
/set m_filter 1

# Disable (default)
/set m_filter 0</code></pre>
    </div>
    
    <h3>Mouse Acceleration</h3>
    <p>Configure mouse acceleration for different movement styles:</p>
    <div class="code-block">
        <pre><code># Enable acceleration
/set m_accel 0.2

# Acceleration style (0 = Quake-style, 1 = Power function)
/set m_accelStyle 0

# Acceleration offset
/set m_accelOffset 0

# Acceleration power
/set m_accelPower 2</code></pre>
    </div>
</div>

<div class="section">
    <h2>Input System Architecture</h2>
    
    <h3>Input Processing Pipeline</h3>
    <ol>
        <li><strong>Hardware Input:</strong> Raw mouse/keyboard data</li>
        <li><strong>Input Capture:</strong> Raw input API or DirectInput</li>
        <li><strong>Event Processing:</strong> Mouse movement, button presses</li>
        <li><strong>Filtering:</strong> Optional smoothing/acceleration</li>
        <li><strong>Command Generation:</strong> Convert to game commands</li>
        <li><strong>Rendering:</strong> Apply to camera/view (unlagged)</li>
    </ol>
    
    <h3>Input Latency Reduction</h3>
    <p>Several improvements reduce input latency:</p>
    <ul>
        <li>Raw input bypasses OS processing</li>
        <li>Unlagged processing before rendering</li>
        <li>Direct hardware access</li>
        <li>Minimal buffering</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Considerations</h2>
    
    <h3>Input Overhead</h3>
    <ul>
        <li>Raw input has minimal CPU overhead</li>
        <li>Unlagged processing adds negligible cost</li>
        <li>Mouse filtering has small performance impact</li>
    </ul>
    
    <h3>Best Practices</h3>
    <ul>
        <li>Use raw input for best precision</li>
        <li>Keep unlagged mode enabled (default)</li>
        <li>Disable mouse filtering if not needed</li>
        <li>Configure acceleration to preference</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/input-system">Input System Documentation</a> - Complete input system reference</li>
        <li><a href="development/debugging">Debugging Tools</a> - Input debugging</li>
        <li><a href="getting-started/configuration">Configuration Guide</a> - Input configuration</li>
    </ul>
</div>

