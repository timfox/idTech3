# idTech3 Project Constitution

## Preamble

This document serves as the **constitutional contract** for the idTech3 engine fork. It defines the project's goals, constraints, and operational rules. All contributors must adhere to these principles. This constitution protects the project's integrity and provides guidance for decision-making.

**Last Updated**: January 18, 2026
**Version**: 1.0

---

## 🎯 PROJECT GOALS

### What "Done" Looks Like

#### Vulkan Renderer (PRIMARY GOAL)
- ✅ **Hardware Acceleration**: Full Vulkan 1.4 + RTX support
- ✅ **Fallback Compatibility**: Automatic OpenGL fallback for all systems
- ✅ **Performance**: 2x+ FPS improvement over OpenGL baseline
- ✅ **Stability**: Zero crashes in production use
- ✅ **Validation**: Clean validation layers, no warnings
- ✅ **CI Coverage**: Automated testing across GCC/Clang, Debug/Release

#### Physically Based Rendering (PBR)
- ✅ **Material System**: Metalness/roughness workflow
- ✅ **Lighting**: Image-based lighting + area lights
- ✅ **Backward Compatibility**: Classic materials still work
- ✅ **Performance**: Minimal overhead when disabled

#### Ray Tracing (RTX)
- 🔶 **Planned**: Vulkan `VK_KHR_ray_tracing_pipeline` scaffolding; extensions probed
- ⏳ **Hardware Support**: NVIDIA RTX / AMD RDNA2+ via Vulkan RT or DXR
- ⏳ **Quality Levels**: Multiple presets (performance/balanced/quality)
- ⏳ **Fallback**: Graceful degradation on non-RT hardware
- ⏳ **Integration**: Hybrid raster + RT (shadows/reflections) or full RT path
- See [docs/RENDERERS_FUTURE.md](docs/RENDERERS_FUTURE.md)

#### Modern C/C++ Standards
- ✅ **C23 Features**: Modern language constructs where beneficial
- ✅ **C++23 Integration**: Strategic C++ usage for complex systems
- ✅ **ABI Stability**: No breaking changes to mod compatibility
- ✅ **Performance**: Zero-cost abstractions

#### Developer Experience
- ✅ **Build System**: One-command builds for all configurations
- ✅ **Testing**: Build-matrix + smoke-test validation (no unified `make test`/`ctest` suite)
- ✅ **Debugging**: Rich debugging tools and error reporting
- ✅ **Documentation**: Complete architectural documentation

### Success Metrics
- **Compatibility**: 100% backward compatibility with Quake III Arena
- **Performance**: Measurable improvements in target use cases
- **Stability**: Zero crashes in CI-validated scenarios
- **Maintainability**: Code remains understandable and extensible

---

## 🏗️ ARCHITECTURAL PRINCIPLES

### Layer Cake Architecture

#### Vanilla Layer (FOUNDATION)
**What**: Pure Id Tech 3 engine - never modified
**Purpose**: Eternal compatibility guarantee
**Rules**:
- Never change existing APIs without 100% backward compatibility
- Original Quake III Arena runs perfectly
- Core engine logic remains untouched
- Historical decisions are preserved

#### Chocolate Layer (ENHANCEMENT)
**What**: Performance and quality improvements
**Purpose**: Better experience with zero breaking changes
**Rules**:
- All enhancements must have fallbacks
- Features can be disabled completely
- Performance improvements are measurable
- Compatibility is never sacrificed

#### Layer Cake (ARCHITECTURE)
**What**: Modern abstractions and systems
**Purpose**: Technical foundation for future development
**Rules**:
- Clean separation of concerns
- Extensible plugin architecture
- Modern C/C++ patterns where beneficial
- API stability for long-term maintenance

### Renderer Boundary (SACRED)
The renderer abstraction layer is **architecturally sensitive**:
- Public interfaces change only when absolutely necessary
- Renderer-specific code stays within renderer modules
- Cross-renderer compatibility maintained
- Fallback chains are robust and tested

---

## 🌿 BRANCH STRATEGY

### Main Branch (`main`)
**Purpose**: Production-ready code
**Rules**:
- Always buildable and testable
- Comprehensive CI validation
- Release candidate quality
- No experimental features

### Development Branches
**Naming**: `feature/<descriptive-name>`
**Rules**:
- Feature-complete before merge
- Comprehensive testing included
- Documentation updated
- CI validation passes

