<?php
/**
 * Filesystem Improvements
 */
$title = 'Filesystem Improvements - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/filesystem-improvements' => 'Filesystem Improvements'
];
?>

<h1>Filesystem Improvements</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 engine has received significant filesystem improvements, including raised limits, better directory scanning, and enhanced mod support.</p>
</div>

<div class="section">
    <h2>Raised Filesystem Limits</h2>
    
    <h3>Map Directory Limit</h3>
    <p>The engine can now handle up to <strong>20,000 maps</strong> in a single directory, a massive increase from previous limitations.</p>
    
    <h3>Benefits</h3>
    <ul>
        <li>Support for large map collections</li>
        <li>No need to split maps across directories</li>
        <li>Better organization possibilities</li>
        <li>Improved server map rotation support</li>
    </ul>
    
    <h3>Performance</h3>
    <p>Despite the increased limit, performance remains excellent:</p>
    <ul>
        <li>Efficient directory scanning</li>
        <li>Lazy loading of map information</li>
        <li>Optimized file system queries</li>
    </ul>
</div>

<div class="section">
    <h2>Recursive Directory Scanning</h2>
    
    <h3>Subdirectory Support</h3>
    <p>The filesystem now supports recursive directory scanning via the <code>FS_MATCH_SUBDIRS</code> flag:</p>
    <ul>
        <li>Enabled for config file completion</li>
        <li>Enabled for demo file completion</li>
        <li>Scans subdirectories automatically</li>
    </ul>
    
    <h3>Use Cases</h3>
    <ul>
        <li>Organized config file structures</li>
        <li>Demo files in subdirectories</li>
        <li>Better file organization</li>
    </ul>
    
    <h3>Configuration</h3>
    <p>Recursive scanning is automatically enabled for:</p>
    <div class="code-block">
        <pre><code># Config files - scans recursively
/complist cfg

# Demo files - scans recursively  
/complist demo</code></pre>
    </div>
</div>

<div class="section">
    <h2>Improved Mod Support</h2>
    
    <h3>Mod List Sorting</h3>
    <p>Mod lists are now automatically sorted alphabetically for easier browsing.</p>
    
    <h3>Mod Description Cleanup</h3>
    <p>Mod descriptions are automatically cleaned:</p>
    <ul>
        <li>Trailing newlines stripped</li>
        <li>Better display formatting</li>
        <li>Consistent presentation</li>
    </ul>
    
    <h3>Mod Detection</h3>
    <p>Improved mod detection and loading:</p>
    <ul>
        <li>Better mod directory scanning</li>
        <li>Faster mod enumeration</li>
        <li>More reliable mod loading</li>
    </ul>
</div>

<div class="section">
    <h2>Library Loading Improvements</h2>
    
    <h3>Predefined Search Paths</h3>
    <p>The <code>FS_LoadLibrary()</code> function has been reworked:</p>
    <ul>
        <li>Uses predefined search paths only</li>
        <li>No longer references current game directory</li>
        <li>More secure library loading</li>
        <li>Prevents accidental library loading from wrong locations</li>
    </ul>
    
    <h3>Security Benefits</h3>
    <ul>
        <li>Prevents loading libraries from unexpected locations</li>
        <li>More predictable behavior</li>
        <li>Better security model</li>
    </ul>
</div>

<div class="section">
    <h2>Directory Traversal Fixes</h2>
    
    <h3>Pattern Matching</h3>
    <p>Fixed crash on directory traversal pattern match:</p>
    <ul>
        <li>Robust pattern matching</li>
        <li>No crashes on edge cases</li>
        <li>Better error handling</li>
    </ul>
    
    <h3>Edge Cases Handled</h3>
    <ul>
        <li>Invalid patterns</li>
        <li>Deep directory structures</li>
        <li>Special characters in paths</li>
        <li>Very long paths</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Improvements</h2>
    
    <h3>Optimized Scanning</h3>
    <ul>
        <li>Faster directory enumeration</li>
        <li>Reduced system calls</li>
        <li>Better caching</li>
        <li>Lazy loading where possible</li>
    </ul>
    
    <h3>Memory Efficiency</h3>
    <ul>
        <li>Efficient file list storage</li>
        <li>Minimal memory overhead</li>
        <li>Better memory management</li>
    </ul>
</div>

<div class="section">
    <h2>Backward Compatibility</h2>
    <p>All filesystem improvements are backward compatible:</p>
    <ul>
        <li>Existing mods continue to work</li>
        <li>No changes required to existing content</li>
        <li>Automatic benefit from improvements</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/filesystem-v2">Virtual Filesystem v2.0</a> - Modern mount management system</li>
        <li><a href="core/filesystem">Filesystem Documentation</a> - Complete filesystem reference</li>
        <li><a href="development/modding">Modding Guide</a> - Creating mods</li>
        <li><a href="development/map-making">Map Making</a> - Creating maps</li>
    </ul>
</div>

