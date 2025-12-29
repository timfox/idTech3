#!/usr/bin/env python3
"""
Bench JSON to CSV converter
Converts a bench.json produced by renderer_bench into a compact CSV row for trend analysis.
Usage:
  python3 tools/bench_json_to_csv.py path/to/bench.json
"""
import json
import csv
import sys
from datetime import datetime

def load_bench_json(path: str) -> dict:
    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    return data

def main(argv):
    if len(argv) < 2:
        print("Usage: bench_json_to_csv.py bench.json", file=sys.stderr)
        return 2
    bench_path = argv[1]
    data = load_bench_json(bench_path)
    # Expect a single JSON object with fields like:
    # timestamp, pathTracer_ms, rtx_ms, denoiser_ms, fsr_ms, iterations_pathTracer, width, height
    timestamp = data.get("timestamp")
    pathTracer_ms = data.get("pathTracer_ms")
    rtx_ms = data.get("rtx_ms")
    denoiser_ms = data.get("denoiser_ms")
    fsr_ms = data.get("fsr_ms")
    iterations_pathTracer = data.get("iterations_pathTracer")
    width = data.get("width")
    height = data.get("height")

    # Fallback: if bench.json structure is different, try a minimal approach
    if timestamp is None:
        # Try to parse from bench_summary.json style
        # bench_summary.json may contain pathTracer_avg_ms instead of pathTracer_ms
        timestamp = datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")
        if "pathTracer_avg_ms" in data:
            pathTracer_ms = float(data["pathTracer_avg_ms"])
        if "rtx_avg_ms" in data:
            rtx_ms = float(data["rtx_avg_ms"])
        if "denoiser_avg_ms" in data:
            denoiser_ms = float(data["denoiser_avg_ms"])
        if "fsr_avg_ms" in data:
            fsr_ms = float(data["fsr_avg_ms"])
        iterations_pathTracer = int(data.get("pathTracer_iterations", 0))
        if width is None: width = int(data.get("width", 0))
        if height is None: height = int(data.get("height", 0))

    # Prepare CSV header and row
    header = ["timestamp","pathTracer_ms","rtx_ms","denoiser_ms","fsr_ms","iterations_pathTracer","width","height"]
    row = [str(timestamp),
           float(pathTracer_ms) if pathTracer_ms is not None else "",
           float(rtx_ms) if rtx_ms is not None else "",
           float(denoiser_ms) if denoiser_ms is not None else "",
           float(fsr_ms) if fsr_ms is not None else "",
           int(iterations_pathTracer) if iterations_pathTracer is not None else "",
           int(width) if width is not None else "",
           int(height) if height is not None else ""]

    # Write to stdout as single-line CSV
    writer = csv.writer(sys.stdout)
    writer.writerow(header)
    writer.writerow(row)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))

