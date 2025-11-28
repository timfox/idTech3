<div class="graphics-settings">
    <h4>Display Settings</h4>
    <table>
        <thead>
            <tr>
                <th>Setting</th>
                <th>Variable</th>
                <th>Values</th>
                <th>Description</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Screen Resolution</strong></td>
                <td><code>r_mode</code></td>
                <td>0-12, -1, -2</td>
                <td>Display resolution mode (-1 for custom, -2 for desktop)</td>
            </tr>
            <tr>
                <td><strong>Fullscreen</strong></td>
                <td><code>r_fullscreen</code></td>
                <td>0, 1</td>
                <td>Enable fullscreen mode</td>
            </tr>
            <tr>
                <td><strong>Gamma</strong></td>
                <td><code>r_gamma</code></td>
                <td>0.5-3.0</td>
                <td>Screen brightness/gamma correction</td>
            </tr>
            <tr>
                <td><strong>Intensity</strong></td>
                <td><code>r_intensity</code></td>
                <td>1.0-1.5</td>
                <td>Overall brightness multiplier</td>
            </tr>
        </tbody>
    </table>

    <h4>Renderer Settings</h4>
    <table>
        <thead>
            <tr>
                <th>Setting</th>
                <th>Variable</th>
                <th>Values</th>
                <th>Description</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Renderer</strong></td>
                <td><code>r_renderer</code></td>
                <td>opengl, vulkan</td>
                <td>Graphics API backend</td>
            </tr>
            <tr>
                <td><strong>PBR Rendering</strong></td>
                <td><code>r_pbr</code></td>
                <td>0, 1</td>
                <td>Enable physically based rendering</td>
            </tr>
            <tr>
                <td><strong>HDR</strong></td>
                <td><code>r_hdr</code></td>
                <td>0, 1</td>
                <td>High dynamic range rendering</td>
            </tr>
            <tr>
                <td><strong>Bloom</strong></td>
                <td><code>r_bloom</code></td>
                <td>0, 1</td>
                <td>Bloom/glow effect</td>
            </tr>
            <tr>
                <td><strong>SSAO</strong></td>
                <td><code>r_ssao</code></td>
                <td>0, 1</td>
                <td>Screen space ambient occlusion</td>
            </tr>
        </tbody>
    </table>

    <h4>Quality Settings</h4>
    <table>
        <thead>
            <tr>
                <th>Setting</th>
                <th>Variable</th>
                <th>Values</th>
                <th>Description</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Texture Quality</strong></td>
                <td><code>r_picmip</code></td>
                <td>0-3</td>
                <td>Texture resolution (0=highest, 3=lowest)</td>
            </tr>
            <tr>
                <td><strong>Texture Filter</strong></td>
                <td><code>r_texturemode</code></td>
                <td>GL_NEAREST, GL_LINEAR, etc.</td>
                <td>Texture filtering method</td>
            </tr>
            <tr>
                <td><strong>Detail Textures</strong></td>
                <td><code>r_detailtextures</code></td>
                <td>0, 1</td>
                <td>Enable detail texture overlays</td>
            </tr>
            <tr>
                <td><strong>Dynamic Lighting</strong></td>
                <td><code>r_dynamiclight</code></td>
                <td>0, 1</td>
                <td>Enable dynamic lighting effects</td>
            </tr>
            <tr>
                <td><strong>Shadows</strong></td>
                <td><code>r_shadows</code></td>
                <td>0, 1, 2</td>
                <td>Shadow quality (0=off, 1=basic, 2=advanced)</td>
            </tr>
        </tbody>
    </table>

    <blockquote>
        <strong>Performance Tip:</strong> For optimal performance, start with medium settings and adjust individual options based on your hardware capabilities.
    </blockquote>
</div> 