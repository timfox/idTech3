## How to Make Shader Materials "Glint" (Vulkan PBR Glint Quick Guide)

This guide explains exactly what you need to set on your material or shader to enable visible *glint* (microfacet sparkle) in the Vulkan renderer. If you’re just looking to make a PBR surface sparkle—start here.

---

### 1. Enable Vulkan PBR Glint Support

First, make sure the engine is running the Vulkan renderer with the glint system enabled:

- Launch your game or engine with `+set r_renderer vulkan`
- Ensure the main glint system cvar is enabled:  
  ```
  \set r_glints 1
  ```
- (Optional) You can use the default glint algorithm or try `r_glints_mode 2` for full Chermain 2020 microfacet glints.

---

### 2. Add Glint to Your Material

To make your material **actually glint** in-game:

#### A. Set the following material keywords (in your shader/material file):

Depending on your material system (shader stages, .mtr files, or GLSL tagging), you typically need:

- A PBR surface (lit with at least a specular or metallic/roughness workflow)
- No explicit "glint" map needed by default; instead:

**For idTech3-style .shader/material scripts:**
```
q3map_glint 1        // Enables glinting for this surface
```
Or for newer PBR systems:
```
glint 1
```
*(Use whichever keyword is recognized by your engine's material parser. For custom engines, check docs or code for "glint" flags.)*

#### B. Control the strength and look:

- The *amount* of glint effect is often driven by:
  - **Roughness/Metalness maps:** Lower roughness, higher metallic = more visible glint
  - **Uniforms/flags:** Some engines support a direct `glint_strength` or similar property on materials/shaders
- For fine control (advanced), look for:
  - `glint_strength`
  - `glint_lobeSigma`
  - `glint_alpha`

Example snippet (GLSL uniform or material definition):
```glsl
glint_strength 0.7
glint_lobeSigma 0.14
glint_alpha 0.18
```

*(Not all material pipelines expose all of these—use what’s available in your definition system.)*

---

### 3. Debug/Visualize Glints

- Use `\set r_glints_debug 1` to visualize where the glint is being applied in the scene.
- You can also adjust `r_glints_strength` or similar cvars live in the console and see changes on reload (`\vid_restart`).

---

### 4. Summary Table

| What to set           | Where                                 | Example/Default                   |
|-----------------------|---------------------------------------|-----------------------------------|
| `r_glints 1`          | Console cvar or config                | `\set r_glints 1`                 |
| `q3map_glint 1`       | .shader material file (idTech3 style) | `q3map_glint 1`                   |
| `glint 1`             | PBR material file (modern style)      | `glint 1`                         |
| `glint_strength ...`  | Material property or cvar             | `glint_strength 0.7`              |
| `r_glints_debug 1`    | Console cvar (for debug view)         | `\set r_glints_debug 1`           |

---

### 5. Minimal Example for Material Definition

In a Quake 3-style `.shader`:
```
textures/myfolder/my_glinty_material
{
    q3map_glint 1
    // ... other PBR settings ...
}
```

Or in a custom PBR material config:
```
glint 1
glint_strength 0.5
// metalness, roughness, maps as usual
```

---

**TL;DR:**  
Set `glint 1` (or `q3map_glint 1`) on your material, ensure `r_glints 1` in the console, and tweak your roughness/metalness for the best visual sparkle.

If you’re authoring GLSL directly: ensure the material’s glint flag/uniform is set to a positive value and uses the glint-enabled shader variants.

For more advanced control over the effect (dictionary shape, sampling quality), adjust the `r_glints_*` console variables and experiment with material-side parameters as described above.

