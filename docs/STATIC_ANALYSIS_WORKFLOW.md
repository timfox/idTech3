# Static Analysis Workflow

This document outlines the static analysis tools and workflows available for id Tech 3 to ensure code quality, catch bugs early, and maintain consistent coding standards.

## Available Tools

### Clang-Tidy
- **Purpose**: Advanced static analysis for C/C++ code
- **Capabilities**: Bug detection, performance issues, code modernization
- **Configuration**: `.clang-tidy` file in project root
- **Integration**: CMake build system integration

### Cppcheck
- **Purpose**: Static analysis tool for C/C++ code
- **Capabilities**: Bug detection, performance issues, unused code
- **Configuration**: `cppcheck.cfg` file in project root
- **Integration**: CMake build system integration

## Quick Start

### Automated Analysis (Recommended)

```bash
# Build with static analysis enabled
cd build
cmake .. -DENABLE_STATIC_ANALYSIS=ON
make

# Analysis runs automatically during compilation
```

### Manual Analysis

```bash
# Run clang-tidy on specific files
./tools/run_clang_tidy.sh src/qcommon/*.c

# Run cppcheck with HTML report
./tools/run_cppcheck.sh --html

# Fix issues automatically (where possible)
./tools/run_clang_tidy.sh --fix src/client/*.c
```

### Selective Analysis

```bash
# Enable only clang-tidy
cmake .. -DENABLE_CLANG_TIDY=ON

# Enable only cppcheck
cmake .. -DENABLE_CPPCHECK=ON
```

## Configuration Files

### .clang-tidy
The `.clang-tidy` file contains:
- **Enabled checks**: Bug detection, performance, portability, readability
- **Disabled checks**: Overly noisy checks inappropriate for game engine code
- **Check options**: Fine-tuned settings for specific checks

### cppcheck.cfg
The `cppcheck.cfg` file contains:
- **Enabled checks**: All available checks
- **Suppressions**: Known false positives and intentional patterns
- **Include paths**: Proper header file locations
- **Platform defines**: Build system compatibility

## Common Issues and Solutions

### Clang-Tidy Issues

**False Positive: Complex macro usage**
```c
// This may trigger warnings but is necessary for QVM compatibility
#define VM_CALL(func) ((void(*)(void))func)()
```
**Solution**: Suppress with `// NOLINT` comments or adjust `.clang-tidy`

**Performance: Unnecessary copies**
```c
// This might be flagged but may be required for C API compatibility
void SomeFunction(vec3_t out, const vec3_t in) {
    VectorCopy(in, out);  // May trigger performance warnings
}
```

### Cppcheck Issues

**Unused functions in headers**
```c
// QVM bridge functions may appear unused
void vmMain(int command, int arg0, int arg1, int arg2);  // NOLINT
```
**Solution**: Suppressed in `cppcheck.cfg` for known patterns

**Platform-specific code**
```c
#ifdef _WIN32
// Windows-specific code may be flagged as unused on Linux builds
void Win32SpecificFunction() { /* ... */ }
#endif
```

## CI/CD Integration

### GitHub Actions Example

```yaml
# .github/workflows/static-analysis.yml
name: Static Analysis

on: [push, pull_request]

jobs:
  clang-tidy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install clang-tidy
        run: sudo apt install -y clang-tidy
      - name: Run clang-tidy
        run: |
          ./tools/run_clang_tidy.sh --quiet --output=clang_tidy.log
          if grep -q "error:" clang_tidy.log; then
            echo "Clang-tidy found errors:"
            cat clang_tidy.log
            exit 1
          fi

  cppcheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install cppcheck
        run: sudo apt install -y cppcheck
      - name: Run cppcheck
        run: |
          ./tools/run_cppcheck.sh --quiet --text
          if grep -q "error:" cppcheck.log; then
            echo "Cppcheck found errors:"
            cat cppcheck.log
            exit 1
          fi
```

### CMake Integration

```cmake
# Enable static analysis for CI builds
if(DEFINED ENV{CI})
    set(ENABLE_STATIC_ANALYSIS ON CACHE BOOL "" FORCE)
endif()

# Treat warnings as errors in CI
if(DEFINED ENV{CI})
    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}" "--warnings-as-errors=*")
endif()
```