### Layer Branches (Conceptual)
**Vanilla Branches**: `vanilla/*` - Core engine changes
**Chocolate Branches**: `chocolate/*` - Enhancement features
**Layercake Branches**: `layercake/*` - Architectural changes

### Release Branches
**Naming**: `release/v<major>.<minor>`
**Purpose**: Stabilization for releases
**Rules**:
- Only bug fixes and documentation
- Extensive testing and validation
- Release notes prepared

---

## 💻 CODING CONVENTIONS

### Language Standards
- **C**: C23 with modern features (designated initializers, constexpr, etc.)
- **C++**: C++23 for complex systems (RAII, templates, smart pointers)
- **Compatibility**: Code must compile with GCC 15+ and Clang 18+
- **Booleans**: On native builds, **`qboolean` is `bool`** (from `<stdbool.h>`) and **`qtrue` / `qfalse`** expand to **`true` / `false`** (`q_shared.h`). Prefer **`qboolean`** for engine/game APIs and **`qtrue` / `qfalse`** in literals so code stays compatible with the legacy **`Q3_VM`** path, which still uses an enum-backed `qboolean`. Do not reintroduce **`typedef enum { qfalse, qtrue } qboolean`** for native targets.

### Naming Conventions

#### Functions and Variables
```c
// C style (preferred for engine core)
void R_InitRenderer(void);
cvar_t *r_vulkan_validation;

// C++ style (for C++ code)
void MaterialSystem::Initialize();
std::unique_ptr<Renderer> renderer;
```

#### Files and Directories
```
src/
├── client/           # Client-side systems
├── server/           # Server-side systems
├── renderers/        # Renderer implementations
│   ├── vulkan/       # Vulkan renderer
│   └── opengl/       # OpenGL renderer
├── common/           # Shared utilities
└── game/             # Game logic
```

#### Constants and Enums
```c
// C style
#define MAX_QPATH 64
enum {
    ERR_NONE,
    ERR_FATAL,
    ERR_WARNING
};

// C++ style (when using C++)
enum class ErrorCode {
    None,
    Fatal,
    Warning
};
```

### Code Organization

#### File Structure
- **Headers** (.h): Interface declarations, no implementation
- **Implementation** (.c/.cpp): Function definitions
- **One responsibility** per file
- **Include guards** or `#pragma once`

#### Function Organization
```c
/*
===============
FunctionName
===============
*/
static void FunctionName(void) {
    // Implementation
}
```

### Logging Standards

#### Log Levels
```c
// Priority order (most to least important)
Com_Error(ERR_FATAL, "Critical failure");        // Crashes engine
Com_Printf(S_COLOR_RED "Error: %s\n", msg);      // Error conditions
Com_Printf(S_COLOR_YELLOW "Warning: %s\n", msg); // Warning conditions
Com_Printf("Info: %s\n", msg);                   // General information
ri.Printf(PRINT_DEVELOPER, "Debug: %s\n");       // Development only
```

#### Log Categories
- **Startup**: Renderer initialization, feature detection
- **Performance**: FPS, memory usage, bottlenecks
- **Errors**: Clear error messages with recovery suggestions
- **Debug**: Detailed internal state for troubleshooting

### Error Handling

#### Error Recovery Hierarchy
1. **Silent Recovery**: Fix and continue (preferred)
2. **Warning Message**: Continue with degraded functionality
3. **Fallback Mode**: Use alternative implementation
4. **Safe Shutdown**: Clean exit with error report

#### Error Patterns
```c
// Check and recover
if (!resource) {
    Com_Printf(S_COLOR_YELLOW "Warning: Resource failed, using fallback\n");
    UseFallbackResource();
    return qtrue;  // Continue execution
}

// Fatal error - cannot continue
if (!critical_system) {
    Com_Error(ERR_FATAL, "Critical system failed to initialize");
}
```

### Memory Management

#### Allocation Patterns
- **Pool Allocators**: For frequently allocated objects
- **Arena Allocators**: For level/map data
- **Stack Allocation**: For temporary buffers
- **RAII**: In C++ code where appropriate

#### Memory Safety
- **Bounds Checking**: All array accesses validated
- **Null Checks**: Pointer validation before use
- **Leak Detection**: Automated testing with Valgrind
- **Corruption Detection**: Memory integrity validation

---

## 🔨 BUILD TRUTH

### Canonical Build Process

