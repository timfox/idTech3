# Asset Validation

## Overview

The asset validation tool (`tools/validate_assets.py`) provides comprehensive validation of game assets in mod directories, checking for missing references, bad paths, invalid metadata, manifest integrity, duplicate assets, and orphaned files.

## Features

### Validation Checks

1. **Missing Asset References**
   - Scans shader files for texture references
   - Scans map files for texture references
   - Reports missing textures, models, sounds

2. **Bad Paths**
   - Checks for paths exceeding MAX_QPATH (256 characters)
   - Detects illegal characters (`..`, `\`, `:`)
   - Validates path format (no absolute paths)

3. **Invalid Files**
   - Detects empty files
   - Validates file readability
   - Checks for corrupted files

4. **Manifest Integrity**
   - Validates `manifest.json` structure
   - Verifies all manifest entries exist
   - Checks file hash integrity

5. **Duplicate Assets**
   - Identifies duplicate files by content hash
   - Reports files with identical content

6. **Orphaned Assets**
   - Finds assets not referenced in manifest
   - Identifies assets not used in shaders/maps

## Usage

### Basic Validation

```bash
# Validate assets in a mod directory
python3 tools/validate_assets.py mymod

# Or use the executable directly
./tools/validate_assets.py mymod
```

### Advanced Options

```bash
# Skip manifest validation
python3 tools/validate_assets.py mymod --no-check-manifest

# Check for orphaned assets
python3 tools/validate_assets.py mymod --check-orphans

# Verbose output
python3 tools/validate_assets.py mymod --verbose

# JSON output (for CI/automation)
python3 tools/validate_assets.py mymod --json
```

### Full Validation

```bash
# Run all checks including manifest and orphaned assets
python3 tools/validate_assets.py mymod --check-manifest --check-orphans --verbose
```

## Output Format

### Standard Output

The tool provides a detailed report with errors categorized by severity:

```
======================================================================
ASSET VALIDATION REPORT
======================================================================

ERRORS (2):
  [ERROR] missing: Shader references missing texture: textures/common/missing (file: shaders/test.shader)
  [ERROR] bad_path: Path too long (257 >= 256): textures/very/long/path/to/texture/that/exceeds/maximum/path/length/texture.tga

WARNINGS (1):
  [WARNING] invalid: Empty file: sounds/missing.wav

INFO (1):
  [INFO] duplicate: Duplicate files found (hash: a1b2c3d4...): textures/duplicate1.tga, textures/duplicate2.tga

======================================================================
Summary: 2 errors, 1 warnings, 1 info messages
======================================================================
```

### JSON Output

For CI/CD integration, use `--json` flag:

```json
{
  "success": false,
  "errors": [
    {
      "severity": "error",
      "category": "missing",
      "message": "Shader references missing texture: textures/common/missing",
      "file": "shaders/test.shader",
      "line": null
    }
  ],
  "stats": {
    "total_assets": 150,
    "pk3_files": 3,
    "manifest_assets": 145,
    "references": 200
  }
}
```

## Supported Asset Formats

### Images
- `.tga`, `.jpg`, `.jpeg`, `.png`, `.pcx`, `.bmp`, `.dds`, `.svg`, `.exr`

### Models
- `.md3`, `.ase`, `.obj`

### Sounds
- `.wav`, `.ogg`

### Other
- `.shader` (shader definitions)
- `.map` (map source files)
- `.bsp` (compiled maps)
- `.pk3` (zip archives)

## CI/CD Integration

### GitHub Actions

Add asset validation to your CI workflow:

```yaml
- name: Validate Assets
  run: |
    python3 tools/validate_assets.py mymod --json > validation_results.json
    if [ $? -ne 0 ]; then
      echo "Asset validation failed"
      cat validation_results.json
      exit 1
    fi
```

### Pre-commit Hook

Add to `.git/hooks/pre-commit`:

```bash
#!/bin/bash
# Validate assets before commit
python3 tools/validate_assets.py mymod --check-manifest
if [ $? -ne 0 ]; then
    echo "Asset validation failed. Please fix errors before committing."
    exit 1
fi
```

## Exit Codes

- `0`: Validation passed (no errors)
- `1`: Validation failed (errors found)

## Best Practices

1. **Run validation before commits**: Catch issues early
2. **Use manifest validation**: Ensure manifest.json is up to date
3. **Check for orphans**: Remove unused assets to reduce package size
4. **Fix errors first**: Address errors before warnings
5. **Use JSON output in CI**: Parse results programmatically

## Troubleshooting

### "Mod directory does not exist"
- Ensure you're running from the project root
- Check that the mod directory path is correct

### "Failed to read shader file"
- Check file permissions
- Ensure files are UTF-8 encoded
- Verify file is not corrupted

### "Hash mismatch"
- File may have been modified after manifest was generated
- Regenerate manifest with `tools/update_manifest.py`

### "Path too long"
- Reduce path depth or filename length
- Consider reorganizing directory structure

## Related Tools

- `tools/update_manifest.py`: Generate/update manifest.json
- `tools/package_mod.sh`: Package mod with validated assets
- `tools/package_release.sh`: Create release packages

## See Also

- [Asset Pipeline Documentation](ASSET_PIPELINE.md)
- [Manifest Format Documentation](MANIFEST_FORMAT.md)
- [CI/CD Documentation](cicd.md)
