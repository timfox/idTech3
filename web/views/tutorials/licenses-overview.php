<?php
/**
 * Licensing Overview (id Tech Engines)
 */
$title = 'Licensing Overview - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/licenses-overview' => 'Licensing Overview'
];
?>

<h1>Licensing Overview (id Tech Engines)</h1>

<div class="section">
    <h2>Engine License Summary</h2>
    <ul>
        <li><strong>Doom Engine:</strong> Released GPL (1997).</li>
        <li><strong>id Tech 1 (Quake):</strong> GPL (1999).</li>
        <li><strong>id Tech 2 (Quake II):</strong> GPL (2001).</li>
        <li><strong>id Tech 3 (Quake III Arena):</strong> GPLv2 (2005).</li>
        <li><strong>id Tech 4 (Doom 3):</strong> GPL (2011 as Doom 3 GPL).</li>
        <li><strong>id Tech 5+:</strong> Proprietary/internal; not open sourced.</li>
    </ul>
</div>

<div class="section">
    <h2>GPL Obligations (High-Level)</h2>
    <ul>
        <li>Copyleft: Derivative works must be GPL when distributed.</li>
        <li>Source: If you ship binaries, you must provide corresponding source or a written offer.</li>
        <li>Notices: Preserve copyright/license notices and include the GPL text.</li>
        <li>Version: Follow the specific GPL version the code is under (id Tech 3: GPLv2).</li>
    </ul>
</div>

<div class="section">
    <h2>GPLv2 vs GPLv3 (Quick)</h2>
    <ul>
        <li><strong>GPLv2:</strong> Copyleft, fewer patent/DRM clauses, no explicit anti-Tivoization.</li>
        <li><strong>GPLv3:</strong> Adds anti-Tivoization, clearer patent terms, DRM/anti-circumvention language; not automatically compatible with GPLv2-only code.</li>
    </ul>
</div>

<div class="section">
    <h2>When You Distribute</h2>
    <ul>
        <li>Bundle source (or provide a valid written offer) with binaries.</li>
        <li>Keep GPL headers and LICENSE files intact.</li>
        <li>Document any third-party code and its licenses; avoid incompatible license mixing.</li>
    </ul>
</div>

<div class="section">
    <h2>Common Pitfalls</h2>
    <ul>
        <li>Mixing GPLv2-only with GPLv3-only without dual-licensing.</li>
        <li>Forgetting to ship source alongside binary releases.</li>
        <li>Removing or omitting copyright/license notices.</li>
    </ul>
</div>

<div class="section">
    <h2>Resources</h2>
    <ul>
        <li><a href="https://www.gnu.org/licenses/old-licenses/gpl-2.0.html">GPLv2 text</a></li>
        <li><a href="https://www.gnu.org/licenses/gpl-3.0.html">GPLv3 text</a></li>
        <li><a href="https://www.gnu.org/licenses/gpl-faq.html">GNU GPL FAQ</a></li>
    </ul>
</div>

