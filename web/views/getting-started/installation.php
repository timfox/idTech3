<?php
/**
 * Installation guide view
 */
$title = 'Installation - id Tech 3 Documentation';
$breadcrumbs = [
    '/getting-started' => 'Getting Started',
    '/getting-started/installation' => 'Installation'
];
?>

<h1>Installation Guide</h1>

<div class="section">
    <h2>System Requirements</h2>
    <ul class="requirements-list">
        <li>
            <strong>Operating System:</strong>
            <ul>
                <li>Windows 7/10/11</li>
                <li>Linux (Modern distributions)</li>
                <li>macOS 10.15+</li>
            </ul>
        </li>
        <li>
            <strong>Hardware:</strong>
            <ul>
                <li>CPU: 2.0 GHz dual-core processor or better</li>
                <li>RAM: 4GB minimum, 8GB recommended</li>
                <li>GPU: OpenGL 3.3 or Vulkan 1.1 capable graphics card</li>
                <li>Storage: 1GB available space</li>
            </ul>
        </li>
    </ul>
</div>

<div class="section">
    <h2>Building from Source</h2>
    
    <?php include __DIR__ . '/../partials/build-instructions.php'; ?>
</div>

<div class="section">
    <h2>Dependencies</h2>
    <?php include __DIR__ . '/../partials/dependencies-list.php'; ?>
</div> 