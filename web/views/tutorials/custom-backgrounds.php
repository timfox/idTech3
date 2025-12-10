<?php
/**
 * Customizing Menu & Console Backgrounds Tutorial
 */
$title = 'Custom Menu and Console Backgrounds - id Tech 3 Modding';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/custom-backgrounds' => 'Custom Menu and Console Backgrounds'
];
?>

<h1>Custom Menu and Console Backgrounds</h1>

<div class="section">
    <h2>Overview</h2>
    <p>This tutorial walks you through replacing the main menu background and the in-game console backdrop in an id Tech 3 mod. We will cover asset prep, shader/menu wiring, and packaging so your mod ships with the new visuals.</p>
    <div class="feature-list">
        <h3>What you will set up</h3>
        <ul>
            <li>A new menu background image loaded from your mod PK3</li>
            <li>A shader definition that points menus to your texture</li>
            <li>A custom console background (conback) and optional tile</li>
            <li>Packaging tips for testing loose files vs. packed PK3</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>A working mod folder (e.g., <code>mymod/</code>) with PK3 loading enabled</li>
        <li>Image editing tool to export <code>.tga</code> or <code>.jpg</code></li>
        <li>Basic familiarity with UI menu files (<code>ui/*.menu</code>) and shader scripts (<code>scripts/*.shader</code>)</li>
    </ul>
</div>

<div class="section">
    <h2>Prepare your images</h2>
    <ol>
        <li>Export your menu background as <code>.tga</code> or <code>.jpg</code>. Use power-of-two dimensions (e.g., <code>2048x1024</code> or <code>1024x512</code>) for best GPU compatibility.</li>
        <li>Export your console backdrop as <code>gfx/2d/conback.tga</code>. A 512x512 or 1024x512 texture works well. Keep alpha simple if you want solid colors; the console usually fades via code.</li>
        <li>(Optional) Export a tile texture for the console as <code>gfx/2d/backtile.tga</code> if you prefer a repeating pattern.</li>
    </ol>
</div>

<div class="section">
    <h2>Wire the main menu background</h2>
    <h3>1) Place the texture</h3>
    <p>Drop your menu image into your mod, for example:</p>
    <div class="code-block"><pre><code>mymod/ui/art/my_menu_bg.tga</code></pre></div>

    <h3>2) Add or update the shader</h3>
    <p>Edit a shader script (create one if needed) under <code>mymod/scripts/ui.shader</code> and point a shader name at your texture:</p>
    <div class="code-block"><pre><code>ui/my_menu_bg
{
    nopicmip
    nomipmap
    {
        clampmap ui/art/my_menu_bg.tga
        blendfunc blend
    }
}</code></pre></div>
    <p>Ensure <code>scripts/ui.shader</code> is listed in <code>mymod/scripts/shaderlist.txt</code> so the renderer loads it.</p>

    <h3>3) Point the menu file to the shader</h3>
    <p>Open the menu definition that controls the main screen (commonly <code>ui/main.menu</code> or similar). Find the background or fullscreen image reference and swap it to your shader:</p>
    <div class="code-block"><pre><code>background "ui/my_menu_bg"</code></pre></div>
    <p>If the menu uses an inline <code>ownerdraw</code> or <code>fullscreen</code> item with <code>background</code> set to an old asset (e.g., <code>menu/art/quake3logo.tga</code>), replace that path with your shader name.</p>

    <h3>4) Test with loose files</h3>
    <p>Place the shader, shaderlist entry, menu edit, and texture as loose files under <code>mymod/</code>. Launch the mod and open the menu; the new background should appear immediately.</p>
</div>

<div class="section">
    <h2>Customize the console background</h2>
    <h3>1) Replace conback</h3>
    <p>Save your console image as <code>gfx/2d/conback.tga</code> and place it in your mod:</p>
    <div class="code-block"><pre><code>mymod/gfx/2d/conback.tga</code></pre></div>
    <p>The engine prefers mod assets over base, so this overrides the default without code changes.</p>

    <h3>2) (Optional) Replace the tile</h3>
    <p>If your build uses a tiled console pattern, also add:</p>
    <div class="code-block"><pre><code>mymod/gfx/2d/backtile.tga</code></pre></div>

    <h3>3) Verify in-game</h3>
    <p>Launch the mod, open the console (<code>~</code>), and confirm the new background. Adjust brightness/contrast in your image if text readability is low.</p>
</div>

<div class="section">
    <h2>Packaging and deployment</h2>
    <ol>
        <li>When satisfied, pack the updated files into a PK3 (zip) placed under <code>mymod/</code>. Keep relative paths identical (e.g., <code>gfx/2d/conback.tga</code>, <code>ui/art/my_menu_bg.tga</code>, <code>scripts/ui.shader</code>, updated <code>ui/*.menu</code>, and <code>scripts/shaderlist.txt</code>).</li>
        <li>Restart the game with the PK3 to ensure load order is correct.</li>
        <li>If you ship multiple variants, use distinct shader names and menu references to avoid conflicts.</li>
    </ol>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li><strong>Menu shows old background:</strong> Confirm the shader file is in <code>shaderlist.txt</code> and the menu file references the shader name, not a bare texture path.</li>
        <li><strong>Pink/black checkerboard:</strong> Path mismatch. Verify case-sensitive paths and that the image is inside the PK3 (or loose file) under the exact referenced path.</li>
        <li><strong>Console still default:</strong> Check that <code>gfx/2d/conback.tga</code> exists in your mod PK3 and no higher-priority PK3 overrides it.</li>
        <li><strong>Text hard to read:</strong> Darken or blur the background image and retest; the console overlay does not guarantee heavy dimming.</li>
    </ul>
</div>

<div class="section">
    <h2>Quick reference</h2>
    <ul>
        <li>Menu background shader: <code>scripts/ui.shader</code> &amp; <code>scripts/shaderlist.txt</code></li>
        <li>Menu texture path: <code>ui/art/&lt;your_image&gt;.tga</code></li>
        <li>Menu file hook: <code>ui/main.menu</code> (or equivalent) <code>background "ui/your_shader"</code></li>
        <li>Console background: <code>gfx/2d/conback.tga</code></li>
        <li>Console tile (optional): <code>gfx/2d/backtile.tga</code></li>
    </ul>
</div>
