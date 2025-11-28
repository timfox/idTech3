<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Music Implementation in id Tech 3</title>
    <style>
        body {
            font-family: "Courier New", monospace;
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
            color: #f0f;
            text-shadow: 2px 2px #0f0;
        }
        a {
            color: #0ff;
        }
        a:hover {
            color: #f0f;
        }
    </style>
</head>
<body>
    <h1>Music Implementation in id Tech 3</h1>
    
    <h2>Overview</h2>
    <p>id Tech 3 supports various music formats including MP3, OGG, and WAV. Music can be implemented for different game states such as the main menu, loading screens, and in-game maps.</p>

    <h2>Music File Structure</h2>
    <p>Music files should be placed in the following directory structure:</p>
    <pre>
base/
├── music/
│   ├── menu/
│   │   └── main.mp3
│   ├── maps/
│   │   ├── q3dm1.mp3
│   │   └── q3dm2.mp3
│   └── loading/
│       └── loading.mp3
    </pre>

    <h2>Implementation Methods</h2>

    <h3>1. Main Menu Music</h3>
    <p>To implement main menu music, add the following to your <code>ui_mainmenu.c</code>:</p>
    <pre>
void UI_InitMainMenu(void) {
    // Start menu music
    trap_S_StartBackgroundTrack("music/menu/main.mp3", "music/menu/main.mp3");
}
    </pre>

    <h3>2. Map Music</h3>
    <p>For map-specific music, add the following to your map's <code>.arena</code> file:</p>
    <pre>
{
    "mapname" "q3dm1"
    "longname" "Arena 1"
    "music" "music/maps/q3dm1.mp3"
    "type" "ffa"
}
    </pre>

    <h3>3. Loading Screen Music</h3>
    <p>To implement loading screen music, modify your <code>ui_loading.c</code>:</p>
    <pre>
void UI_LoadScreen(void) {
    // Play loading music
    trap_S_StartBackgroundTrack("music/loading/loading.mp3", "music/loading/loading.mp3");
}
    </pre>

    <h2>Music Control Functions</h2>
    <p>Common music control functions in id Tech 3:</p>
    <pre>
// Start background music
trap_S_StartBackgroundTrack(const char *intro, const char *loop);

// Stop background music
trap_S_StopBackgroundTrack(void);

// Fade out music
trap_S_FadeBackgroundTrack(float targetvol, int time, int fade);

// Set music volume
trap_S_SetVolume(float volume);
    </pre>

    <h2>Best Practices</h2>
    <ul>
        <li>Use OGG format for better compression and quality</li>
        <li>Keep music files under 5MB for optimal performance</li>
        <li>Implement proper fade transitions between different music tracks</li>
        <li>Provide volume control options for music in the game settings</li>
        <li>Use appropriate music for different game modes and maps</li>
    </ul>

    <h2>Example Implementation</h2>
    <p>Here's a complete example of implementing music in a map:</p>
    <pre>
void Map_Init(void) {
    // Start map music with fade in
    trap_S_StartBackgroundTrack("music/maps/q3dm1.mp3", "music/maps/q3dm1.mp3");
    trap_S_FadeBackgroundTrack(1.0f, 1000, 0);
}

void Map_Shutdown(void) {
    // Fade out music before map change
    trap_S_FadeBackgroundTrack(0.0f, 1000, 0);
    trap_S_StopBackgroundTrack();
}
    </pre>

    <h2>Common Issues and Solutions</h2>
    <ul>
        <li><strong>Music not playing:</strong> Check file paths and format compatibility</li>
        <li><strong>Audio glitches:</strong> Ensure music files are properly encoded and not corrupted</li>
        <li><strong>Performance issues:</strong> Optimize music file size and bitrate</li>
        <li><strong>Volume problems:</strong> Verify volume settings in both code and game settings</li>
    </ul>
</body>
</html>

