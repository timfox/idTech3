<?php
/**
 * Reload UI Documentation
 */
$title = 'Reload UI Changes - id Tech 3 Documentation';
$breadcrumbs = [
    '/ui' => 'UI',
    '/ui/reload-ui' => 'Reload UI Changes'
];
?>

<div class="content-section">
    <h1>How to See UI Changes</h1>
    
    <blockquote>
        <strong>Quick Reference:</strong> This guide explains how to reload UI changes in your mod so you can see updates without fully restarting the game.
    </blockquote>

    <div class="section">
        <h2>Quick Steps</h2>
        <p>Follow these steps to see UI changes in your mod:</p>
        
        <ol>
            <li><strong>Open game console</strong> (press <code>~</code> key)</li>
            <li><strong>Check vm_ui setting:</strong>
                <div class="code-block">
                    <pre><code>/vm_ui</code></pre>
                </div>
                <ul>
                    <li>Should show: <code>vm_ui = 0</code></li>
                    <li>If not 0, type: <code>/set vm_ui 0</code></li>
                </ul>
            </li>
            <li><strong>Check fs_game setting:</strong>
                <div class="code-block">
                    <pre><code>/fs_game</code></pre>
                </div>
                <ul>
                    <li>Should show: <code>fs_game = mymod</code> (or your mod name)</li>
                    <li>If not your mod name, type: <code>/set fs_game mymod</code></li>
                </ul>
            </li>
            <li><strong>COMPLETELY EXIT the game</strong> (not just minimize)</li>
            <li><strong>Restart the game</strong></li>
            <li><strong>Load your mod</strong></li>
            <li><strong>Navigate to:</strong> System Setup → Graphics</li>
            <li>You should see all new rendering options!</li>
        </ol>
    </div>

    <div class="section">
        <h2>Troubleshooting</h2>
        
        <h3>If UI Changes Still Don't Appear</h3>
        <p>Try these additional steps:</p>
        <ul>
            <li><strong>Reload UI:</strong> Type <code>/vid_restart</code> in console (reloads UI)</li>
            <li><strong>Check console for errors:</strong> Look for any error messages related to UI loading</li>
            <li><strong>Verify UI module exists:</strong> Check that <code>mymod/vm/uix86_64.so</code> exists and is recent</li>
            <li><strong>Check file permissions:</strong> Ensure the UI module file is readable</li>
        </ul>

        <h3>Common Issues</h3>
        <div class="note">
            <strong>vm_ui Setting:</strong> The <code>vm_ui</code> CVAR controls whether the UI runs as a virtual machine (QVM) or as a native library. Setting it to 0 enables native UI loading, which is required for UI changes to take effect.
        </div>

        <div class="note">
            <strong>fs_game Setting:</strong> The <code>fs_game</code> CVAR specifies which mod directory to load. Make sure this matches your mod's directory name exactly.
        </div>

        <div class="note">
            <strong>Full Restart Required:</strong> UI modules are loaded at game startup. Simply minimizing and restoring the game window is not sufficient - you must completely exit and restart the game for UI changes to take effect.
        </div>
    </div>

    <div class="section">
        <h2>Understanding UI Loading</h2>
        
        <h3>UI Module Types</h3>
        <p>The engine supports two types of UI modules:</p>
        <ul>
            <li><strong>QVM (Virtual Machine):</strong> Compiled bytecode (.qvm files)</li>
            <li><strong>Native Library:</strong> Shared library (.so on Linux, .dll on Windows, .dylib on macOS)</li>
        </ul>

        <h3>UI Module Location</h3>
        <p>UI modules are loaded from:</p>
        <div class="code-block">
            <pre><code>&lt;mod_directory&gt;/vm/ui&lt;platform&gt;.&lt;extension&gt;
Examples:
  mymod/vm/uix86_64.so      (Linux 64-bit)
  mymod/vm/uiwin64.dll      (Windows 64-bit)
  mymod/vm/uimac64.dylib    (macOS 64-bit)</code></pre>
        </div>

        <h3>Loading Process</h3>
        <p>The UI module is loaded during game initialization:</p>
        <ol>
            <li>Engine checks <code>vm_ui</code> CVAR</li>
            <li>If <code>vm_ui = 0</code>, attempts to load native library</li>
            <li>If native library not found, falls back to QVM</li>
            <li>UI module is initialized and menu system is set up</li>
        </ol>
    </div>

    <div class="section">
        <h2>Console Commands</h2>
        
        <h3>UI-Related Commands</h3>
        <ul>
            <li><code>/vm_ui</code> - Show current vm_ui setting</li>
            <li><code>/set vm_ui 0</code> - Enable native UI loading</li>
            <li><code>/fs_game</code> - Show current mod directory</li>
            <li><code>/set fs_game &lt;modname&gt;</code> - Set mod directory</li>
            <li><code>/vid_restart</code> - Restart video subsystem (reloads UI)</li>
        </ul>
    </div>

    <div class="section">
        <h2>Development Workflow</h2>
        
        <h3>Recommended Workflow</h3>
        <ol>
            <li>Make changes to your UI code</li>
            <li>Compile/rebuild your UI module</li>
            <li>Verify <code>vm_ui = 0</code> in console</li>
            <li>Verify <code>fs_game</code> points to your mod</li>
            <li>Completely exit the game</li>
            <li>Restart the game</li>
            <li>Load your mod</li>
            <li>Test UI changes</li>
        </ol>

        <h3>Quick Testing</h3>
        <p>For quick UI testing without full restart:</p>
        <ul>
            <li>Use <code>/vid_restart</code> to reload UI without restarting</li>
            <li>Note: This may not work for all UI changes, especially structural changes</li>
            <li>Full restart is recommended for reliable testing</li>
        </ul>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="ui/ui">UI System</a> - Complete UI system documentation</li>
            <li><a href="core/virtual-machine">Virtual Machine</a> - QVM system details</li>
            <li><a href="development/modding">Modding Guide</a> - Mod development guide</li>
            <li><a href="core/filesystem">Filesystem</a> - File system and mod loading</li>
        </ul>
    </div>
</div>

