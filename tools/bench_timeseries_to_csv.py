#!/usr/bin/env python3
"""
Benchmark Time-Series to CSV Converter
Converts benchmark_timeseries.jsonl to structured CSV files for analysis.

Usage:
  python3 tools/bench_timeseries_to_csv.py benchmark_timeseries.jsonl [--output-dir <dir>]
"""

import json
import csv
import sys
import os
import argparse
from datetime import datetime
from collections import defaultdict

def main():
    parser = argparse.ArgumentParser(description="Convert benchmark time-series to CSV")
    parser.add_argument("timeseries_file", help="benchmark_timeseries.jsonl file")
    parser.add_argument("--output-dir", default="benchmark_csv",
                       help="Output directory for CSV files")

    args = parser.parse_args()

    if not os.path.exists(args.timeseries_file):
        print(f"Error: Timeseries file '{args.timeseries_file}' not found")
        return 1

    os.makedirs(args.output_dir, exist_ok=True)

    # Load and organize data
    data_by_test_metric = defaultdict(list)

    with open(args.timeseries_file, 'r') as f:
        for line in f:
            try:
                entry = json.loads(line.strip())
                test = entry['test']
                metric = entry['metric']
                key = f"{test}_{metric}"

                # Add iteration info if present
                if 'iteration' in entry:
                    entry_copy = entry.copy()
                    data_by_test_metric[key].append(entry_copy)
                else:
                    # Aggregate metrics
                    data_by_test_metric[key].append(entry)

            except json.JSONDecodeError as e:
                print(f"Warning: Failed to parse line: {e}")
                continue

    # Generate CSV files for each test/metric combination
    for key, entries in data_by_test_metric.items():
        if not entries:
            continue

        test_name, metric_name = key.rsplit('_', 1)

        # Sort by timestamp
        entries.sort(key=lambda x: x['timestamp'])

        csv_filename = f"{test_name}_{metric_name}.csv"
        csv_path = os.path.join(args.output_dir, csv_filename)

        with open(csv_path, 'w', newline='') as csvfile:
            fieldnames = ['timestamp', 'value']
            if 'iteration' in entries[0]:
                fieldnames.insert(1, 'iteration')

            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()

            for entry in entries:
                row = {
                    'timestamp': entry['timestamp'],
                    'value': entry['value']
                }
                if 'iteration' in entry:
                    row['iteration'] = entry['iteration']

                writer.writerow(row)

        print(f"Generated CSV: {csv_path} ({len(entries)} entries)")

    # Generate summary CSV
    summary_path = os.path.join(args.output_dir, "benchmark_summary.csv")

    # Calculate summary statistics
    summary_data = defaultdict(lambda: {'count': 0, 'sum': 0, 'min': float('inf'), 'max': float('-inf')})

    for key, entries in data_by_test_metric.items():
        if not entries:
            continue

        test_name, metric_name = key.rsplit('_', 1)

        for entry in entries:
            value = entry['value']
            summary_data[f"{test_name}_{metric_name}"]['count'] += 1
            summary_data[f"{test_name}_{metric_name}"]['sum'] += value
            summary_data[f"{test_name}_{metric_name}"]['min'] = min(summary_data[f"{test_name}_{metric_name}"]['min'], value)
            summary_data[f"{test_name}_{metric_name}"]['max'] = max(summary_data[f"{test_name}_{metric_name}"]['max'], value)

    with open(summary_path, 'w', newline='') as csvfile:
        fieldnames = ['test_metric', 'count', 'mean', 'min', 'max', 'timestamp']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()

        for key, stats in summary_data.items():
            writer.writerow({
                'test_metric': key,
                'count': stats['count'],
                'mean': stats['sum'] / stats['count'] if stats['count'] > 0 else 0,
                'min': stats['min'],
                'max': stats['max'],
                'timestamp': datetime.now().strftime('%Y-%m-%dT%H:%M:%SZ')
            })

    print(f"Generated summary CSV: {summary_path}")

    print(f"\n✅ CSV conversion complete. Files saved to {args.output_dir}/")
    return 0

if __name__ == "__main__":
    sys.exit(main())