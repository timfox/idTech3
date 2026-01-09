### Bench outputs and dashboard usage

- What you get after running the benchmarks
  - `tests/benchmarks/bench.json`: rich per-iteration timing data plus memory samples (linux). Contains fields such as:
    - `timestamp`, `pathTracer_ms`, `rtx_ms`, `denoiser_ms`, `fsr_ms`, `iterations_pathTracer`, `width`, `height`
    - Optionally per-iteration arrays: `pathTracer_perIterMs`, `rtx_perIterMs`, `denoiser_perIterMs`, `fsr_perIterMs`
    - Memory arrays: `memory_per_iter_mb`, `memory_end_mb`
  - `tests/benchmarks/bench_summary.json`: run summary with overall timings and end-state metrics, including:
    - `start_time`, `end_time`, `duration_seconds`, `memory_end_mb`
  - `tests/benchmarks/bench_timeseries.jsonl`: per-iteration lines for time-series analysis, including:
    - `timestamp`, `idx`, `pathTracer_ms`, `pathTracer_delta_ms`, `rtx_ms`, `rtx_delta_ms`, `denoiser_ms`, `denoiser_delta_ms`, `fsr_ms`, `fsr_delta_ms`, `memory_mb`
  - `tests/benchmarks/bench.csv`: compact CSV form produced by `tools/bench_json_to_csv.py`, with optional per-iteration fields when requested:
    - header includes: `timestamp`, `pathTracer_ms`, `rtx_ms`, `denoiser_ms`, `fsr_ms`, `iterations_pathTracer`, `width`, `height`
    - optional per-iteration columns: `pathTracer_perIterMs`, `rtx_perIterMs`, `denoiser_perIterMs`, `fsr_perIterMs`, `memory_per_iter_mb`, `memory_end_mb` (as JSON strings)
  - `tests/benchmarks/bench_dashboard.html`: HTML dashboard visualizing bench data via the dashboard generator.
- How to view dashboards and data
  - Dashboard (HTML)
    - Location: `tests/benchmarks/bench_dashboard.html`
    - Generate with: `python3 tools/dashboard_generator.py --input tests/benchmarks/bench.json --output tests/benchmarks/bench_dashboard.html`
  - CSV
    - Convert bench.json to CSV with per-iteration data: `python3 tools/bench_json_to_csv.py tests/benchmarks/bench.json --include-per-iter > tests/benchmarks/bench.csv`
  - Time-series
    - The per-iteration time-series is emitted as `tests/benchmarks/bench_timeseries.jsonl`.
- Cross-platform memory metrics
  - Linux: memory in MB from `/proc/self/statm` (default)
  - Windows/macOS: memory from platform-specific calls (via cross-platform memory helper in bench)
- Wayland testing
  - To test Wayland fallback behavior, use `SDL_VIDEODRIVER=wayland` or `+set r_wayland 1`
  - The engine will attempt Wayland first, then automatically fallback to X11 if it fails
  - CI testing: Run `scripts/ci_wayland_fallback.sh` to validate the complete fallback logic
- Quick usage examples
  - Minimal bench.json example (snippet)
    {
      "timestamp": "2025-01-01T00:00:00Z",
      "pathTracer_ms": 1.0,
      "rtx_ms": 0.5,
      "denoiser_ms": 0.2,
      "fsr_ms": 0.1,
      "iterations_pathTracer": 3,
      "width": 128,
      "height": 128,
      "pathTracer_perIterMs": [0.3, 0.4, 0.3],
      "rtx_perIterMs": [0.2, 0.25, 0.05],
      "denoiser_perIterMs": [0.1, 0.05, 0.05],
      "fsr_perIterMs": [0.04, 0.04, 0.02],
      "memory_per_iter_mb": [100.1, 100.3, 100.5],
      "memory_end_mb": 100.6
    }

- Engine Profiling System
  - The engine includes a comprehensive profiling system that integrates multiple backends:
    - **Tracy**: CPU/GPU profiling when USE_TRACY=1
    - **Vulkan Render Profiler**: GPU render graph analysis
    - **Performance Benchmarking**: Automated regression detection
    - **Memory Bandwidth Profiler**: Memory access pattern analysis
  - Control profiling via CVARs:
    - `profiler_mode`: 0=disabled, 1=basic, 2=vulkan, 3=full, 4=benchmark
    - `profiler_overhead_limit`: Maximum profiling overhead percentage
  - Runtime commands:
    - `profiler_status`: Show current profiling status
    - `profiler_toggle`: Cycle through profiling modes
    - `profiler_dump`: Dump current frame profiling data
    - `profiler_reset`: Reset profiling statistics
    - `profiler_export`: Export profiling data to text file
    - `profiler_export_json`: Export to JSON format
    - `profiler_export_csv`: Export to CSV format
    - `profiler_increase_detail`/`profiler_decrease_detail`: Adjust profiling detail level

- Notes
  - All file names and paths are subject to the CI and local build layout.
  - The dashboard is designed to be lightweight and pluggable into your existing data pipeline.
  - Profiling can be enabled at build time with USE_TRACY=1 for Tracy integration.