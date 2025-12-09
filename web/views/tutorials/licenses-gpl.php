<?php
/**
 * Licensing Overview: GPLv2 vs GPLv3
 */
$title = 'Licensing Overview: GPLv2 vs GPLv3 - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/licenses-gpl' => 'Licensing Overview: GPLv2 vs GPLv3'
];
?>

<h1>Licensing Overview: GPLv2 vs GPLv3</h1>

<div class="section">
    <h2>Why It Matters</h2>
    <p>id Tech 3 and several id engines have been released under the GNU General Public License. Understanding GPL terms helps you comply when modifying or redistributing source and binaries.</p>
</div>

<div class="section">
    <h2>GPLv2 (used by id Tech 3)</h2>
    <ul>
        <li><strong>Copyleft:</strong> Derivative works must also be released under GPLv2 when distributed.</li>
        <li><strong>Source availability:</strong> If you distribute binaries, you must provide corresponding source.</li>
        <li><strong>Notable:</strong> No explicit anti-Tivoization clause; patent language is minimal compared to GPLv3.</li>
        <li><strong>Compatibility:</strong> GPLv2-only code is not automatically compatible with GPLv3 code.</li>
    </ul>
</div>

<div class="section">
    <h2>GPLv3</h2>
    <ul>
        <li><strong>Copyleft (broader):</strong> Similar sharing requirements, with stronger protections against additional restrictions.</li>
        <li><strong>Anti-Tivoization:</strong> Prevents use in locked-down hardware that disallows user-modified binaries.</li>
        <li><strong>Patent clauses:</strong> More explicit patent licensing/grant terms to protect users.</li>
        <li><strong>DRM clauses:</strong> Addresses anti-circumvention concerns.</li>
    </ul>
</div>

<div class="section">
    <h2>Key Differences (at a Glance)</h2>
    <ul>
        <li><strong>Hardware lockdown:</strong> GPLv3 restricts “Tivoization”; GPLv2 does not.</li>
        <li><strong>Patent language:</strong> GPLv3 includes explicit patent grants/termination; GPLv2 is less explicit.</li>
        <li><strong>Compatibility:</strong> GPLv2-only is not directly compatible with GPLv3; dual-licensing or GPLv2-or-later can bridge the gap.</li>
    </ul>
</div>

<div class="section">
    <h2>Using GPL Code in Your Project</h2>
    <ul>
        <li>Comply with the license version used by the codebase (id Tech 3: GPLv2).</li>
        <li>When distributing binaries, provide source or a written offer for source.</li>
        <li>Preserve copyright and license notices; include a copy of the GPL.</li>
        <li>If mixing licenses, ensure compatibility (avoid combining GPLv2-only with GPLv3-only without a clear path).</li>
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

