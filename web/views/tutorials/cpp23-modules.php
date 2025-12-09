<?php
/**
 * C23 / C++23 Migration Highlights
 */
$title = 'C23 / C++23 Migration Highlights - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/cpp23-modules' => 'C23 / C++23 Migration Highlights'
];
?>

<h1>C23 / C++23 Migration Highlights</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The engine and mod codebases have been updated to target C23 and C++23. This tutorial summarizes the key areas you can leverage now.</p>
</div>

<div class="section">
    <h2>C23 (C code)</h2>
    <ul>
        <li><strong>Standard:</strong> <code>CMAKE_C_STANDARD 23</code> is enabled.</li>
        <li><strong>Designated initializers:</strong> use modern designated init consistently.</li>
        <li><strong>Attributes:</strong> prefer <code>[[maybe_unused]]</code>, <code>[[nodiscard]]</code> equivalents where applicable.</li>
    </ul>
</div>

<div class="section">
    <h2>C++23 (game code)</h2>
    <ul>
        <li><strong>Std lib:</strong> you can use <code>std::expected</code>, <code>std::span</code>, <code>std::string_view</code>, and modern containers.</li>
        <li><strong>Feature flags:</strong> Blacksun exposes a C++23-backed registry (<code>bs_features.cpp</code>) with a C ABI.</li>
        <li><strong>ECS helpers:</strong> new setters and lifetime system use modern C++ in ECS code paths.</li>
    </ul>
</div>

<div class="section">
    <h2>How to Opt In</h2>
    <ol>
        <li>Include C++ files in the mod (see <code>mymod/gamesrc/CMakeLists.txt</code> targeting C++23).</li>
        <li>Link against existing engine headers; prefer <code>std::span</code>/<code>std::string_view</code> over raw pointers where possible.</li>
        <li>Keep ABI boundaries C-compatible when crossing into the VM/syscall interface.</li>
    </ol>
</div>

<div class="section">
    <h2>Recommended Practices</h2>
    <ul>
        <li>Use <code>std::expected</code> for loaders/parsers to return rich errors.</li>
        <li>Use <code>std::span</code> for contiguous data and avoid raw length+pointer pairs.</li>
        <li>Add <code>[[nodiscard]]</code> to functions where ignoring results is a bug.</li>
        <li>Keep C ABI stable: wrap C++23 logic behind C-callable shims when interfacing with legacy code.</li>
    </ul>
</div>