## Development Workflow

### Pre-commit Analysis

```bash
# Run quick analysis on staged files
./tools/run_clang_tidy.sh --quiet $(git diff --cached --name-only | grep '\.c$')

# Fix issues automatically
./tools/run_clang_tidy.sh --fix $(git diff --cached --name-only | grep '\.c$')
```

### Code Review Integration

- **Pull Request Checks**: Automated static analysis on PRs
- **Issue Classification**: Categorize findings by severity
- **Fix Validation**: Ensure fixes don't introduce new issues

### Performance Considerations

- **Incremental Analysis**: Only analyze changed files
- **Parallel Execution**: Use multiple cores for faster analysis
- **Caching**: Cache analysis results between runs

## Tool-Specific Configuration

### Clang-Tidy Check Categories

**Enabled Categories:**
- `bugprone-*`: Detect likely bugs
- `performance-*`: Performance improvements
- `readability-*`: Code readability
- `modernize-*`: Modern C++ features
- `portability-*`: Cross-platform issues

**Disabled Categories:**
- `misc-no-recursion`: Game engines often use recursion appropriately
- `readability-magic-numbers`: Game code has many domain-specific constants
- `cppcoreguidelines-pro-type-reinterpret-cast`: Necessary for engine internals

### Cppcheck Suppressions

**Function Suppressions:**
- `unusedFunction:qvm_*`: QVM bridge functions
- `unusedFunction:*SDL*`: SDL functions called via pointers
- `unusedFunction:*Z_*`: Memory management functions

**Platform Suppressions:**
- `unusedFunction:*linux*`: Platform-specific code
- `unusedFunction:*win32*`: Windows-specific code

## Troubleshooting

### Common Problems

**Clang-tidy is too slow**
- Use `--quiet` flag to reduce output
- Analyze only changed files
- Use parallel jobs: `clang-tidy --jobs=4`

**False positives overwhelm results**
- Fine-tune `.clang-tidy` configuration
- Use `// NOLINT` comments for known false positives
- Add suppressions to configuration

**Cppcheck misses header files**
- Ensure include paths are correct in `cppcheck.cfg`
- Check that header files exist and are accessible

**Build system conflicts**
- Static analysis can conflict with other build tools
- Run analysis separately from regular builds
- Use different build configurations

### Debugging Analysis Issues

```bash
# Debug clang-tidy configuration
clang-tidy --dump-config

# Debug cppcheck configuration
cppcheck --check-config src/some_file.c

# Verbose output
./tools/run_clang_tidy.sh --verbose src/some_file.c
```

## Integration with Other Tools

### Code Formatting

Static analysis works alongside clang-format:

```bash
# Format first, then analyze
./tools/format_code.sh
./tools/run_clang_tidy.sh
```

### Dynamic Analysis

Static analysis complements dynamic tools:

- **ASan/UBSan**: Runtime memory error detection
- **Valgrind**: Memory leak detection
- **Static Analysis**: Compile-time bug detection

### IDE Integration

**VS Code:**
```json
{
    "clang-tidy.checks": ["-*", "bugprone-*", "performance-*"],
    "cppcheck.options": ["--enable=all", "--suppress=missingIncludeSystem"]
}
```

**CLion:**
- Built-in clang-tidy support
- Configure cppcheck as external tool

## Best Practices

### Code Quality Standards

1. **Fix critical issues immediately**
2. **Review warnings regularly**
3. **Maintain suppression lists**
4. **Update configurations with new code patterns**

### Team Workflow

1. **Pre-commit hooks**: Run basic analysis before commits
2. **CI checks**: Enforce quality gates on PRs
3. **Regular reviews**: Manual review of analysis results
4. **Training**: Educate team on common patterns

### Maintenance

- **Regular updates**: Keep analysis tools updated
- **Configuration review**: Periodically review and update configs
- **Performance monitoring**: Track analysis time and effectiveness
- **Feedback loop**: Use results to improve development practices

This comprehensive static analysis workflow ensures high code quality, early bug detection, and maintainable code standards throughout the id Tech 3 development process.