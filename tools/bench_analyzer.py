#!/usr/bin/env python3
"""
Enhanced Benchmark Analyzer
Generates dashboards and analysis from benchmark time-series data.

Usage:
  python3 tools/bench_analyzer.py benchmark_timeseries.jsonl [--output-dir <dir>] [--generate-html] [--compare-baseline <baseline.jsonl>]
"""

import json
import sys
import os
import argparse
from datetime import datetime
from collections import defaultdict
import statistics
try:
    import matplotlib.pyplot as plt
    import pandas as pd
    import seaborn as sns
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib/pandas/seaborn not available. Skipping visualizations.")

class BenchmarkAnalyzer:
    def __init__(self):
        self.data = defaultdict(lambda: defaultdict(list))
        self.metadata = {}

    def load_timeseries_data(self, filepath):
        """Load time-series benchmark data from JSONL file."""
        print(f"Loading benchmark data from {filepath}...")

        with open(filepath, 'r') as f:
            for line_num, line in enumerate(f, 1):
                try:
                    entry = json.loads(line.strip())
                    timestamp = entry['timestamp']
                    test = entry['test']
                    metric = entry['metric']
                    value = entry['value']

                    # Store data by test and metric
                    key = f"{test}_{metric}"
                    self.data[key]['timestamps'].append(timestamp)
                    self.data[key]['values'].append(value)

                    # Store iteration data if present
                    if 'iteration' in entry:
                        if 'iterations' not in self.data[key]:
                            self.data[key]['iterations'] = []
                        self.data[key]['iterations'].append(entry['iteration'])

                except json.JSONDecodeError as e:
                    print(f"Warning: Failed to parse line {line_num}: {e}")
                    continue

        print(f"Loaded data for {len(self.data)} metric series")

    def analyze_performance_trends(self):
        """Analyze performance trends and generate statistics."""
        analysis = {}

        for key, data in self.data.items():
            if 'values' in data and len(data['values']) > 0:
                values = data['values']

                # Calculate basic statistics
                stats = {
                    'count': len(values),
                    'mean': statistics.mean(values),
                    'median': statistics.median(values),
                    'min': min(values),
                    'max': max(values),
                    'std_dev': statistics.stdev(values) if len(values) > 1 else 0,
                    'latest': values[-1]
                }

                # Calculate trend (simple linear regression slope)
                if len(values) > 1:
                    x = list(range(len(values)))
                    slope = statistics.linear_regression(x, values).slope
                    stats['trend'] = slope
                    stats['trend_direction'] = 'improving' if slope < 0 else 'degrading'
                else:
                    stats['trend'] = 0
                    stats['trend_direction'] = 'stable'

                analysis[key] = stats

        return analysis

    def generate_performance_report(self, output_dir="benchmark_reports"):
        """Generate comprehensive performance report."""
        os.makedirs(output_dir, exist_ok=True)

        analysis = self.analyze_performance_trends()

        # Generate text report
        report_path = os.path.join(output_dir, "performance_report.txt")
        with open(report_path, 'w') as f:
            f.write("Benchmark Performance Analysis Report\n")
            f.write("=" * 50 + "\n\n")
            f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")

            for key, stats in analysis.items():
                test_name, metric = key.rsplit('_', 1)
                f.write(f"Test: {test_name}\n")
                f.write(f"Metric: {metric}\n")
                f.write(f"Samples: {stats['count']}\n")
                f.write(f"Mean: {stats['mean']:.6f}\n")
                f.write(f"Median: {stats['median']:.6f}\n")
                f.write(f"Min: {stats['min']:.6f}\n")
                f.write(f"Max: {stats['max']:.6f}\n")
                f.write(f"Std Dev: {stats['std_dev']:.6f}\n")
                f.write(f"Latest: {stats['latest']:.6f}\n")
                f.write(f"Trend: {stats['trend_direction']} ({stats['trend']:.6f})\n")
                f.write("-" * 30 + "\n\n")

        print(f"Performance report saved to {report_path}")

        # Generate visualizations
        self.generate_visualizations(output_dir)

        return analysis

    def generate_visualizations(self, output_dir):
        """Generate performance visualization charts."""
        if not HAS_MATPLOTLIB:
            print("Skipping visualizations - matplotlib not available")
            return

        # Set up matplotlib
        plt.style.use('seaborn-v0_8')
        fig_size = (12, 8)

        # Group data by test
        test_data = defaultdict(lambda: defaultdict(list))

        for key, data in self.data.items():
            if 'values' in data and len(data['values']) > 0:
                test_name, metric = key.rsplit('_', 1)
                test_data[test_name][metric] = data['values']

        # Generate charts for each test
        for test_name, metrics in test_data.items():
            if not metrics:
                continue

            fig, axes = plt.subplots(len(metrics), 1, figsize=fig_size)
            if len(metrics) == 1:
                axes = [axes]

            fig.suptitle(f'Performance Metrics: {test_name}', fontsize=16)

            for i, (metric_name, values) in enumerate(metrics.items()):
                ax = axes[i]

                # Plot time series
                ax.plot(range(len(values)), values, 'b-', linewidth=2, alpha=0.7)
                ax.scatter(range(len(values)), values, c='red', s=30, alpha=0.6)

                # Add trend line if enough data points
                if len(values) > 2:
                    x = list(range(len(values)))
                    slope, intercept = statistics.linear_regression(x, values)[:2]
                    trend_line = [slope * xi + intercept for xi in x]
                    ax.plot(x, trend_line, 'r--', linewidth=1, label=f'Trend (slope: {slope:.6f})')
                    ax.legend()

                ax.set_title(f'{metric_name.replace("_", " ").title()}')
                ax.set_xlabel('Iteration')
                ax.set_ylabel('Value')
                ax.grid(True, alpha=0.3)

            plt.tight_layout()
            chart_path = os.path.join(output_dir, f"{test_name}_metrics.png")
            plt.savefig(chart_path, dpi=150, bbox_inches='tight')
            plt.close()

            print(f"Visualization saved to {chart_path}")

    def generate_dashboard_html(self, output_dir="benchmark_reports"):
        """Generate interactive HTML dashboard."""
        os.makedirs(output_dir, exist_ok=True)

        analysis = self.analyze_performance_trends()

        html_content = f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Benchmark Performance Dashboard</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        body {{
            font-family: Arial, sans-serif;
            margin: 20px;
            background-color: #f5f5f5;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }}
        .header {{
            text-align: center;
            margin-bottom: 30px;
            padding-bottom: 20px;
            border-bottom: 2px solid #333;
        }}
        .metric-card {{
            background: #f8f9fa;
            padding: 15px;
            margin: 10px;
            border-radius: 5px;
            border-left: 4px solid #007bff;
        }}
        .metric-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin: 20px 0;
        }}
        .status-good {{ border-left-color: #28a745; }}
        .status-warning {{ border-left-color: #ffc107; }}
        .status-bad {{ border-left-color: #dc3545; }}
        .chart-container {{
            margin: 20px 0;
            padding: 20px;
            background: white;
            border-radius: 5px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚀 Benchmark Performance Dashboard</h1>
            <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
        </div>

        <h2>📊 Performance Metrics Summary</h2>
        <div class="metric-grid">
"""

        # Add metric cards
        for key, stats in analysis.items():
            test_name, metric = key.rsplit('_', 1)

            # Determine status based on trend
            status_class = "status-good"
            if stats['trend_direction'] == 'degrading':
                status_class = "status-warning"
            elif abs(stats['trend']) > 0.01:  # Significant trend
                status_class = "status-bad"

            html_content += f"""
            <div class="metric-card {status_class}">
                <h3>{test_name} - {metric.replace('_', ' ').title()}</h3>
                <p><strong>Latest:</strong> {stats['latest']:.6f}</p>
                <p><strong>Mean:</strong> {stats['mean']:.6f}</p>
                <p><strong>Trend:</strong> {stats['trend_direction'].title()} ({stats['trend']:+.6f})</p>
                <p><strong>Samples:</strong> {stats['count']}</p>
            </div>
"""

        html_content += """
        </div>

        <h2>📈 Performance Charts</h2>
"""

        # Add charts for each test
        test_data = defaultdict(lambda: defaultdict(list))
        for key, data in self.data.items():
            if 'values' in data and len(data['values']) > 0:
                test_name, metric = key.rsplit('_', 1)
                test_data[test_name][metric] = data['values']

        chart_count = 0
        for test_name, metrics in test_data.items():
            if not metrics:
                continue

            html_content += f"""
        <div class="chart-container">
            <h3>{test_name} Performance Metrics</h3>
            <div id="chart-{chart_count}" style="height: 400px;"></div>
        </div>

        <script>
            var data_{chart_count} = [
"""

            traces = []
            for metric_name, values in metrics.items():
                trace = f"""            {{
                x: {list(range(len(values)))},
                y: {values},
                type: 'scatter',
                mode: 'lines+markers',
                name: '{metric_name.replace("_", " ").title()}',
                line: {{width: 2}}
            }}"""
                traces.append(trace)

            html_content += ",\n".join(traces)
            html_content += f"""
            ];

            var layout_{chart_count} = {{
                title: '{test_name} Performance Trends',
                xaxis: {{title: 'Iteration'}},
                yaxis: {{title: 'Value'}},
                showlegend: true
            }};

            Plotly.newPlot('chart-{chart_count}', data_{chart_count}, layout_{chart_count});
        </script>
"""

            chart_count += 1

        html_content += """
    </div>
</body>
</html>
"""

        dashboard_path = os.path.join(output_dir, "dashboard.html")
        with open(dashboard_path, 'w') as f:
            f.write(html_content)

        print(f"Interactive dashboard saved to {dashboard_path}")

    def compare_with_baseline(self, baseline_file):
        """Compare current results with baseline data."""
        print(f"Comparing with baseline: {baseline_file}")

        if not os.path.exists(baseline_file):
            print("Baseline file not found")
            return {}

        baseline_analyzer = BenchmarkAnalyzer()
        baseline_analyzer.load_timeseries_data(baseline_file)
        baseline_analysis = baseline_analyzer.analyze_performance_trends()

        current_analysis = self.analyze_performance_trends()

        comparison = {}
        for key in set(current_analysis.keys()) | set(baseline_analysis.keys()):
            current = current_analysis.get(key, {})
            baseline = baseline_analysis.get(key, {})

            if current and baseline:
                comparison[key] = {
                    'current_mean': current.get('mean', 0),
                    'baseline_mean': baseline.get('mean', 0),
                    'change': current.get('mean', 0) - baseline.get('mean', 0),
                    'change_percent': ((current.get('mean', 0) - baseline.get('mean', 0)) / baseline.get('mean', 0) * 100) if baseline.get('mean', 0) != 0 else 0
                }

        return comparison

def main():
    parser = argparse.ArgumentParser(description="Enhanced Benchmark Analyzer")
    parser.add_argument("timeseries_file", help="benchmark_timeseries.jsonl file")
    parser.add_argument("--output-dir", default="benchmark_reports",
                       help="Output directory for reports and charts")
    parser.add_argument("--generate-html", action="store_true",
                       help="Generate interactive HTML dashboard")
    parser.add_argument("--compare-baseline", metavar="BASELINE_FILE",
                       help="Compare with baseline timeseries file")

    args = parser.parse_args()

    if not os.path.exists(args.timeseries_file):
        print(f"Error: Timeseries file '{args.timeseries_file}' not found")
        return 1

    analyzer = BenchmarkAnalyzer()
    analyzer.load_timeseries_data(args.timeseries_file)

    # Generate reports
    analyzer.generate_performance_report(args.output_dir)

    if args.generate_html:
        analyzer.generate_dashboard_html(args.output_dir)

    if args.compare_baseline:
        comparison = analyzer.compare_with_baseline(args.compare_baseline)
        if comparison:
            print("\n📊 Baseline Comparison:")
            for key, data in comparison.items():
                test_name, metric = key.rsplit('_', 1)
                print(f"  {test_name} {metric}: {data['change_percent']:+.2f}% "
                      f"({data['current_mean']:.6f} vs {data['baseline_mean']:.6f})")

    print(f"\n✅ Analysis complete. Results saved to {args.output_dir}/")
    return 0

if __name__ == "__main__":
    sys.exit(main())