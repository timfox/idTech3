#!/usr/bin/env python3
"""
Enhanced Benchmark Analysis and Visualization Tool

Parses benchmark data from bench_timeseries.jsonl, bench.json, and bench.csv
Generates visualizations, reports, and CI/CD integration.
"""

import json
import csv
import argparse
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
from pathlib import Path
import numpy as np
from datetime import datetime
import sys

class BenchmarkAnalyzer:
    def __init__(self, bench_dir="tests/benchmarks"):
        self.bench_dir = Path(bench_dir)
        self.data = {}

    def load_timeseries_data(self):
        """Load time-series data from bench_timeseries.jsonl"""
        timeseries_file = self.bench_dir / "bench_timeseries.jsonl"
        if not timeseries_file.exists():
            print(f"Warning: {timeseries_file} not found")
            return pd.DataFrame()

        records = []
        with open(timeseries_file, 'r') as f:
            for line in f:
                if line.strip():
                    records.append(json.loads(line))

        if records:
            df = pd.DataFrame(records)
            df['timestamp'] = pd.to_datetime(df['timestamp'], unit='ms')
            return df
        return pd.DataFrame()

    def load_summary_data(self):
        """Load summary data from bench.json"""
        json_file = self.bench_dir / "bench.json"
        if not json_file.exists():
            print(f"Warning: {json_file} not found")
            return {}

        with open(json_file, 'r') as f:
            return json.load(f)

    def load_csv_data(self):
        """Load CSV data for historical comparison"""
        csv_file = self.bench_dir / "bench.csv"
        if not csv_file.exists():
            print(f"Warning: {csv_file} not found")
            return pd.DataFrame()

        return pd.read_csv(csv_file)

    def generate_performance_report(self, output_dir="bench_reports"):
        """Generate comprehensive performance report with visualizations"""
        output_dir = Path(output_dir)
        output_dir.mkdir(exist_ok=True)

        # Load all data
        timeseries_df = self.load_timeseries_data()
        summary_data = self.load_summary_data()
        csv_df = self.load_csv_data()

        if timeseries_df.empty and not summary_data:
            print("No benchmark data found to analyze")
            return

        # Generate visualizations
        self._generate_timeseries_plots(timeseries_df, output_dir)
        self._generate_summary_plots(summary_data, output_dir)
        self._generate_comparison_plots(csv_df, output_dir)

        # Generate HTML report
        self._generate_html_report(summary_data, timeseries_df, output_dir)

        print(f"Performance report generated in {output_dir}")

    def _generate_timeseries_plots(self, df, output_dir):
        """Generate time-series plots for detailed analysis"""
        if df.empty:
            return

        # Set up the plotting style
        plt.style.use('seaborn-v0_8')
        fig, axes = plt.subplots(2, 2, figsize=(15, 10))
        fig.suptitle('Benchmark Time-Series Analysis', fontsize=16)

        # Duration over time by operation
        if 'operation' in df.columns:
            operations = df['operation'].unique()
            for op in operations:
                op_data = df[df['operation'] == op]
                axes[0, 0].plot(op_data['timestamp'], op_data['duration_ms'],
                               label=op, marker='o', markersize=2, alpha=0.7)
            axes[0, 0].set_title('Operation Duration Over Time')
            axes[0, 0].set_xlabel('Time')
            axes[0, 0].set_ylabel('Duration (ms)')
            axes[0, 0].legend()
            axes[0, 0].tick_params(axis='x', rotation=45)

        # Memory usage over time
        if 'memory_mb' in df.columns:
            axes[0, 1].plot(df['timestamp'], df['memory_mb'],
                           color='green', marker='s', markersize=2, alpha=0.7)
            axes[0, 1].set_title('Memory Usage Over Time')
            axes[0, 1].set_xlabel('Time')
            axes[0, 1].set_ylabel('Memory (MB)')
            axes[0, 1].tick_params(axis='x', rotation=45)

        # Duration distribution by operation
        if 'operation' in df.columns and 'duration_ms' in df.columns:
            df.boxplot(column='duration_ms', by='operation', ax=axes[1, 0])
            axes[1, 0].set_title('Duration Distribution by Operation')
            axes[1, 0].set_xlabel('Operation')
            axes[1, 0].set_ylabel('Duration (ms)')

        # Performance stability (coefficient of variation)
        if 'operation' in df.columns and 'duration_ms' in df.columns:
            stability_data = []
            operations = df['operation'].unique()
            for op in operations:
                op_data = df[df['operation'] == op]['duration_ms']
                if len(op_data) > 1:
                    cv = op_data.std() / op_data.mean() * 100  # Coefficient of variation as percentage
                    stability_data.append({'operation': op, 'cv_percent': cv})

            if stability_data:
                stab_df = pd.DataFrame(stability_data)
                axes[1, 1].bar(stab_df['operation'], stab_df['cv_percent'],
                              color='orange', alpha=0.7)
                axes[1, 1].set_title('Performance Stability (Lower CV = More Stable)')
                axes[1, 1].set_xlabel('Operation')
                axes[1, 1].set_ylabel('Coefficient of Variation (%)')
                axes[1, 1].tick_params(axis='x', rotation=45)

        plt.tight_layout()
        plt.savefig(output_dir / 'timeseries_analysis.png', dpi=300, bbox_inches='tight')
        plt.close()

    def _generate_summary_plots(self, summary_data, output_dir):
        """Generate summary statistics plots"""
        if not summary_data:
            return

        plt.style.use('seaborn-v0_8')

        # Extract operation data
        operations = ['pathTracer', 'rtx', 'denoiser', 'fsr']
        metrics = ['avg_ms', 'min_ms', 'max_ms', 'median_ms', 'stddev_ms']

        # Create comparison plot
        fig, axes = plt.subplots(2, 2, figsize=(15, 10))
        fig.suptitle('Benchmark Summary Statistics', fontsize=16)

        # Average performance comparison
        avg_times = []
        op_names = []
        for op in operations:
            if op in summary_data:
                avg_times.append(summary_data[op].get('avg_ms', 0))
                op_names.append(op)

        if avg_times:
            axes[0, 0].bar(op_names, avg_times, color='skyblue', alpha=0.8)
            axes[0, 0].set_title('Average Execution Time by Operation')
            axes[0, 0].set_xlabel('Operation')
            axes[0, 0].set_ylabel('Time (ms)')
            axes[0, 0].tick_params(axis='x', rotation=45)

        # Performance consistency (stddev)
        stddev_times = []
        for op in operations:
            if op in summary_data:
                stddev_times.append(summary_data[op].get('stddev_ms', 0))

        if stddev_times:
            axes[0, 1].bar(op_names, stddev_times, color='salmon', alpha=0.8)
            axes[0, 1].set_title('Performance Variability (Standard Deviation)')
            axes[0, 1].set_xlabel('Operation')
            axes[0, 1].set_ylabel('StdDev (ms)')
            axes[0, 1].tick_params(axis='x', rotation=45)

        # Percentiles comparison
        p95_times = []
        p99_times = []
        for op in operations:
            if op in summary_data:
                p95_times.append(summary_data[op].get('p95_ms', 0))
                p99_times.append(summary_data[op].get('p99_ms', 0))

        if p95_times and p99_times:
            x = np.arange(len(op_names))
            width = 0.35
            axes[1, 0].bar(x - width/2, p95_times, width, label='P95', color='lightgreen', alpha=0.8)
            axes[1, 0].bar(x + width/2, p99_times, width, label='P99', color='darkgreen', alpha=0.8)
            axes[1, 0].set_title('Percentile Performance (P95/P99)')
            axes[1, 0].set_xlabel('Operation')
            axes[1, 0].set_ylabel('Time (ms)')
            axes[1, 0].set_xticks(x)
            axes[1, 0].set_xticklabels(op_names, rotation=45)
            axes[1, 0].legend()

        # Memory usage
        if 'memory_start_mb' in summary_data and 'memory_end_mb' in summary_data:
            memory_labels = ['Start', 'End']
            memory_values = [summary_data['memory_start_mb'], summary_data['memory_end_mb']]
            axes[1, 1].bar(memory_labels, memory_values, color='purple', alpha=0.8)
            axes[1, 1].set_title('Memory Usage')
            axes[1, 1].set_xlabel('Benchmark Phase')
            axes[1, 1].set_ylabel('Memory (MB)')

        plt.tight_layout()
        plt.savefig(output_dir / 'summary_statistics.png', dpi=300, bbox_inches='tight')
        plt.close()

    def _generate_comparison_plots(self, csv_df, output_dir):
        """Generate historical comparison plots"""
        if csv_df.empty or len(csv_df) < 2:
            return

        plt.style.use('seaborn-v0_8')

        # Convert timestamp to datetime
        csv_df['timestamp'] = pd.to_datetime(csv_df['timestamp'])

        # Create comparison plots for key metrics
        fig, axes = plt.subplots(2, 2, figsize=(15, 10))
        fig.suptitle('Historical Benchmark Comparison', fontsize=16)

        operations = ['pathTracer', 'rtx', 'denoiser', 'fsr']

        for i, op in enumerate(operations):
            avg_col = f'{op}_avg_ms'
            if avg_col in csv_df.columns:
                ax = axes[i // 2, i % 2]
                ax.plot(csv_df['timestamp'], csv_df[avg_col],
                       marker='o', linestyle='-', linewidth=2, markersize=4)
                ax.set_title(f'{op.title()} Performance Trend')
                ax.set_xlabel('Time')
                ax.set_ylabel('Average Time (ms)')
                ax.tick_params(axis='x', rotation=45)

                # Add trend line if we have enough data points
                if len(csv_df) >= 3:
                    z = np.polyfit(range(len(csv_df)), csv_df[avg_col], 1)
                    p = np.poly1d(z)
                    ax.plot(csv_df['timestamp'], p(range(len(csv_df))),
                           "r--", alpha=0.8, label='Trend')
                    ax.legend()

        plt.tight_layout()
        plt.savefig(output_dir / 'historical_comparison.png', dpi=300, bbox_inches='tight')
        plt.close()

    def _generate_html_report(self, summary_data, timeseries_df, output_dir):
        """Generate comprehensive HTML report"""
        html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <title>idTech3++ Benchmark Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 40px; }}
        .header {{ background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 20px; border-radius: 10px; margin-bottom: 30px; }}
        .section {{ margin: 20px 0; padding: 20px; border: 1px solid #ddd; border-radius: 5px; }}
        .metric {{ display: inline-block; margin: 10px; padding: 10px; background: #f5f5f5; border-radius: 5px; min-width: 150px; }}
        .metric-value {{ font-size: 24px; font-weight: bold; color: #2c3e50; }}
        .metric-label {{ font-size: 12px; color: #7f8c8d; text-transform: uppercase; }}
        .chart {{ margin: 20px 0; text-align: center; }}
        .chart img {{ max-width: 100%; border-radius: 5px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }}
        table {{ width: 100%; border-collapse: collapse; margin: 20px 0; }}
        th, td {{ padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }}
        th {{ background-color: #f8f9fa; font-weight: bold; }}
        .status-good {{ color: #27ae60; }}
        .status-warning {{ color: #f39c12; }}
        .status-bad {{ color: #e74c3c; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>🚀 idTech3++ Enhanced Benchmark Report</h1>
        <p>Generated on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
    </div>

    <div class="section">
        <h2>📊 Executive Summary</h2>
"""

        if summary_data:
            duration = summary_data.get('duration_seconds', 0)
            memory_start = summary_data.get('memory_start_mb', 0)
            memory_end = summary_data.get('memory_end_mb', 0)

            html_content += f"""
        <div class="metric">
            <div class="metric-value">{duration:.2f}s</div>
            <div class="metric-label">Total Duration</div>
        </div>
        <div class="metric">
            <div class="metric-value">{memory_end:.1f}MB</div>
            <div class="metric-label">Peak Memory</div>
        </div>
        <div class="metric">
            <div class="metric-value">{memory_end - memory_start:+.1f}MB</div>
            <div class="metric-label">Memory Delta</div>
        </div>
"""

        html_content += """
    </div>

    <div class="section">
        <h2>📈 Performance Metrics</h2>
        <table>
            <tr>
                <th>Operation</th>
                <th>Avg Time (ms)</th>
                <th>Min Time (ms)</th>
                <th>Max Time (ms)</th>
                <th>Median (ms)</th>
                <th>StdDev (ms)</th>
                <th>P95 (ms)</th>
                <th>P99 (ms)</th>
                <th>Stability</th>
            </tr>
"""

        operations = ['pathTracer', 'rtx', 'denoiser', 'fsr']
        for op in operations:
            if op in summary_data:
                data = summary_data[op]
                avg = data.get('avg_ms', 0)
                stddev = data.get('stddev_ms', 0)
                cv = (stddev / avg * 100) if avg > 0 else 0

                stability_class = "status-good" if cv < 10 else "status-warning" if cv < 25 else "status-bad"
                stability_text = "Excellent" if cv < 10 else "Good" if cv < 25 else "Poor"

                html_content += f"""
            <tr>
                <td>{op.title()}</td>
                <td>{avg:.3f}</td>
                <td>{data.get('min_ms', 0):.3f}</td>
                <td>{data.get('max_ms', 0):.3f}</td>
                <td>{data.get('median_ms', 0):.3f}</td>
                <td>{stddev:.3f}</td>
                <td>{data.get('p95_ms', 0):.3f}</td>
                <td>{data.get('p99_ms', 0):.3f}</td>
                <td class="{stability_class}">{stability_text} ({cv:.1f}%)</td>
            </tr>"""

        html_content += """
        </table>
    </div>

    <div class="section">
        <h2>📊 Time-Series Analysis</h2>
        <div class="chart">
            <img src="timeseries_analysis.png" alt="Time-Series Analysis">
        </div>
    </div>

    <div class="section">
        <h2>📈 Summary Statistics</h2>
        <div class="chart">
            <img src="summary_statistics.png" alt="Summary Statistics">
        </div>
    </div>

    <div class="section">
        <h2>📉 Historical Trends</h2>
        <div class="chart">
            <img src="historical_comparison.png" alt="Historical Comparison">
        </div>
    </div>

    <div class="section">
        <h2>🔍 Data Sources</h2>
        <ul>
            <li><strong>bench_timeseries.jsonl</strong>: Per-iteration timing and memory data</li>
            <li><strong>bench.json</strong>: Detailed statistics and percentiles</li>
            <li><strong>bench.csv</strong>: Historical comparison data</li>
            <li><strong>bench_summary.json</strong>: High-level summary metrics</li>
        </ul>
    </div>
</body>
</html>"""

        with open(output_dir / 'benchmark_report.html', 'w') as f:
            f.write(html_content)

    def run_ci_analysis(self):
        """Run CI-specific analysis and generate reports for automated testing"""
        print("Running CI analysis...")

        # Load current benchmark data
        summary_data = self.load_summary_data()

        if not summary_data:
            print("No benchmark data available for CI analysis")
            return False

        # Check for performance regressions
        regressions = self._check_performance_regressions()

        # Generate CI report
        ci_report = {
            "timestamp": datetime.now().isoformat(),
            "status": "success" if not regressions else "warning",
            "summary": summary_data,
            "regressions": regressions,
            "recommendations": self._generate_recommendations(summary_data)
        }

        # Write CI report
        with open('bench_results.json', 'w') as f:
            json.dump(ci_report, f, indent=2)

        print(f"CI analysis complete. Status: {ci_report['status']}")
        if regressions:
            print(f"Performance regressions detected: {len(regressions)}")

        return len(regressions) == 0

    def _check_performance_regressions(self):
        """Check for performance regressions against historical data"""
        csv_df = self.load_csv_data()
        if csv_df.empty or len(csv_df) < 2:
            return []

        regressions = []
        operations = ['pathTracer', 'rtx', 'denoiser', 'fsr']

        # Get the latest and previous runs
        latest = csv_df.iloc[-1]
        previous = csv_df.iloc[-2] if len(csv_df) > 1 else None

        if previous is not None:
            for op in operations:
                avg_col = f'{op}_avg_ms'
                if avg_col in latest and avg_col in previous:
                    current_avg = latest[avg_col]
                    prev_avg = previous[avg_col]

                    # Check for 10%+ regression
                    if current_avg > prev_avg * 1.1:
                        regression_pct = ((current_avg - prev_avg) / prev_avg) * 100
                        regressions.append({
                            "operation": op,
                            "regression_percent": regression_pct,
                            "previous_avg": prev_avg,
                            "current_avg": current_avg
                        })

        return regressions

    def _generate_recommendations(self, summary_data):
        """Generate performance recommendations based on benchmark data"""
        recommendations = []

        operations = ['pathTracer', 'rtx', 'denoiser', 'fsr']
        for op in operations:
            if op in summary_data:
                data = summary_data[op]
                avg_time = data.get('avg_ms', 0)
                stddev = data.get('stddev_ms', 0)
                cv = (stddev / avg_time * 100) if avg_time > 0 else 0

                # Performance recommendations
                if avg_time > 10.0:
                    recommendations.append(f"Consider optimizing {op} (avg: {avg_time:.2f}ms)")

                # Stability recommendations
                if cv > 25:
                    recommendations.append(f"Improve stability of {op} (CV: {cv:.1f}%)")

        return recommendations

def main():
    parser = argparse.ArgumentParser(description='Enhanced Benchmark Analysis Tool')
    parser.add_argument('--bench-dir', default='tests/benchmarks', help='Benchmark data directory')
    parser.add_argument('--output-dir', default='bench_reports', help='Output directory for reports')
    parser.add_argument('--ci', action='store_true', help='Run CI analysis mode')
    parser.add_argument('--html-only', action='store_true', help='Generate HTML report only')

    args = parser.parse_args()

    analyzer = BenchmarkAnalyzer(args.bench_dir)

    if args.ci:
        success = analyzer.run_ci_analysis()
        sys.exit(0 if success else 1)
    elif args.html_only:
        analyzer.generate_performance_report(args.output_dir)
    else:
        analyzer.generate_performance_report(args.output_dir)
        print(f"\nReports generated in {args.output_dir}")
        print("Open benchmark_report.html for detailed analysis")

if __name__ == '__main__':
    main()