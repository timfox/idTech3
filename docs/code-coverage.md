# Code Coverage Reporting

## Overview

The id Tech 3 engine supports code coverage reporting using `gcov`/`gcovr` and `lcov` to identify untested code and measure test coverage.

## Prerequisites

### Required Tools

**gcovr** (recommended):
```bash
# Ubuntu/Debian
sudo apt install gcovr

# macOS
brew install gcovr

# Or via pip
pip install gcovr
```

**lcov** (alternative):
```bash
# Ubuntu/Debian
sudo apt install lcov

# macOS
brew install lcov
```

## Building with Coverage

### Step 1: Configure CMake with Coverage Enabled

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON
```

### Step 2: Build the Project

```bash
cmake --build . -j$(nproc)
```

This will:
- Compile with `--coverage` flags (GCC/Clang)
- Generate `.gcda` and `.gcno` files during compilation
- Link with coverage instrumentation

### Step 3: Run Tests

Tests must be run to generate coverage data:

```bash
# Run all tests
ctest --output-on-failure

# Or run specific tests
./tests/test_qcommon
./tests/test_qmath
./tests/test_memory
```

### Step 4: Generate Coverage Reports

#### Using gcovr (Recommended)

```bash
# Generate HTML, XML, and text reports
cmake --build . --target coverage

# Reports will be generated in build/:
# - coverage.html (interactive HTML report)
# - coverage.xml (for CI/CD integration)
# - coverage.txt (summary text report)
```

#### Using lcov

```bash
# Generate lcov HTML report
cmake --build . --target coverage_lcov

# Report will be in build/coverage_lcov/
```

## Coverage Reports

### HTML Report (gcovr)

Open `build/coverage.html` in a web browser to view:
- Overall coverage percentage
- File-by-file coverage breakdown
- Line-by-line coverage highlighting
- Branch coverage information

### XML Report (gcovr)

The `coverage.xml` file can be integrated with:
- **Jenkins** (Cobertura plugin)
- **GitLab CI** (coverage visualization)
- **GitHub Actions** (coverage badges)
- **Codecov** / **Coveralls** (online coverage tracking)

### Text Summary

The `coverage.txt` file provides a quick summary:
```
------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: /home/user/idtech3/src
------------------------------------------------------------------------------
File                                       Lines    Exec  Cover   Missing
------------------------------------------------------------------------------
qcommon/common.c                            1234    890   72%    45-67,123-145
qcommon/q_shared.c                           567    456   80%    89-92
...
------------------------------------------------------------------------------
TOTAL                                       5678   4234   74%
------------------------------------------------------------------------------
```

## Coverage Targets

| Target | Description | Output |
|--------|-------------|--------|
| `coverage` | Generate gcovr reports (HTML, XML, TXT) | `coverage.html`, `coverage.xml`, `coverage.txt` |
| `coverage_lcov` | Generate lcov HTML report | `coverage_lcov/index.html` |
| `coverage_run_tests` | Run tests to generate coverage data | (runs tests) |

## CI/CD Integration

### GitHub Actions Example

```yaml
- name: Build with Coverage
  run: |
    cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON
    cmake --build build

- name: Run Tests
  run: |
    cd build
    ctest --output-on-failure

- name: Generate Coverage Report
  run: |
    cd build
    cmake --build . --target coverage

- name: Upload Coverage to Codecov
  uses: codecov/codecov-action@v3
  with:
    files: build/coverage.xml
    flags: unittests
```

### GitLab CI Example

```yaml
coverage:
  script:
    - cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON
    - cmake --build build
    - cd build && ctest --output-on-failure
    - cmake --build . --target coverage
  coverage: '/TOTAL.*\s+(\d+\.\d+)%/'
  artifacts:
    reports:
      cobertura: build/coverage.xml
```

## Coverage Exclusions

The following are automatically excluded from coverage reports:
- Third-party libraries (`libs/`)
- Build artifacts (`build/`)
- Test files themselves (`tests/`)

To exclude additional files, modify the `--exclude` patterns in `CMakeLists.txt`.

## Interpreting Coverage

### Coverage Metrics

- **Line Coverage**: Percentage of executable lines executed
- **Branch Coverage**: Percentage of branches taken
- **Function Coverage**: Percentage of functions called

### Coverage Goals

- **Critical code**: Aim for 90%+ coverage
- **Core engine code**: Aim for 80%+ coverage
- **Utility code**: Aim for 70%+ coverage
- **Legacy code**: Document gaps, improve incrementally

### Common Issues

**Low Coverage:**
- Add unit tests for untested functions
- Test error paths and edge cases
- Test boundary conditions

**Missing Coverage Data:**
- Ensure tests actually run (`ctest` or manual execution)
- Check that `.gcda` files are generated
- Verify `ENABLE_COVERAGE=ON` was set during build

**Coverage Not Updating:**
- Clean build directory: `rm -rf build && mkdir build`
- Rebuild with coverage: `cmake .. -DENABLE_COVERAGE=ON`
- Re-run tests after code changes

## Best Practices

1. **Run coverage regularly** during development
2. **Set coverage thresholds** in CI/CD (e.g., fail if coverage drops below 70%)
3. **Focus on critical paths** first (networking, filesystem, memory)
4. **Don't aim for 100%** - focus on meaningful tests
5. **Use coverage to find gaps**, not as the only quality metric

## Troubleshooting

### "gcovr not found"
```bash
sudo apt install gcovr
# or
pip install gcovr
```

### "No coverage data found"
- Ensure `ENABLE_COVERAGE=ON` during CMake configuration
- Run tests after building
- Check that `.gcda` files exist in build directory

### "Coverage shows 0%"
- Verify compiler is GCC or Clang (coverage requires these)
- Check that `--coverage` flags are in compile commands
- Ensure tests actually execute the code paths

### Coverage with Sanitizers

Coverage and sanitizers (ASan/UBSan) can conflict. If you need both:
1. Build with sanitizers first for testing
2. Build separate coverage build without sanitizers
3. Or use `ENABLE_COVERAGE=ON` without sanitizers

## Related Documentation

- [Unit Testing Guide](tests/README.md)
- [Static Analysis Workflow](STATIC_ANALYSIS_WORKFLOW.md)
- [Memory Safety Workflows](MEMORY_SAFETY_WORKFLOWS.md)
