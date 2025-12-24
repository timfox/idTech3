# NetRadiant Features Implementation Plan

## Overview
This document outlines the implementation plan for porting NetRadiant-inspired features into the Qt6 Radiant editor.

## Feature Priority & Status

### 1. Unhardcoded Engine Path in Gamepacks ⚠️ IN PROGRESS
**Status**: Starting implementation
**Files to modify**:
- `radiant/qt/qt_env.cpp` - Path expansion utility
- `radiant/qt/path_expander.h` - NEW: Path expansion class
- `radiant/qt/path_expander.cpp` - NEW: Implementation
- Gamepack loading code (TBD: locate gamepack parser)

**Implementation steps**:
1. Create `PathExpander` utility class supporting:
   - `%VAR%` (Windows-style) and `${VAR}` (Unix-style) expansion
   - `~` home directory expansion
   - XDG environment variables (`${XDG_CONFIG_HOME}`, `${XDG_DATA_HOME}`, `${XDG_CACHE_HOME}`)
   - Windows special paths (`%ProgramFiles%`, `%AppData%`, etc.)
   - Fallback handling (leave token unchanged + warn if var missing)
   - Path normalization (resolve `..`, normalize slashes)
2. Integrate into `qt_env.cpp` for engine/game path resolution
3. Update gamepack schema/docs with examples
4. Add unit tests for path expansion

**Dependencies**: None
**Estimated effort**: 2-3 hours

---

### 2. User Path Change (Windows AppData / Cross-platform Config Dirs) ⏳ PENDING
**Status**: Not started
**Files to modify**:
- `radiant/qt/qt_env.cpp` - Change `tempPath` to use XDG/AppData
- `radiant/qt/config_migration.cpp` - NEW: Migration logic
- `radiant/qt/config_migration.h` - NEW: Migration header
- `radiant/qt/main_qt.cpp` - Add `--no-migrate` flag

**Implementation steps**:
1. Update `InitQtRadiantEnv()` to use:
   - Windows: `%AppData%/QtRadiant` (Roaming)
   - Linux: `$XDG_CONFIG_HOME/qtradiant` (default: `~/.config/qtradiant`)
   - macOS: `~/Library/Application Support/QtRadiant`
2. Implement migration from `~/.radiant/`:
   - Detect old directory
   - Copy/move files (timestamp/size heuristic to avoid overwriting newer)
   - Log migration summary + failures
3. Add `--no-migrate` CLI flag
4. Ensure separate folder names to avoid conflicts with other forks

**Dependencies**: Feature 1 (path expansion)
**Estimated effort**: 3-4 hours

---

### 3. Modifiable Camera Field of View (FOV) ⏳ PENDING
**Status**: UI exists, not connected
**Files to modify**:
- `radiant/qt/vk_viewport.cpp` - Replace hardcoded 75.0f with preference value
- `radiant/qt/preferences_dialog.cpp` - Already has FOV spinbox (line 113-116)
- `radiant/qt/preferences_dialog.h` - Already has `m_fovSpin` member

**Implementation steps**:
1. Load FOV from QSettings in `VkViewportWidget` constructor
2. Replace hardcoded `75.0f` values (lines 368, 1260, 1783) with loaded value
3. Connect preference change signal to viewport update
4. Ensure correct behavior with mouse-look/fly mode
5. Add validation (60-120 range, or justify different range)

**Dependencies**: None
**Estimated effort**: 1 hour

---

### 4. DDS Prefix Support (Editor Asset Lookup) ⏳ PENDING
**Status**: Not started
**Files to modify**:
- Texture loading code (TBD: locate texture loader)
- Gamepack config - Add DDS prefix option
- `radiant/qt/texture_extractor.cpp` - May need updates

