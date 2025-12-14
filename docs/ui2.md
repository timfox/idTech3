# UI2 - Deterministic UI/Layout System

## Overview

UI2 is a deterministic UI/layout system inspired by CSS box model and flexbox, designed specifically for the Quake3e engine. It provides a runtime for rendering HUD/menu/editor overlays using the existing Quake3e renderer without external GUI libraries.

## Key Features

- **Deterministic layout** across platforms (no dependency on system fonts for tests)
- **CSS-like styling** with a limited property set
- **Flexbox layout** support (row/column)
- **Per-frame arena allocation** (no dynamic allocation during render)
- **C API wrapper** for integration with legacy C code
- **Golden-image testing** ("G-ACID") with hash-based verification

## Architecture

### Core Components

1. **Arena Allocator** (`ui2_arena.cpp`) - Linear allocator for per-frame temporary allocations
2. **C API** (`ui2_public.h`) - C-compatible interface for engine integration
3. **C++ Internal Types** (`ui2_internal.h`) - Core data structures and types
4. **Style Parser** (`ui2_style_parser.cpp`) - CSS-like property parsing (TODO)
5. **Layout Engine** (`ui2_layout.cpp`) - Block and flex layout algorithms (TODO)
6. **Renderer Bridge** (`ui2_render.cpp`) - Draw rects/text via renderer (TODO)
7. **Tests** (`ui2_tests.cpp`) - Golden render tests (TODO)

### Core Types

- **UiNode** - Tree structure with style, computed layout, and children
- **Style** - Specified CSS values
- **ComputedStyle** - Resolved integer values
- **LayoutBox** - Computed position and size (x, y, width, height)
- **UiContext** - Arena, node pool, string table, renderer hooks

## Property Set

### Display
- `display: none | block | flex`

### Flex
- `flex-direction: row | column`
- `justify-content: start | center | end | space-between`
- `align-items: start | center | end | stretch`

### Sizing
- `width/height: auto | px`
- `min-width/min-height: px`

### Spacing
- `padding: px` (shorthand 1/2/4 values)
- `margin: px` (shorthand 1/2/4 values)

### Appearance
- `background-color: rgba`
- `color: rgba`
- `border: px + color` (optional, phase 2)

### Position
- `position: relative | absolute`
- `left/top/right/bottom: px` (only for absolute)

### Overflow
- `overflow: visible | clip`

### Font
- `font: "default"` (no TTF shaping yet)

### Units
- Only `px` (integer) for now

## Usage Example

```c
// Initialize UI2 system
UI2_Init();

// Create context with renderer callbacks
ui2Renderer_t renderer = {
    .SetColor = RE_SetColor,
    .DrawStretchPic = RE_StretchPic,
    .Scissor = RE_Scissor  // optional
};
ui2Context_t *ctx = UI2_CreateContext(&renderer);

// Load stylesheet
const char *css = 
    "root { display:flex; flex-direction:row; padding:8; background-color:#202020ff; }\n"
    "panel { display:block; width:320; margin:8; background-color:#303030ff; }\n"
    "title { display:block; margin:8; color:#ffffffff; }\n";
UI2_LoadStylesheet(ctx, css);

// Build UI tree
UI2_BeginFrame(ctx, 1024, 768);
UI2_BeginNode(ctx, "root", NULL);
    UI2_BeginNode(ctx, "panel", "panelStyleClass");
        UI2_Text(ctx, "title", "Hello");
    UI2_EndNode(ctx);
UI2_EndNode(ctx);
UI2_EndFrame(ctx);  // Computes layout and renders

// Cleanup
UI2_DestroyContext(ctx);
UI2_Shutdown();
```

## Implementation Status

### Step 1: Module Skeleton ✅
- [x] Directory structure created
- [x] Arena allocator implemented
- [x] C API wrapper created
- [x] C++ internal types defined
- [x] CMake build integration
- [x] Basic initialization/shutdown

### Step 2: Style Parser ✅
- [x] CSS tokenizer
- [x] Property parser
- [x] Class/tag selector matching

### Step 3: Layout Engine ✅
- [x] Block layout algorithm
- [x] Flex layout (row/column)
- [x] Absolute positioning
- [x] Overflow clipping

### Step 4: Renderer Bridge ✅
- [x] Solid rectangle drawing
- [x] Text rendering (placeholder)
- [x] Scissor/clip support

### Step 5: Testing ✅
- [x] Golden-image test harness
- [x] Hash computation (FNV-1a)
- [x] Baseline hash storage
- [x] `ui2_test` command

## CVar

- `ui2_enable` - Enable/disable UI2 system (default: 0)

## Integration Points

- **Memory**: Uses `Hunk_AllocateTempMemory` for arena allocation
- **Renderer**: Uses `RE_SetColor` and `RE_StretchPic` for drawing
- **Logging**: Uses `Com_Printf` for debug output
- **CVars**: Uses `Cvar_Get` for configuration

## Testing ("G-ACID")

The golden-image test harness will:
1. Build deterministic test scenes
2. Render to offscreen buffer (512x512)
3. Compute hash (xxHash or SHA1)
4. Compare against baseline hashes
5. Report pass/fail per test

Baseline hashes are stored in `tests/ui2_baselines.txt`.

## Enhanced Features

### Font Rendering
- Integrated with renderer's `TextPaint` callback
- Supports `font-size` property (in px)
- Falls back to placeholder rendering if font not available

### Flex Properties
- `flex-grow`: Controls how much item grows (default: 0)
- `flex-shrink`: Controls how much item shrinks (default: 1)
- `flex-basis`: Base size before growing/shrinking (default: auto)

### Percent Units
- Width and height support `%` units (percentage of parent)
- Example: `width:50%` uses 50% of parent width

### Border Radius
- `border-radius` property with 1/2/4 value shorthand
- Example: `border-radius:8px` or `border-radius:4px 8px`
- Currently renders as regular rectangles (shader implementation TODO)

### Baseline Hash Generation
- Test harness outputs hash values for baseline generation
- Baselines stored in `tests/ui2_baselines.txt`
- Run `ui2_test` command to generate new baselines

## Notes

- No floats, margin collapsing, inline formatting, or complex selectors
- No HTML DOM features
- No dynamic allocation during frame render
- C/C++ boundary is stable (extern "C" API)
- Compiles on Linux/Windows with CMake
- Uses C++23 where supported, doesn't break C targets
- Border-radius rendering is simplified (full rounded corners require shader)
