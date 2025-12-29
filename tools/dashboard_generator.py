#!/usr/bin/env python3
"""
HTML dashboard generator for bench results.
Supports bench.json (rich with per-iteration arrays) or bench_summary.json (summary only).
Outputs a standalone HTML file at web/bench_dashboard.html.
Usage:
  python3 tools/dashboard_generator.py --input bench.json [--output web/bench_dashboard.html]
"""
import json
import math
import os
import sys
from datetime import datetime

def load_json(path: str) -> dict:
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)

def sparkline_svg(vals, w=400, h=40):
    if not vals:
        return ""
    maxv = max(vals)
    maxv = max(1.0, maxv)
    n = len(vals)
    if n == 1:
        return f'<line x1="0" y1="{h/2}" x2="{w}" y2="{h/2}" stroke="steelblue" stroke-width="2"/>\n'
    pts = []
    for i, v in enumerate(vals):
        x = (i / (n - 1)) * w
        y = h - (v / maxv) * h
        pts.append(f"{x:.2f},{y:.2f}")
    return f"<polyline fill='none' stroke='steelblue' stroke-width='2' points='{ ' '.join(pts) }' />\n"

def render_dashboard(data: dict) -> str:
    timestamp = data.get("timestamp", datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"))
    pathTracer = data.get("pathTracer_ms")
    rtx = data.get("rtx_ms")
    denoiser = data.get("denoiser_ms")
    fsr = data.get("fsr_ms")
    memEnd = data.get("memory_end_mb")
    pathsIter = data.get("pathTracer_perIterMs", [])
    rtIter = data.get("rtx_perIterMs", [])
    dnIter = data.get("denoiser_perIterMs", [])
    fsIter = data.get("fsr_perIterMs", [])

    # Basic layout
    html = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <title>Renderer Bench Dashboard</title>
  <style>
    body {{ font-family: Arial, sans-serif; padding: 20px; }}
    h1 {{ font-size: 1.6rem; }}
    .metrics {{ display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; }}
    .card {{ border: 1px solid #ddd; border-radius: 8px; padding: 12px; background: #fff; }}
    .bar { height: 18px; background: #4CAF50; border-radius: 6px; }
    svg { width: 100%; height: 40px; display: block; }
  </style>
</head>
<body>
  <h1>Renderer Bench Dashboard</h1>
  <p>Generated: {datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S UTC')}</p>

  <div class="metrics">
    <div class="card">
      <strong>PathTracer avg (ms)</strong>
      <div class="bar" style="width: {pathTracer and max(0, min(100, (pathTracer/ max(1e-6, max(pathTracer or 1, 1.0))) * 100)):.0f}%"></div>
      <div>Value: {pathTracer:.3f} ms</div>
      { (sparkline_svg(pathsIter)) if pathsIter else "" }
    </div>
    <div class="card">
      <strong>RTX avg (ms)</strong>
      <div class="bar" style="width: {rtx and max(0, min(100, (rtx/ max(1e-6, max(rtx or 1, 1.0))) * 100)):.0f}%"></div>
      <div>Value: {rtx:.3f} ms</div>
      { (sparkline_svg(rtIter)) if rtIter else "" }
    </div>
    <div class="card">
      <strong>Denoiser avg (ms)</strong>
      <div class="bar" style="width: {denoiser and max(0, min(100, (denoiser/ max(1e-6, max(denoiser or 1, 1.0))) * 100)):.0f}%"></div>
      <div>Value: {denoiser:.3f} ms</div>
      { (sparkline_svg(dnIter)) if dnIter else "" }
    </div>
    <div class="card">
      <strong>FSR avg (ms)</strong>
      <div class="bar" style="width: {fsr and max(0, min(100, (fsr/ max(1e-6, max(fsr or 1, 1.0))) * 100)):.0f}%"></div>
      <div>Value: {fsr:.3f} ms</div>
      { (sparkline_svg(fsIter)) if fsIter else "" }
    </div>
  </div>

  { f'<div class="card" style="margin-top:20px;"><h3>Memory</h3><p>End mb: {memEnd if memEnd is not None else "N/A"}</p></div>' if memEnd is not None else "" }

</body></html>"""
    return html

def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True, help="bench.json or bench_summary.json")
  ap.add_argument("--output", default="web/bench_dashboard.html", help="Output HTML file")
    args = ap.parse_args()

    inpath = args.input
    if not os.path.exists(inpath):
        print(f"Input file not found: {inpath}", file=sys.stderr)
        sys.exit(2)
    data = None
    if inpath.endswith(".json") and os.path.basename(inpath) == "bench.json":
        data = load_json(inpath)
        # bench.json may contain per-iteration arrays
    else:
        data = load_json(inpath)

    html = render_dashboard(data)
    outpath = args.output
    os.makedirs(os.path.dirname(outpath), exist_ok=True)
    with open(outpath, "w", encoding="utf-8") as f:
        f.write(html)
    print(f"Wrote dashboard: {outpath}")

if __name__ == "__main__":
    main()