**Implementation steps**:
1. Locate texture/material file resolution code
2. Implement fallback: `textures/foo/bar.tga` → `dds/textures/foo/bar.dds`
3. Make configurable per-gamepack
4. Add diagnostics (log once per session when DDS fallback resolves)
5. Ensure non-destructive (don't change canonical shader path)

**Dependencies**: Feature 1 (gamepack config)
**Estimated effort**: 2-3 hours

---

### 5. q3map2: IQM Model Baking ⏳ PENDING
**Status**: Not started
**Files to modify**:
- `radiant/tools/quake3/q3map2/model.c` - Add IQM parsing
- `radiant/tools/quake3/q3map2/q3map2.h` - Add IQM-related structures
- `radiant/tools/quake3/q3map2/bsp.c` - Bake geometry into BSP
- `radiant/tools/quake3/q3map2/light.c` - Lightmap baked geometry

**Implementation steps**:
1. Research IQM format specification
2. Add IQM parser to `model.c`
3. Implement geometry baking into BSP at compile time
4. Add lightmapping support for baked geometry
5. Maintain `misc_anim_model` fallback (engine-rendered)
6. Add CLI switches: `--bake-iqm`, `--no-bake-iqm`
7. Create test map + IQM fixture in `/testdata/`
8. Add CI script to compile test map

**Dependencies**: None (standalone tool)
**Estimated effort**: 8-12 hours (complex)

---

### 6. Clickable Linux Binary / Packaging Sanity ⏳ PENDING
**Status**: Not started
**Files to modify**:
- `radiant/CMakeLists.txt` - Ensure correct ELF type
- `tools/verify_binary.sh` - NEW: Verification script
- `docs/BUILD.md` or `docs/TOOLS.md` - Packaging notes

**Implementation steps**:
1. Verify CMake produces `ET_EXEC` or PIE, not `ET_DYN` mis-detected
2. Ensure executable bit is set
3. Create/verify `.desktop` file + icon (if project packages one)
4. Create `verify_binary.sh` script checking:
   - `file <binary>` contains "executable"
   - Runtime Qt plugin paths are correct
5. Add packaging notes for AppImage/Flatpak/zip

**Dependencies**: None
**Estimated effort**: 1-2 hours

---

## System Locations Found

### Gamepack Loading
- **Location**: TBD (need to search for `.game` file parser)
- **References**: `radiant/radiant/gtkdlgs.cpp` mentions `.game` files
- **Action**: Search for gamepack loading code

### Preferences/Config
- **Qt**: `radiant/qt/preferences_dialog.cpp` (QSettings-based)
- **GTK**: `radiant/radiant/preferences.cpp` (XML-based)
- **User dir**: `radiant/qt/qt_env.cpp` (currently `~/.radiant/`)

### Camera/Viewport
- **Viewport**: `radiant/qt/vk_viewport.cpp`
- **FOV**: Hardcoded `75.0f` at lines 368, 1260, 1783
- **Preferences**: `radiant/qt/preferences_dialog.cpp` (FOV spinbox exists)

### Texture Loading
- **Location**: TBD (need to search)
- **References**: `radiant/qt/texture_extractor.cpp` exists
- **Action**: Search for texture file resolution code

### q3map2
- **Location**: `radiant/tools/quake3/q3map2/`
- **Model handling**: `model.c` exists
- **BSP writing**: `bsp.c`, `writebsp.c` exist

---

## Implementation Order

1. ✅ **Feature 1** (Path Expansion) - Starting now
2. **Feature 3** (FOV) - Quick win, UI already exists
3. **Feature 2** (User Paths) - Depends on Feature 1
4. **Feature 4** (DDS Prefix) - Depends on Feature 1
5. **Feature 6** (Packaging) - Independent, can do anytime
6. **Feature 5** (IQM Baking) - Most complex, do last

---

## Notes

- **Wayland Safety**: Ensure Qt surfaces/resources are destroyed properly to avoid proxy leaks
- **Cross-platform**: All features must work on Linux/Windows/macOS unless explicitly OS-specific
- **Documentation**: Each feature needs docs in `/docs/` or nearest existing docs location
- **Testing**: Add small regression tests or validation steps for each feature
