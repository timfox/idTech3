# Advanced Testing Infrastructure

This document describes the comprehensive testing infrastructure added to idtech3, including fuzzing, performance benchmarking, security testing, GUI testing, and property-based testing.

## Overview

The testing infrastructure includes:

- **Unit Tests**: Traditional example-based tests for math, geometry, networking
- **Security Tests**: Input validation and attack vector testing
- **Fuzzing**: Network message fuzzing with libFuzzer/AFL
- **Performance Benchmarks**: Automated regression testing
- **GUI Testing**: Mock UI component interaction testing
- **Property-Based Testing**: Generated test cases for mathematical properties

## Running Tests

### All Tests at Once
```bash
# Build first
./tools/compile_engine.sh

# Run all tests
./tools/run_all_tests.sh
```

### Individual Test Suites

#### Unit Tests
```bash
cd build
./test_qmath          # Math operations
./test_geometry       # 3D geometry
./test_network_enet   # Network protocols
./test_security       # Security validation
./test_gui_basic      # GUI components
./test_property_based # Property testing
```

#### Performance Benchmarks
```bash
cd build
./benchmark_performance  # Run performance suite
```

#### Fuzzing (requires Clang)
```bash
# Quick fuzzing session (10 seconds)
timeout 10s ./fuzz_network

# Or use AFL for longer sessions
afl-fuzz -i test_input_dir -o findings_dir ./fuzz_network
```

## CI Integration

### Fuzzing in CI
- Automated fuzzing runs on Clang-based CI jobs
- Short fuzzing sessions (10 seconds) to catch obvious issues
- Runs on every PR and nightly builds

### Performance Regression Gates
- Performance benchmarks run on CI
- Automatic comparison against baseline metrics
- CI fails if performance degrades beyond thresholds:
  - Build time: +10%
  - Binary size: +5%
  - Startup time: +20%
  - Memory usage: +15%

### Test Coverage in CI
- All test suites run on every CI build
- Unit tests run on Linux (GCC/Clang), Windows, macOS
- Performance benchmarks run weekly
- Fuzzing runs on Clang builds only

## Test Categories

### 1. Unit Tests (`test_*.c`)

#### Math Tests (`test_qmath.c`)
- Vector operations (add, subtract, scale, normalize)
- Matrix operations and transformations
- Quaternion operations
- Angle normalization and delta calculations
- Bounds checking and collision detection
- Random number generation

#### Geometry Tests (`test_geometry.c`)
- Ray-plane intersection
- AABB intersection and containment
- Frustum culling basics
- Vector rotation and transformation
- Winding and polygon operations
- Bezier curve evaluation
- BSP tree traversal basics

#### Network Tests (`test_network_enet.c`)
- Basic packet transmission/reception
- Large packet handling (64KB+)
- Connection timeout behavior
- Multiple channel communication
- Ping-pong reliability testing

#### Security Tests (`test_security.c`)
- Input validation (strings, filenames, integers)
- Buffer overflow prevention
- Command injection protection
- Path traversal prevention
- Memory corruption detection
- Integer overflow protection
- Format string safety

### 2. GUI Testing Framework

#### GUI Test Framework (`test_gui_framework.h/c`)
- Mock UI context management
- Mouse and keyboard simulation
- Menu interaction testing
- Text input simulation
- Event handling verification

#### GUI Component Tests (`test_gui_basic.c`)
- Context initialization/shutdown
- Mouse movement and clicking
- Menu creation and navigation
- Text input handling
- Bounds checking and edge cases

### 3. Property-Based Testing

#### Property Framework (`test_property_based.h/c`)
- Random test case generation
- Configurable iteration counts
- Property violation detection
- Statistical testing coverage

#### Mathematical Properties Tested:
- **Commutativity**: `a + b == b + a`
- **Associativity**: `(a + b) + c == a + (b + c)`
- **Identity Elements**: `a + 0 == a`
- **Homogeneity**: `s1 * (s2 * v) == (s1 * s2) * v`
- **Vector Length**: Always positive for non-zero vectors
- **Normalization**: Unit vectors maintain direction
- **Cross Product**: Anti-commutativity and triple products

### 4. Fuzzing Infrastructure