#### Linux (Primary Development Platform)
```bash
# Prerequisites
sudo apt-get install cmake clang-18 gcc-15 ninja-build

# Vulkan build (recommended)
./scripts/compile_engine.sh vulkan

# OpenGL build (fallback)
./scripts/compile_engine.sh opengl

# Debug build
./scripts/compile_engine.sh vulkan debug

# Clean build
./scripts/compile_engine.sh clean vulkan

# Optional link-time optimization (Release/RelWithDebInfo; GCC/Clang; longer links)
./scripts/compile_engine.sh vulkan lto
# Equivalent: cmake -DENABLE_LTO=ON ... (see CMakeLists.txt; MSVC not wired for this option)
```

#### Windows (Cross-Platform Validation)
```bash
# Using MSYS2
./scripts/compile_engine.sh vulkan

# Visual Studio (alternative)
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

### Build System Requirements

#### CMake Configuration
- **Minimum Version**: 3.20
- **Generator**: Ninja (preferred) or Make
- **Build Types**: Debug, Release, RelWithDebInfo
- **Cross-Compilation**: Supported via toolchain files

#### Compiler Requirements
- **GCC**: 15.0+ with C23/C++23 support
- **Clang**: 18.0+ with modern optimizations
- **MSVC**: 2022+ (Windows cross-compilation)

#### Dependency Management
- **System Packages**: SDL2, Vulkan SDK, OpenEXR, etc.
- **Build Dependencies**: Automatically detected
- **Optional Features**: Gracefully disabled if dependencies missing

### Build Validation

#### CI Requirements
- **Matrix Builds**: GCC + Clang, Debug + Release
- **Warning Policy**: Errors for new/changed code only
- **Test Execution**: Smoke tests and basic validation
- **Artifact Upload**: Binaries and logs for verification

#### Local Development
- **Incremental Builds**: Fast rebuilds for development
- **Clean Builds**: Full rebuilds for validation
- **Cross-Compilation**: Test Windows builds on Linux

---

## 🚫 NON-GOALS

### Things We Will NOT Do

#### Architecture
- **No Engine-Wide C++ Rewrite**: Strategic C++ usage, not conversion
- **No Breaking Changes**: Existing mods always work
- **No Monolithic Architecture**: Modular design preserved
- **No Vendor Lock-in**: Multi-vendor GPU support maintained

#### Compatibility
- **No API Breaking**: Public interfaces remain stable
- **No File Format Changes**: Existing assets always load
- **No Network Protocol Changes**: Compatible with existing servers
- **No License Changes**: GPL compatibility maintained

#### Scope
- **No Game Development**: Engine only, no built-in games
- **No Platform Exclusivity**: Linux-first but cross-platform
- **No Feature Bloat**: Each feature must prove its value
- **No Experimental Dependencies**: Only stable, well-supported libraries

#### Development Process
- **No Big Bang Rewrites**: Incremental, testable changes
- **No Unreviewed Code**: All changes go through review
- **No Undocumented Features**: Every feature documented
- **No Unmaintained Code**: Code must be maintainable long-term

### Why These Constraints?

#### Stability First
The engine serves **production games** that cannot break. Compatibility and stability take precedence over shiny new features.

#### Long-Term Maintenance
This engine will be maintained for **decades**. Decisions must consider 10-year maintainability, not 6-month convenience.

#### Ecosystem Health
Breaking changes harm the **entire modding ecosystem**. We protect the community by maintaining compatibility.

#### Focus Preservation
Limited scope prevents **feature creep** and maintains focus on core engine excellence.

---

## 📋 DEVELOPMENT WORKFLOW

### Code Contribution Process

#### 1. Planning
- Consult this constitution for architectural decisions
- Choose appropriate layer (vanilla/chocolate/layercake)
- Plan incremental implementation

#### 2. Implementation
- Follow coding conventions
- Include comprehensive error handling
- Add logging for debugging
- Write tests for new functionality

#### 3. Validation
- Build with all supported compilers
- Run smoke tests and validation scripts
- Validate no regressions
- Update documentation

#### 4. Review
- Clear commit messages
- Documentation updated
- CI validation passes
- Architectural compliance verified

### Quality Gates

#### Code Review Checklist
- [ ] Architectural compliance (layer assignment correct)
- [ ] Coding conventions followed
- [ ] Error handling comprehensive
- [ ] Logging appropriate
- [ ] Tests included
- [ ] Documentation updated

#### CI Validation
- [ ] All compilers pass
- [ ] No new warnings
- [ ] Smoke/validation scripts pass
- [ ] Smoke tests validate functionality

#### Release Criteria
- [ ] All quality gates pass
- [ ] Documentation complete
- [ ] Backward compatibility verified
- [ ] Performance impact assessed

---

## 🎯 DECISION FRAMEWORK

### How to Decide What to Implement

#### Questions to Ask
1. **Which Layer?** Does this fit vanilla/chocolate/layercake?
2. **Compatibility?** Will existing mods break?
3. **Maintainability?** Can this be maintained for 10+ years?
4. **Value?** Does this provide clear benefit to users?
5. **Scope?** Is this within our defined goals?

#### Architectural Review
All significant changes require architectural review:
- Impact on renderer boundary
- Compatibility implications
- Maintenance burden assessment
- Performance impact analysis

#### When to Say No
- **Breaking Changes**: If it breaks existing mods
- **Maintenance Burden**: If it creates unsustainable complexity
- **Scope Creep**: If it doesn't align with core goals
- **Risk**: If it introduces instability without clear benefit

---

## 📚 REFERENCE

### Key Documents
- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)**: High-level architectural overview
- **[RENDERERS.md](docs/RENDERERS.md)**: Renderer architecture and features
- **[FORWARD_PLUS_PIPELINE_AUDIT.md](docs/FORWARD_PLUS_PIPELINE_AUDIT.md)**: Vulkan Forward+ scaffolding (SSBOs, compute cull, PBR fragment path, Tier A hooks)
- **[BRANCHES.md](docs/BRANCHES.md)**: Development philosophy
- **[ROADMAP.md](docs/ROADMAP.md)**: Development priorities
- **[DEVELOPMENT_SETUP.md](docs/DEVELOPMENT_SETUP.md)**: Development environment
- **[QUICKSTART.md](docs/QUICKSTART.md)**: End-user quick start (download, game data, run)
- **[MINIMAL_GAME_SHELL.md](docs/MINIMAL_GAME_SHELL.md)**: Smallest valid `base/` + `.pk3` bootstrap (engine-only / tech demo)
- **[RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md)**: Pre-release validation and release steps

### Build Scripts
- `./scripts/compile_engine.sh` - Primary build script
- `./scripts/smoke_test.sh` - Basic validation
- `./scripts/validate_ci_build.sh` - Full CI validation

### Quality Tools
- `./scripts/run_clang_tidy.sh` - Code quality analysis
- `./scripts/run_cppcheck.sh` - Static analysis
- `./scripts/smoke_test.sh` - Runtime smoke test checks

---

## AMENDMENT PROCESS

This constitution can be amended through:
1. **Community Consensus**: Discussion and agreement
2. **Architectural Review**: Technical validation
3. **Documentation Update**: Clear record of changes
4. **Version Bumping**: Constitution version incremented

**Rationale**: Project constraints evolve, but changes must be deliberate and well-considered.

---

## USER RULES (Personal Defaults)

These are my personal rules that apply to **all changes I make**, regardless of project. They prevent "AI drift" and keep modifications focused and safe.

### Core Principles
- **Never change gameplay behavior unless explicitly requested**
- **Keep C ABI stable; wrap C++ in `#ifdef __cplusplus` guards only**
- **No new dependencies without explicit justification and approval**
- **Every new feature must include a toggle and startup log line**
- **Prefer small, reviewable commits over large refactoring**

### Implementation Rules
- **Preserve existing APIs and interfaces**
- **Add logging for all new code paths**
- **Include error handling for edge cases**
- **Test changes before committing**
- **Document non-obvious design decisions**

### Quality Standards
- **No code without tests (where applicable)**
- **No features without documentation**
- **No changes without backward compatibility**
- **No complexity without justification**
- **No "cleanup" commits without clear benefit**

### Communication Rules
- **Explain reasoning for non-obvious changes**
- **Ask before making architectural changes**
- **Flag when I'm unsure about approach**
- **Provide context for design decisions**
- **Be explicit about tradeoffs and alternatives**

### Boundaries
- **Don't rewrite working code "because it's ugly"**
- **Don't add features "because they're cool"**
- **Don't change patterns "to be more modern"**
- **Don't optimize "just in case"**
- **Don't abstract "for future flexibility"**

---

*This constitution ensures the idTech3 engine remains a stable, maintainable, and extensible foundation for game development for decades to come.*
