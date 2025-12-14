# Memory Safety Workflows

This document outlines the recommended workflows for detecting and debugging memory-related issues in id Tech 3 using various memory analysis tools.

## Available Tools

### AddressSanitizer (ASan)
- **Purpose**: Detect memory corruption, use-after-free, buffer overflows
- **Platforms**: Linux, macOS, Windows
- **Performance Impact**: ~2x slowdown, 2-3x memory usage

### UndefinedBehaviorSanitizer (UBSan)
- **Purpose**: Detect undefined behavior (integer overflow, null pointer dereference, etc.)
- **Platforms**: Linux, macOS, Windows
- **Performance Impact**: Minimal (~10% slowdown)

### Valgrind (Memcheck)
- **Purpose**: Comprehensive memory error detection and leak checking
- **Platforms**: Linux, macOS (limited)
- **Performance Impact**: 10-50x slowdown (depending on workload)

### Dr. Memory (Windows)
- **Purpose**: Memory corruption and leak detection (similar to Valgrind)
- **Platforms**: Windows
- **Performance Impact**: 2-10x slowdown

## Quick Start

### ASan + UBSan (Recommended for Development)

```bash
# Build with sanitizers
cd build
cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
make

# Run normally - sanitizer reports will appear on stderr
./ioquake3.x86_64
```

### Valgrind (Comprehensive Analysis)

```bash
# Build with Valgrind support
cd build
cmake .. -DENABLE_VALGRIND=ON
make

# Run with Valgrind
./tools/run_valgrind.sh ./ioquake3.x86_64 +set dedicated 1 +map q3dm1

# Check results
cat valgrind.log
```

### Dr. Memory (Windows)

```bash
# Build with Dr. Memory support
cmake .. -DENABLE_DRMEMORY=ON
cmake --build . --config Debug

# Run with Dr. Memory
tools\run_drmemory.bat ioquake3.x86_64.exe +set dedicated 1 +map q3dm1
```

## Common Issues and Solutions

### False Positives

Some libraries intentionally leak memory or use patterns that look like errors:

- **SDL**: Uses static initialization that appears as leaks
- **OpenAL**: Context and device management
- **Vulkan**: Driver initialization and resource management
- **X11**: Display connection management

These are suppressed in `valgrind.supp`.

### Performance Considerations

- **ASan**: Good for interactive development and CI
- **Valgrind**: Use for targeted analysis only (too slow for gameplay)
- **UBSan**: Always enable in development builds

### CI Integration

For automated testing, use ASan in CI pipelines:

```yaml
# Example GitHub Actions
- name: Build with sanitizers
  run: |
    cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
    make

- name: Run tests
  run: |
    ctest --output-on-failure
    # ASan will abort on errors, failing the CI job
```

## Debugging Memory Issues

### Interpreting ASan Reports

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-use-after-free
READ of size 4 at 0x7fff12345678
    #0 0xdeadbeef in SomeFunction source.c:42
    #1 0xfeedface in AnotherFunction source.c:31
    #2 0x12345678 in main main.c:15

freed by thread T0 here:
    #0 0xdeadbeef in free (/usr/lib/libasan.so)
    #1 0xfeedface in Z_Free source.c:123

allocated by thread T0 here:
    #0 0xdeadbeef in malloc (/usr/lib/libasan.so)
    #1 0xfeedface in Z_Malloc source.c:456
```

**Action Items:**
1. Check allocation/deallocation balance
2. Look for double-free or use-after-free patterns
3. Verify pointer ownership and lifetime

### Valgrind Memcheck Reports

```
==12345== Invalid read of size 4
==12345==    at 0xDEADBEEF: SomeFunction (source.c:42)
==12345==    by 0xFEEDFACE: main (main.c:15)
==12345==  Address 0x12345678 is 0 bytes inside a block of size 100 free'd
==12345==    at 0xDEADBEEF: free (vg_replace_malloc.c:530)
==12345==    by 0xFEEDFACE: Z_Free (memory.c:123)
```

**Action Items:**
1. Use `--track-origins=yes` for allocation source info
2. Check suppression file for known false positives
3. Focus on "definite" leaks first, then "possible"

## Best Practices

### Development Workflow

1. **Always enable ASan/UBSan** in debug builds
2. **Run Valgrind** on suspected memory issues
3. **Use suppression files** to reduce noise
4. **Fix issues immediately** - don't accumulate technical debt

### Code Review Checklist

- [ ] No raw `malloc`/`free` usage (use engine allocators)
- [ ] Pointer ownership is clear and documented
- [ ] Arrays have proper bounds checking
- [ ] String operations use safe functions
- [ ] Memory is freed in error paths

### Performance Optimization

When memory checking impacts performance too much:

1. **Selective instrumentation**: Only instrument specific modules
2. **Sampling**: Use periodic checking instead of continuous
3. **Release builds**: Memory checkers can be disabled for performance-critical sections

## Tool-Specific Configuration

### Valgrind Options

```bash
# For detailed analysis
valgrind --tool=memcheck \
         --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --suppressions=valgrind.supp \
         ./ioquake3.x86_64
```

### ASan Options

```bash
# Environment variables
export ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:strict_init_order=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0"

./ioquake3.x86_64
```

### Dr. Memory Options

```bash
drmemory -batch \
         -results_to_stderr \
         -callstack_max_frames 20 \
         ioquake3.x86_64.exe
```

## Troubleshooting

### Common Issues

**Q: ASan reports false positives in third-party code**
A: Use `-fsanitize-ignorelist=file.txt` to exclude problematic functions

**Q: Valgrind is too slow for gameplay testing**
A: Use callgrind with `--collect-atstart=no` and trigger collection programmatically

**Q: Memory checker conflicts with other tools**
A: Disable other instrumentation when using memory checkers

**Q: False leak reports in static initialization**
A: Add suppressions for known static allocation patterns

## Integration with CI/CD

```yaml
# .github/workflows/memory-safety.yml
name: Memory Safety Checks

on: [push, pull_request]

jobs:
  asan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with ASan
        run: |
          cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
          make
      - name: Run tests
        run: ctest --output-on-failure

  valgrind:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with Valgrind
        run: |
          cmake .. -DENABLE_VALGRIND=ON
          make
      - name: Quick Valgrind check
        run: |
          timeout 30s ./tools/run_valgrind.sh ./ioquake3.x86_64 --help || true
          # Check for critical errors in log
          if grep -q "definitely lost\|Invalid read\|Invalid write" valgrind.log; then
            echo "Memory issues detected"
            cat valgrind.log
            exit 1
          fi
```

This comprehensive memory safety workflow ensures robust detection and debugging of memory-related issues throughout the development process.