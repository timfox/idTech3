<?php
/**
 * Sound System Documentation
 */
$title = 'Sound System - id Tech 3 Documentation';
$breadcrumbs = [
    '/sound' => 'Sound',
    '/sound/sound' => 'Sound System'
];
?>

<h1>Sound System</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 sound system provides 3D positional audio, music playback, and environmental audio effects.</p>
</div>

<div class="section">
    <h2>Features</h2>
    <ul>
        <li><strong>3D Audio:</strong> Positional sound with distance attenuation</li>
        <li><strong>Environmental Effects:</strong> Reverb and echo</li>
        <li><strong>Music System:</strong> Background music and dynamic tracks</li>
        <li><strong>Voice Chat:</strong> Real-time voice communication</li>
    </ul>
</div>

<div class="section">
    <h2>Audio Formats</h2>
    <ul>
        <li><strong>WAV:</strong> Uncompressed audio for sound effects</li>
        <li><strong>OGG:</strong> Compressed audio for music</li>
        <li><strong>Quality:</strong> 44.1kHz, 16-bit recommended</li>
    </ul>
</div>

<div class="section">
    <h2>Configuration</h2>
    <div class="code-block">
        <pre><code># Sound settings
seta s_volume "0.8"
seta s_musicvolume "0.25"
seta s_doppler "1"
seta s_distance "100"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="tools/asset-tools">Audio Asset Creation</a></li>
<li><a href="music">Music System Details</a></li>
    </ul>
</div> 