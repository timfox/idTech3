<?php
/**
 * CI/CD Pipeline for id Tech 3
 */
$title = 'CI/CD Pipeline - id Tech 3 Documentation';
$breadcrumbs = [
    '/modernization' => 'Modernization',
    '/modernization/ci-cd' => 'CI/CD Pipeline'
];
?>

<h1>Continuous Integration/Deployment Pipeline</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Modern CI/CD practices for id Tech 3 development, including automated building, testing, and deployment across multiple platforms. This guide covers setting up robust pipelines that ensure code quality and streamline the release process.</p>
    
    <div class="feature-list">
        <h3>CI/CD Benefits</h3>
        <ul>
            <li><strong>Automated Testing:</strong> Catch regressions early with comprehensive test suites</li>
            <li><strong>Multi-Platform Builds:</strong> Consistent builds across Windows, Linux, and macOS</li>
            <li><strong>Quality Assurance:</strong> Static analysis, linting, and performance benchmarks</li>
            <li><strong>Automated Releases:</strong> Streamlined deployment and distribution</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>GitHub Actions Workflow</h2>
    
    <h3>Main Build Pipeline</h3>
    <div class="code-block">
        <pre><code># .github/workflows/build-and-test.yml
name: Build and Test

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]
  release:
    types: [ published ]

