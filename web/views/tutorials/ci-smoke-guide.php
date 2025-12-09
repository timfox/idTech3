<?php
/**
 * CI Smoke & Build Guide
 */
$title = 'CI Smoke & Build Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/ci-smoke-guide' => 'CI Smoke & Build Guide'
];
?>

<h1>CI Smoke & Build Guide</h1>

<div class="section">
    <h2>Local Smoke Test</h2>
    <p>Run the bundled smoke script to build and sanity-check:</p>
    <div class="code-block">
        <pre><code>ENGINE_BIN=./build/idtech3.x86_64 MOD_LIST="mymod blacksun" ./tools/ci_smoke.sh</code></pre>
    </div>
    <ul>
        <li>Builds engine and specified mods.</li>
        <li>Basic sanity checks; adjust <code>MOD_LIST</code> for your targets.</li>
    </ul>
</div>

<div class="section">
    <h2>Suggested CI Matrix</h2>
    <ul>
        <li>Linux (GCC & Clang): Release and Debug.</li>
        <li>Windows (MSVC): Release.</li>
        <li>Options toggles: GL/VK enabled; ENT/NO-ENT if applicable.</li>
    </ul>
</div>

<div class="section">
    <h2>Validation Steps</h2>
    <ul>
        <li>Build engine: <code>./tools/compile_engine.sh Release</code>.</li>
        <li>Run shader validation: <code>glslangValidator</code> and <code>spirv-val</code> on Vulkan shaders.</li>
        <li>Run smoke script per target mod.</li>
    </ul>
</div>

<div class="section">
    <h2>Caching</h2>
    <ul>
        <li>Use <code>ccache</code> or compiler cache to speed rebuilds.</li>
        <li>Persist CMake build directories between CI runs when possible.</li>
        <li>Optionally persist <code>release/pipeline_cache_vk.bin</code> per-build-config to warm Vulkan pipelines.</li>
    </ul>
</div>

<div class="section">
    <h2>Artifacts</h2>
    <ul>
        <li>Upload <code>release/</code> binaries and renderer .so/.dll files.</li>
        <li>Include logs from shader validation and build output on failure.</li>
    </ul>
</div>

<div class="section">
    <h2>Recommended Flags</h2>
    <ul>
        <li>Enable <code>-Wextra -Wpedantic</code> in CI to catch warnings early.</li>
        <li>Keep validation layers on for Vulkan debug builds.</li>
    </ul>
</div>