#### Network Message Fuzzing (`fuzz_network.c`)
- Targets network message parsing code
- Tests protocol buffer handling
- Validates bounds checking in message processing
- Compatible with libFuzzer (Clang) and AFL

#### Fuzzing Strategy:
1. Generate random byte sequences
2. Attempt to parse as network messages
3. Verify no crashes or undefined behavior
4. Test edge cases and malformed inputs

### 5. Performance Benchmarking

#### Performance Suite (`benchmark_performance.c`)
- Vector operation benchmarks (10k iterations)
- Matrix transformation benchmarks
- Bounds checking performance
- Memory allocation patterns
- String operation performance

#### Regression Detection:
- Automatic baseline comparison
- Configurable performance thresholds
- CI failure on significant regressions
- Historical performance tracking

## Configuration

### Test Settings
```c
// In test files
#define BENCHMARK_ITERATIONS 10000    // Performance test iterations
#define PROPERTY_TEST_ITERATIONS 1000 // Property test cases
#define PERFORMANCE_THRESHOLD_MS 50   // Max acceptable benchmark time
```

### CI Configuration
```yaml
# In .github/workflows/ci.yml
- name: Run unit tests
  run: |
    cd build
    ./test_qmath && ./test_network_enet && ./test_geometry && ./test_security

- name: Run fuzzing tests
  if: matrix.compiler == 'clang'
  run: timeout 10s ./fuzz_network || true
```

## Extending the Framework

### Adding New Unit Tests
1. Create `test_your_feature.c` in `tests/` directory
2. Include `test_framework.h`
3. Use `TEST(test_name)` macro for test functions
4. Use `RUN_TEST(test_name)` in `main()`
5. Add to `CMakeLists.txt` build configuration

### Adding Property Tests
1. Include `test_property_based.h`
2. Use `PROPERTY_TEST(test_name)` macro
3. Generate random inputs using provided generators
4. Assert mathematical properties hold
5. Configure iteration count as needed

### Adding GUI Tests
1. Include `test_gui_framework.h`
2. Initialize GUI context with `gui_test_init()`
3. Create mock menus with `gui_test_create_menu()`
4. Simulate user interactions
5. Verify UI state changes

## Performance Baselines

The framework establishes performance baselines that can be used to detect regressions:

- **Vector Operations**: ~0.5-2.0ms for 10k operations
- **Matrix Operations**: ~1.0-3.0ms for 10k operations
- **Bounds Checking**: ~0.3-1.0ms for 10k operations
- **Memory Operations**: ~2.0-5.0ms for 1k allocations/frees

These baselines are hardware-dependent and should be established per CI environment.

## Security Testing Coverage

The security tests cover common attack vectors:

- **Input Validation**: Length limits, character restrictions
- **Buffer Overflows**: Bounds checking, safe string operations
- **Injection Attacks**: Command injection, XSS, SQL-like patterns
- **Path Traversal**: Directory traversal prevention
- **Memory Safety**: Use-after-free, double-free detection
- **Integer Handling**: Overflow protection, bounds validation

## Future Enhancements

- **Integration Testing**: End-to-end game session testing
- **Visual Regression**: Screenshot comparison for UI changes
- **Load Testing**: Stress testing with many concurrent operations
- **Cross-Platform Validation**: Ensure behavior consistency across platforms
- **Performance Profiling**: Integration with Tracy/profiler for detailed analysis

## Troubleshooting

### Common Issues

**Tests Fail on Different Platforms**
- Some tests may have platform-specific behavior (endianness, floating-point precision)
- Use appropriate epsilon values for floating-point comparisons
- Consider platform-specific test variants

**Performance Tests Inconsistent**
- Performance can vary based on system load and CPU frequency scaling
- Run tests multiple times and use statistical analysis
- Consider CPU pinning for consistent results

**GUI Tests Flaky**
- GUI tests depend on precise timing and state management
- Ensure proper cleanup between tests
- Use deterministic delays for animations/transitions

**Fuzzing Finds False Positives**
- Some "crashes" may be expected behavior (graceful error handling)
- Review fuzzing output to distinguish real bugs from expected failures
- Adjust fuzzing input generation to avoid known edge cases