env:
  BUILD_TYPE: Release

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: windows-latest
            triplet: x64-windows
            generator: "Visual Studio 17 2022"
            artifact_name: "quake3e-windows-x64"
            
          - os: ubuntu-latest
            triplet: x64-linux
            generator: "Unix Makefiles"
            artifact_name: "quake3e-linux-x64"
            
          - os: macos-latest
            triplet: x64-osx
            generator: "Unix Makefiles" 
            artifact_name: "quake3e-macos-x64"

    runs-on: ${{ matrix.os }}
    
    steps:
    - name: Checkout
      uses: actions/checkout@v4
      with:
        submodules: recursive
        fetch-depth: 0

    - name: Setup vcpkg
      uses: lukka/run-vcpkg@v11
      with:
        vcpkgGitCommitId: 'a42af01b72c28a8e1d7b48107b33e4f286a55ef6'
        vcpkgDirectory: '${{ github.workspace }}/vcpkg'

    - name: Cache vcpkg packages
      uses: actions/cache@v3
      with:
        path: |
          ${{ github.workspace }}/vcpkg_installed/
          ${{ github.workspace }}/.vcpkg-cache/
        key: vcpkg-${{ matrix.os }}-${{ matrix.triplet }}-${{ hashFiles('vcpkg.json') }}
        restore-keys: |
          vcpkg-${{ matrix.os }}-${{ matrix.triplet }}-

    - name: Configure CMake
      run: |
        cmake -B build -S . \
          -DCMAKE_BUILD_TYPE=${{ env.BUILD_TYPE }} \
          -DCMAKE_TOOLCHAIN_FILE=${{ github.workspace }}/vcpkg/scripts/buildsystems/vcpkg.cmake \
          -DVCPKG_TARGET_TRIPLET=${{ matrix.triplet }} \
          -G "${{ matrix.generator }}" \
          -DBUILD_TESTS=ON \
          -DUSE_IMGUI=ON \
          -DUSE_TRACY=OFF

    - name: Build
      run: cmake --build build --config ${{ env.BUILD_TYPE }} --parallel 4

    - name: Run Unit Tests
      working-directory: build
      run: ctest --build-config ${{ env.BUILD_TYPE }} --parallel 4 --output-on-failure

    - name: Run Integration Tests
      working-directory: build
      run: |
        # Run headless integration tests
        ./bin/quake3e-test +set dedicated 2 +exec integration_tests.cfg +quit

    - name: Performance Benchmarks
      if: matrix.os == 'ubuntu-latest'
      working-directory: build
      run: |
        # Run automated benchmarks
        ./bin/quake3e +set r_mode 6 +exec benchmark.cfg +quit
        
    - name: Package Artifacts
      run: |
        cmake --build build --target package --config ${{ env.BUILD_TYPE }}

    - name: Upload Build Artifacts
      uses: actions/upload-artifact@v3
      with:
        name: ${{ matrix.artifact_name }}
        path: |
          build/*.zip
          build/*.tar.gz
          build/*.dmg

    - name: Upload to Release
      if: github.event_name == 'release'
      uses: softprops/action-gh-release@v1
      with:
        files: |
          build/*.zip
          build/*.tar.gz
          build/*.dmg</code></pre>
    </div>
    
    <h3>Code Quality Pipeline</h3>
    <div class="code-block">
        <pre><code># .github/workflows/code-quality.yml
name: Code Quality

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  static-analysis:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout
      uses: actions/checkout@v4
      
    - name: Setup Clang Tools
      run: |
        sudo apt-get update
        sudo apt-get install -y clang-tidy clang-format cppcheck

    - name: Run clang-format
      run: |
        find src/ -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | \
        xargs clang-format --dry-run --Werror

    - name: Run clang-tidy
      run: |
        cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        run-clang-tidy -p build -header-filter='src/.*' 'src/.*\.(c|cpp)$'

    - name: Run cppcheck
      run: |
        cppcheck --enable=all --inconclusive --xml --xml-version=2 \
          --suppress=missingIncludeSystem \
          --project=build/compile_commands.json \
          src/ 2> cppcheck-report.xml

    - name: Upload Static Analysis Results
      uses: actions/upload-artifact@v3
      with:
        name: static-analysis-reports
        path: |
          cppcheck-report.xml

  security-scan:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout
      uses: actions/checkout@v4
      
    - name: Run CodeQL Analysis
      uses: github/codeql-action/init@v2
      with:
        languages: cpp
        
    - name: Build for Analysis
      run: |
        cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
        cmake --build build --parallel 4
        
    - name: Run CodeQL Analysis
      uses: github/codeql-action/analyze@v2

  memory-safety:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout
      uses: actions/checkout@v4
      
    - name: Build with AddressSanitizer
      run: |
        cmake -B build -S . \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
          -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
        cmake --build build --parallel 4
        
    - name: Run Tests with ASan
      working-directory: build
      run: |
        export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1"
        ctest --build-config Debug --parallel 4 --output-on-failure</code></pre>
    </div>
</div>

<div class="section">
    <h2>GitLab CI/CD Pipeline</h2>
    
    <h3>Multi-Stage Pipeline</h3>
    <div class="code-block">
        <pre><code># .gitlab-ci.yml
stages:
  - build
  - test
  - quality
  - package
  - deploy

variables:
  BUILD_TYPE: "Release"
  CMAKE_GENERATOR: "Ninja"

# Build stage
.build_template: &build_template
  stage: build
  script:
    - cmake -B build -S . -G $CMAKE_GENERATOR
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE
      -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
      -DBUILD_TESTS=ON
    - cmake --build build --parallel
  artifacts:
    paths:
      - build/
    expire_in: 1 hour

build:windows:
  <<: *build_template
  tags:
    - windows
    - cmake
    - vcpkg
  variables:
    VCPKG_TARGET_TRIPLET: "x64-windows"

build:linux:
  <<: *build_template
  image: ubuntu:22.04
  before_script:
    - apt-get update && apt-get install -y build-essential cmake ninja-build
  variables:
    VCPKG_TARGET_TRIPLET: "x64-linux"

build:macos:
  <<: *build_template
  tags:
    - macos
    - xcode
  variables:
    VCPKG_TARGET_TRIPLET: "x64-osx"

# Test stage
test:unit:
  stage: test
  dependencies:
    - build:linux
  script:
    - cd build
    - ctest --parallel --output-on-failure

test:integration:
  stage: test
  dependencies:
    - build:linux
  script:
    - cd build
    - ./bin/quake3e +set dedicated 2 +exec integration_tests.cfg +quit

test:performance:
  stage: test
  dependencies:
    - build:linux
  script:
    - cd build
    - ./bin/quake3e +exec benchmark.cfg +quit
  artifacts:
    reports:
      performance: benchmark-results.json

# Quality stage
quality:static-analysis:
  stage: quality
  image: ubuntu:22.04
  before_script:
    - apt-get update && apt-get install -y clang-tidy cppcheck
  script:
    - cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    - run-clang-tidy -p build 'src/.*\.(c|cpp)$'
    - cppcheck --enable=all --project=build/compile_commands.json src/

quality:format-check:
  stage: quality
  image: ubuntu:22.04
  before_script:
    - apt-get update && apt-get install -y clang-format
  script:
    - find src/ -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" |
      xargs clang-format --dry-run --Werror

# Package stage
package:
  stage: package
  dependencies:
    - build:windows
    - build:linux
    - build:macos
  script:
    - cmake --build build --target package
  artifacts:
    paths:
      - build/*.zip
      - build/*.tar.gz
      - build/*.dmg

# Deploy stage
deploy:staging:
  stage: deploy
  dependencies:
    - package
  script:
    - echo "Deploying to staging environment"
    - rsync -av build/*.zip staging-server:/path/to/staging/
  environment:
    name: staging
    url: https://staging.example.com
  only:
    - develop

deploy:production:
  stage: deploy
  dependencies:
    - package
  script:
    - echo "Deploying to production"
    - rsync -av build/*.zip production-server:/path/to/releases/
  environment:
    name: production
    url: https://releases.example.com
  only:
    - main
  when: manual</code></pre>
    </div>
</div>

<div class="section">
    <h2>Docker-based CI</h2>
    
    <h3>Multi-stage Dockerfile</h3>
    <div class="code-block">
        <pre><code># Dockerfile.ci
# Build stage
FROM ubuntu:22.04 as builder

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    libsdl2-dev \
    libopenal-dev \
    libvulkan-dev \
    vulkan-validationlayers-dev \
    && rm -rf /var/lib/apt/lists/*

# Set up vcpkg
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
RUN /opt/vcpkg/bootstrap-vcpkg.sh
ENV VCPKG_ROOT=/opt/vcpkg

WORKDIR /src
COPY . .

# Build the project
RUN cmake -B build -S . -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DBUILD_TESTS=ON

RUN cmake --build build --parallel

# Test stage
FROM builder as tester
RUN cd build && ctest --parallel --output-on-failure

# Package stage
FROM builder as packager
RUN cmake --build build --target package

# Runtime stage
FROM ubuntu:22.04 as runtime

RUN apt-get update && apt-get install -y \
    libsdl2-2.0-0 \
    libopenal1 \
    libvulkan1 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=packager /src/build/bin/ /usr/local/bin/
COPY --from=packager /src/build/share/ /usr/local/share/

EXPOSE 27960
ENTRYPOINT ["/usr/local/bin/quake3e"]</code></pre>
    </div>
    
    <h3>Docker Compose for CI Services</h3>
    <div class="code-block">
        <pre><code># docker-compose.ci.yml
version: '3.8'

services:
  build-windows:
    image: mcr.microsoft.com/windows/servercore:ltsc2022
    volumes:
      - .:/src
      - vcpkg-cache:/vcpkg-cache
    command: >
      powershell -Command "
        cd /src;
        cmake -B build -S . -A x64
          -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake;
        cmake --build build --config Release --parallel;
      "

  build-linux:
    build:
      context: .
      dockerfile: Dockerfile.ci
      target: builder
    volumes:
      - .:/src
      - vcpkg-cache:/opt/vcpkg/cache

  test-runner:
    build:
      context: .
      dockerfile: Dockerfile.ci
      target: tester
    depends_on:
      - build-linux

  static-analysis:
    image: ubuntu:22.04
    volumes:
      - .:/src
    command: >
      bash -c "
        apt-get update && apt-get install -y clang-tidy cppcheck;
        cd /src;
        cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON;
        run-clang-tidy -p build 'src/.*\.(c|cpp)$$';
      "

volumes:
  vcpkg-cache:</code></pre>
    </div>
</div>

<div class="section">
    <h2>Performance Monitoring</h2>
    
    <h3>Automated Benchmarking</h3>
    <div class="code-block">
        <pre><code># scripts/benchmark.py
#!/usr/bin/env python3

import subprocess
import json
import sys
import time
from pathlib import Path

class BenchmarkRunner:
    def __init__(self, executable_path, maps_dir):
        self.executable = Path(executable_path)
        self.maps_dir = Path(maps_dir)
        self.results = []
    
    def run_benchmark(self, map_name, duration=30):
        """Run benchmark on specified map"""
        cmd = [
            str(self.executable),
            "+set", "dedicated", "2",
            "+map", map_name,
            "+set", "timedemo", "1",
            "+set", "com_speeds", "1",
            "+wait", str(duration * 1000),  # milliseconds
            "+quit"
        ]
        
        start_time = time.time()
        result = subprocess.run(cmd, capture_output=True, text=True)
        end_time = time.time()
        
        # Parse output for performance metrics
        output_lines = result.stdout.split('\n')
        fps_line = next((line for line in output_lines if 'fps' in line.lower()), '')
        
        if fps_line:
            fps = float(fps_line.split()[-1])
        else:
            fps = 0.0
        
        benchmark_result = {
            'map': map_name,
            'duration': duration,
            'avg_fps': fps,
            'execution_time': end_time - start_time,
            'timestamp': time.time()
        }
        
        self.results.append(benchmark_result)
        return benchmark_result
    
    def run_all_benchmarks(self):
        """Run benchmarks on all available maps"""
        maps = ['q3dm1', 'q3dm6', 'q3dm17', 'q3tourney2']
        
        for map_name in maps:
            print(f"Running benchmark: {map_name}")
            result = self.run_benchmark(map_name)
            print(f"  Average FPS: {result['avg_fps']:.1f}")
    
    def save_results(self, output_file):
        """Save benchmark results to JSON file"""
        with open(output_file, 'w') as f:
            json.dump(self.results, f, indent=2)
    
    def compare_with_baseline(self, baseline_file, threshold=0.05):
        """Compare current results with baseline"""
        try:
            with open(baseline_file, 'r') as f:
                baseline = json.load(f)
        except FileNotFoundError:
            print("No baseline found, saving current results as baseline")
            self.save_results(baseline_file)
            return True
        
        performance_regression = False
        
        for current in self.results:
            baseline_result = next(
                (b for b in baseline if b['map'] == current['map']), 
                None
            )
            
            if baseline_result:
                fps_change = (current['avg_fps'] - baseline_result['avg_fps']) / baseline_result['avg_fps']
                
                if fps_change < -threshold:
                    print(f"REGRESSION: {current['map']} - FPS dropped by {abs(fps_change)*100:.1f}%")
                    performance_regression = True
                elif fps_change > threshold:
                    print(f"IMPROVEMENT: {current['map']} - FPS improved by {fps_change*100:.1f}%")
        
        return not performance_regression

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: benchmark.py <executable_path>")
        sys.exit(1)
    
    runner = BenchmarkRunner(sys.argv[1], "maps/")
    runner.run_all_benchmarks()
    runner.save_results("benchmark-results.json")
    
    # Check for performance regressions
    if not runner.compare_with_baseline("baseline-results.json"):
        print("Performance regression detected!")
        sys.exit(1)
    
    print("Benchmark completed successfully")</code></pre>
    </div>
    
    <h3>CI Integration</h3>
    <div class="code-block">
        <pre><code># .github/workflows/performance.yml
name: Performance Monitoring

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout
      uses: actions/checkout@v4
      
    - name: Build
      run: |
        cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
        cmake --build build --parallel 4
        
    - name: Download Test Assets
      run: |
        wget https://releases.example.com/test-assets.zip
        unzip test-assets.zip -d test-assets/
        
    - name: Run Benchmarks
      run: |
        python3 scripts/benchmark.py build/bin/quake3e
        
    - name: Upload Benchmark Results
      uses: actions/upload-artifact@v3
      with:
        name: benchmark-results
        path: benchmark-results.json
        
    - name: Performance Regression Check
      run: |
        if [ -f baseline-results.json ]; then
          python3 scripts/benchmark.py build/bin/quake3e
          # Script exits with error code if regression detected
        fi
        
    - name: Update Performance Dashboard
      if: github.ref == 'refs/heads/main'
      run: |
        curl -X POST https://api.example.com/performance-data \
          -H "Content-Type: application/json" \
          -d @benchmark-results.json</code></pre>
    </div>
</div>

<div class="section">
    <h2>Release Automation</h2>
    
    <h3>Semantic Release Configuration</h3>
    <div class="code-block">
        <pre><code># .releaserc.json
{
  "branches": ["main"],
  "plugins": [
    ["@semantic-release/commit-analyzer", {
      "preset": "conventionalcommits",
      "releaseRules": [
        {"type": "feat", "release": "minor"},
        {"type": "fix", "release": "patch"},
        {"type": "perf", "release": "patch"},
        {"type": "BREAKING CHANGE", "release": "major"}
      ]
    }],
    ["@semantic-release/release-notes-generator", {
      "preset": "conventionalcommits"
    }],
    ["@semantic-release/changelog", {
      "changelogFile": "CHANGELOG.md"
    }],
    ["@semantic-release/github", {
      "assets": [
        {"path": "build/*.zip", "label": "Windows Build"},
        {"path": "build/*.tar.gz", "label": "Linux Build"},
        {"path": "build/*.dmg", "label": "macOS Build"}
      ]
    }],
    ["@semantic-release/git", {
      "assets": ["CHANGELOG.md", "package.json"],
      "message": "chore(release): ${nextRelease.version} [skip ci]\n\n${nextRelease.notes}"
    }]
  ]
}</code></pre>
    </div>
    
    <h3>Release Workflow</h3>
    <div class="code-block">
        <pre><code># .github/workflows/release.yml
name: Release

on:
  push:
    branches: [ main ]

jobs:
  release:
    runs-on: ubuntu-latest
    if: "!contains(github.event.head_commit.message, '[skip ci]')"
    
    steps:
    - name: Checkout
      uses: actions/checkout@v4
      with:
        fetch-depth: 0
        token: ${{ secrets.GITHUB_TOKEN }}
        
    - name: Setup Node.js
      uses: actions/setup-node@v3
      with:
        node-version: '18'
        
    - name: Install semantic-release
      run: |
        npm install -g semantic-release @semantic-release/git @semantic-release/github
        
    - name: Build All Platforms
      run: |
        # Trigger build matrix and wait for completion
        gh workflow run build-and-test.yml --ref main
        
        # Wait for completion and download artifacts
        sleep 300  # Wait 5 minutes for builds
        gh run download --name quake3e-windows-x64
        gh run download --name quake3e-linux-x64  
        gh run download --name quake3e-macos-x64
        
    - name: Semantic Release
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
      run: semantic-release
      
  deploy-documentation:
    needs: release
    runs-on: ubuntu-latest
    
    steps:
    - name: Deploy Documentation
      run: |
        # Update documentation website
        curl -X POST ${{ secrets.DOCS_DEPLOY_WEBHOOK }}
        
    - name: Notify Discord
      uses: sarisia/actions-status-discord@v1
      with:
        webhook: ${{ secrets.DISCORD_WEBHOOK }}
        title: "New Release Published"
        description: "Quake3e Modern v${{ needs.release.outputs.version }} is now available!"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Quality Gates</h2>
    
    <h3>Branch Protection Rules</h3>
    <div class="code-block">
        <pre><code># Repository settings for branch protection
# Configure via GitHub UI or API

# Main branch protection
{
  "required_status_checks": {
    "strict": true,
    "contexts": [
      "build (windows-latest)",
      "build (ubuntu-latest)", 
      "build (macos-latest)",
      "static-analysis",
      "security-scan",
      "performance-benchmark"
    ]
  },
  "enforce_admins": true,
  "required_pull_request_reviews": {
    "required_approving_review_count": 2,
    "dismiss_stale_reviews": true,
    "require_code_owner_reviews": true
  },
  "restrictions": null,
  "allow_force_pushes": false,
  "allow_deletions": false
}</code></pre>
    </div>
    
    <h3>SonarQube Integration</h3>
    <div class="code-block">
        <pre><code># sonar-project.properties
sonar.projectKey=quake3e-modern
sonar.projectName=Quake3e Modern
sonar.projectVersion=1.0

sonar.sources=src/
sonar.tests=tests/
sonar.cfamily.build-wrapper-output=build-wrapper-output

sonar.cfamily.cache.enabled=true
sonar.cfamily.threads=4

# Coverage settings
sonar.cfamily.gcov.reportsPath=build/coverage/
sonar.cfamily.llvm-cov.reportPath=build/coverage/coverage.txt

# Exclusions
sonar.exclusions=**/external/**,**/thirdparty/**
sonar.test.exclusions=**/mocks/**

# Quality gate
sonar.qualitygate.wait=true

# GitHub integration
sonar.pullrequest.provider=github
sonar.pullrequest.github.repository=yourorg/quake3e-modern</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/modernization/build-systems">Modern Build Systems</a></li>
        <li><a href="/modernization/package-management">Package Management</a></li>
        <li><a href="/modernization/profiling-tools">Performance Profiling</a></li>
        <li><a href="/development/debugging">Debugging Tools</a></li>
    </ul>
</div>