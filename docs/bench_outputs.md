### Bench outputs and dashboard usage

- What you get after running the benchmarks
  - `bench.json`: rich per-iteration timing data plus memory samples (linux). Contains fields such as:
    - `timestamp`, `pathTracer_ms`, `rtx_ms`, `denoiser_ms`, `fsr_ms`, `iterations_pathTracer`, `width`, `height`
    - Optionally per-iteration arrays: `pathTracer_perIterMs`, `rtx_perIterMs`, `denoiser_perIterMs`, `fsr_perIterMs`
    - Memory arrays: `memory_per_iter_mb`, `memory_end_mb`
  - `bench_summary.json`: run summary with overall timings and end-state metrics, including:
    - `start_time`, `end_time`, `duration_seconds`, `memory_end_mb`
  - `bench_timeseries.jsonl`: per-iteration lines for time-series analysis, including:
    - `timestamp`, `idx`, `pathTracer_ms`, `pathTracer_delta_ms`, `rtx_ms`, `rtx_delta_ms`, `denoiser_ms`, `denoiser_delta_ms`, `fsr_ms`, `fsr_delta_ms`, `memory_mb`
  - `bench.csv`: compact CSV form produced by `tools/bench_json_to_csv.py`, with optional per-iteration fields when requested:
    - header includes: `timestamp`, `pathTracer_ms`, `rtx_ms`, `denoiser_ms`, `fsr_ms`, `iterations_pathTracer`, `width`, `height`
    - optional per-iteration columns: `pathTracer_perIterMs`, `rtx_perIterMs`, `denoiser_perIterMs`, `fsr_perIterMs`, `memory_per_iter_mb`, `memory_end_mb` (as JSON strings)
  - `bench_dashboard.html` (under `web/`): HTML dashboard visualizing bench data via the dashboard generator.
- How to view dashboards and data
  - Dashboard (HTML)
    - Location: `web/bench_dashboard.html`
    - Generate with: `python3 tools/dashboard_generator.py --input bench.json --output web/bench_dashboard.html`
  - CSV
    - Convert bench.json to CSV with per-iteration data: `python3 tools/bench_json_to_csv.py bench.json --include-per-iter > bench.csv`
  - Time-series
    - The per-iteration time-series is emitted as `bench_timeseries.jsonl` in the repo root.
- Cross-platform memory metrics
  - Linux: memory in MB from `/proc/self/statm` (default)
  - Windows/macOS: memory from platform-specific calls (via cross-platform memory helper in bench)
- Wayland toggle for testing
  - You can force using Wayland even if SDL would fallback to X11 by setting the environment variable `WAYLAND_FORCE=1`.
  - In that mode, the system will log that Wayland is forced and will avoid an X11 fallback.
  - This is useful for deterministic testing of the Wayland path in CI or dedicated tests.
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

- Notes
  - All file names and paths are subject to the CI and local build layout.
  - The dashboard is designed to be lightweight and pluggable into your existing data pipeline.
