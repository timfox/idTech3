# idTech3 Modding Guide

## Creating a Mod

### Quick Start

Use the mod creation script:

```bash
./tools/create_mod.sh mymod
```

This creates a mod structure in `mods/mymod/` with:
- `scripts/` - Shader files
- `maps/` - BSP map files
- `textures/` - Texture files
- `sounds/` - Sound files
- `models/` - Model files

### Manual Mod Creation

1. Create a directory for your mod:
   ```bash
   mkdir -p mods/mymod/{scripts,maps,textures,sounds,models}
   ```

2. Add your content files to the appropriate directories

3. Test your mod:
   ```bash
   ./idtech3_launcher +set fs_game mymod
   ```

## Mod Structure

### Directory Layout

```
mods/mymod/
├── scripts/           # Shader files (.shader)
├── maps/              # BSP map files (.bsp)
├── textures/          # Texture files (.tga, .jpg)
│   └── mymod/         # Organize textures in subdirectories
├── sounds/            # Sound files (.wav)
├── models/            # Model files (.md3, .md2)
├── vm/                # Virtual machine files (optional)
│   ├── game.qvm       # Game module
│   ├── cgame.qvm      # Client game module
│   └── ui.qvm         # UI module
└── modinfo.txt        # Mod information (optional)
```

## Content Types

### Shaders

Shaders define material properties for surfaces. Place `.shader` files in `scripts/`:

```glsl
// Example shader
textures/mymod/test
{
    {
        map textures/mymod/test_diffuse
        rgbGen identity
    }
    {
        map $lightmap
        blendFunc filter
        rgbGen identity
    }
}
```

### Maps

Compiled BSP map files go in `maps/`. Use a BSP compiler (like q3map2) to compile `.map` files into `.bsp` files.

### Textures

Texture files (`.tga`, `.jpg`) go in `textures/`. Organize them in subdirectories:

```
textures/
└── mymod/
    ├── test_diffuse.tga
    ├── test_normal.tga
    └── test_specular.tga
```

### Sounds

Sound files (`.wav`) go in `sounds/`. Use 44.1kHz, 16-bit, mono or stereo WAV files.

### Models

Model files (`.md3`, `.md2`) go in `models/`. These are compiled model formats.

## Packaging Mods

### As Directory

Keep your mod as a directory structure in `mods/`:

```bash
mods/mymod/
```

### As PK3 File

Package your mod into a `.pk3` file (which is just a ZIP file):

```bash
cd mods/mymod
zip -r ../mymod.pk3 .
```

Then place `mymod.pk3` in the `base/` directory.

Or use the packaging script:

```bash
./tools/package_content.sh mods/mymod release/base/mymod.pk3
```

## Testing Mods

### Using the Launcher

```bash
./idtech3_launcher +set fs_game mymod
```

### Direct Command Line

```bash
./idtech3.x86_64 +set fs_game mymod +map mymap
```

### Developer Mode

```bash
./idtech3.x86_64 +set fs_game mymod +devmap mymap
```

Developer mode provides additional debugging features and console access.

## Asset Pipeline

### Validating Assets

Use the validation script to check your mod assets:

```bash
./tools/validate_assets.sh mods/mymod
```

This checks:
- BSP file integrity
- PK3 file validity (if using pak files)
- Asset file presence

### Packaging Content

Package your mod content into a pk3 file:

```bash
./tools/package_content.sh mods/mymod release/base/mymod.pk3
```

## Mod Compatibility

### VM Modules

If your mod includes VM modules (`.qvm` files), ensure they are compatible with the engine version:

- Place VM files in `vm/` directory
- Ensure VM files are compiled for the correct architecture
- Test VM modules thoroughly before distribution

### Content-Only Mods

Content-only mods (no VM files) are generally more compatible:

- Use standard asset formats
- Follow naming conventions
- Test on multiple engine versions if possible

## Best Practices

1. **Organize your files**: Use subdirectories for textures, sounds, etc.
2. **Name your assets uniquely**: Prefix textures/models with your mod name
3. **Test thoroughly**: Test your mod with different renderers and settings
4. **Document your mod**: Include a README with installation instructions
5. **Validate assets**: Use the validation tools before distributing

## Troubleshooting

### Mod Not Loading

- Check that the mod directory exists in `mods/` or `base/`
- Verify the mod name matches the directory name
- Check console output for error messages

### Assets Not Found

- Ensure files are in the correct directories
- Check file names match shader/material references
- Verify file extensions are correct

### Crashes When Loading Mod

- Check `crash_report.txt` for details
- Validate mod assets using `validate_assets.sh`
- Test with a minimal mod first, then add content incrementally

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for more help.
