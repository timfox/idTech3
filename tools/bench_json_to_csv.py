#!/usr/bin/env python3
"""
Bench JSON to CSV converter
Converts a bench.json produced by renderer_bench into a compact CSV row for trend analysis.
Usage:
  python3 tools/bench_json_to_csv.py path/to/bench.json [--include-per-iter] [--output <path>]
"""
import json
import csv
import sys
import os
import argparse
from datetime import datetime

def load_bench_json(path: str) -> dict:
    with open(path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    return data

def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("bench_json", help="bench.json path")
    parser.add_argument("--include-per-iter", action="store_true", dest="include_per_iter",
                        help="include per-iteration arrays as JSON strings in CSV (new fields)")
    parser.add_argument("--output", dest="output", default=None, help="output CSV path (optional)")
    args = parser.parse_args(argv[1:])

    bench_path = args.bench_json
    data = load_bench_json(bench_path)

    timestamp = data.get("timestamp")
    pathTracer_ms = data.get("pathTracer_ms")
    rtx_ms = data.get("rtx_ms")
    denoiser_ms = data.get("denoiser_ms")
    fsr_ms = data.get("fsr_ms")
    iterations_pathTracer = data.get("iterations_pathTracer")
    width = data.get("width")
    height = data.get("height")

    if timestamp is None:
        timestamp = datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")

    header = ["timestamp","pathTracer_ms","rtx_ms","denoiser_ms","fsr_ms","iterations_pathTracer","width","height"]
    row = [str(timestamp),
           float(pathTracer_ms) if pathTracer_ms is not None else "",
           float(rtx_ms) if rtx_ms is not None else "",
           float(denoiser_ms) if denoiser_ms is not None else "",
           float(fsr_ms) if fsr_ms is not None else "",
           int(iterations_pathTracer) if iterations_pathTracer is not None else "",
           int(width) if width is not None else "",
           int(height) if height is not None else ""]

    if args.include_per_iter:
        pathTracer_perIter = data.get("pathTracer_perIterMs", [])
        rtx_perIter = data.get("rtx_perIterMs", [])
        denoiser_perIter = data.get("denoiser_perIterMs", [])
        fsr_perIter = data.get("fsr_perIterMs", [])
        mem_perIter = data.get("memory_per_iter_mb", [])
        memory_end = data.get("memory_end_mb")
        import json as _json
        header += ["pathTracer_perIterMs","rtx_perIterMs","denoiser_perIterMs","fsr_perIterMs","memory_per_iter_mb","memory_end_mb"]
        row += [
            _json.dumps(pathTracer_perIter),
            _json.dumps(rtx_perIter),
            _json.dumps(denoiser_perIter),
            _json.dumps(fsr_perIter),
            _json.dumps(mem_perIter),
            memory_end if memory_end is not None else ""
        ]

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(header)
            writer.writerow(row)
        return 0

    writer = csv.writer(sys.stdout)
    writer.writerow(header)
    writer.writerow(row)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))

