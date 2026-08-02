#!/usr/bin/env python3
"""
Performance Regression Detection Tool for Surf Engine
This script analyzes performance benchmarks and detects regressions
"""

import json
import sys
import os
import subprocess
import argparse
from datetime import datetime
from pathlib import Path
import statistics

class PerformanceRegressionDetector:
    def __init__(self, baseline_file="baseline.json", current_file="current.json"):
        self.baseline_file = baseline_file
        self.current_file = current_file
        self.baseline_data = {}
        self.current_data = {}
        self.regressions = []
        self.improvements = []
        
    def load_baseline(self, filepath=None):
        """Load baseline performance data"""
        if filepath is None:
            filepath = self.baseline_file
            
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                self.baseline_data = json.load(f)
            print(f"Loaded baseline from {filepath}")
            return True
        else:
            print(f"Baseline file not found: {filepath}")
            return False
    
    def load_current(self, filepath=None):
        """Load current performance data"""
        if filepath is None:
            filepath = self.current_file
            
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                self.current_data = json.load(f)
            print(f"Loaded current data from {filepath}")
            return True
        else:
            print(f"Current data file not found: {filepath}")
            return False
    
    def run_benchmarks(self, output_dir="build-perf"):
        """Run performance benchmarks and save results"""
        print("Running performance benchmarks...")
        
        # Ensure output directory exists
        os.makedirs(output_dir, exist_ok=True)
        
        results = {
            "timestamp": datetime.now().isoformat(),
            "benchmarks": {}
        }
        
        # Run movement physics benchmark
        if os.path.exists("build-debug/tests/unit_surfmove"):
            print("  Running movement physics benchmark...")
            result = self._run_benchmark(
                "build-debug/tests/unit_surfmove",
                "--benchmark",
                output_dir
            )
            if result:
                results["benchmarks"]["movement_physics"] = result
        
        # Run BSP collision benchmark
        if os.path.exists("build-debug/tests/unit_surf_trace_ex"):
            print("  Running BSP collision benchmark...")
            result = self._run_benchmark(
                "build-debug/tests/unit_surf_trace_ex",
                "--benchmark",
                output_dir
            )
            if result:
                results["benchmarks"]["bsp_collision"] = result
        
        # Run pmove replay benchmark
        if os.path.exists("build-debug/tests/unit_pmove_replay"):
            print("  Running pmove replay benchmark...")
            result = self._run_benchmark(
                "build-debug/tests/unit_pmove_replay",
                "--benchmark",
                output_dir
            )
            if result:
                results["benchmarks"]["pmove_replay"] = result
        
        # Save current results
        current_file = os.path.join(output_dir, "current.json")
        with open(current_file, 'w') as f:
            json.dump(results, f, indent=2)
        
        print(f"Current results saved to {current_file}")
        return results
    
    def _run_benchmark(self, executable, args, output_dir):
        """Run a single benchmark and return results"""
        try:
            cmd = [executable] + args.split()
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=120
            )
            
            # Parse output for timing information
            output = result.stdout + result.stderr
            
            # Try to extract timing information
            timing = self._extract_timing(output)
            
            if timing:
                return {
                    "executed": True,
                    "output": output,
                    "timing_ms": timing
                }
            else:
                return {
                    "executed": True,
                    "output": output,
                    "timing_ms": None,
                    "warning": "Could not extract timing information"
                }
                
        except subprocess.TimeoutExpired:
            return {
                "executed": False,
                "error": "Benchmark timed out"
            }
        except Exception as e:
            return {
                "executed": False,
                "error": str(e)
            }
    
    def _extract_timing(self, output):
        """Extract timing information from benchmark output"""
        # Look for timing patterns in output
        import re
        
        # Try to find timing in various formats
        patterns = [
            r'(\d+\.?\d*)\s*ms',
            r'(\d+\.?\d*)\s*seconds?',
            r'(\d+\.?\d*)\s*s',
            r'time[:\s]+(\d+\.?\d*)',
            r'duration[:\s]+(\d+\.?\d*)'
        ]
        
        for pattern in patterns:
            match = re.search(pattern, output, re.IGNORECASE)
            if match:
                try:
                    value = float(match.group(1))
                    # Convert to milliseconds if in seconds
                    if value < 10 and 'seconds' in output.lower():
                        return value * 1000
                    return value
                except ValueError:
                    continue
        
        return None
    
    def compare_benchmarks(self):
        """Compare current benchmarks against baseline"""
        print("Comparing benchmarks...")
        
        if not self.baseline_data or not self.current_data:
            print("Error: Must load both baseline and current data first")
            return False
        
        baseline_benchmarks = self.baseline_data.get("benchmarks", {})
        current_benchmarks = self.current_data.get("benchmarks", {})
        
        for benchmark_name, current_benchmark in current_benchmarks.items():
            if benchmark_name not in baseline_benchmarks:
                print(f"  New benchmark: {benchmark_name}")
                continue
            
            baseline_benchmark = baseline_benchmarks[benchmark_name]
            
            # Compare timing metrics
            current_timing = current_benchmark.get("timing_ms")
            baseline_timing = baseline_benchmark.get("timing_ms")
            
            if current_timing is not None and baseline_timing is not None:
                self._compare_timing(
                    benchmark_name,
                    baseline_timing,
                    current_timing
                )
        
        return True
    
    def _compare_timing(self, benchmark_name, baseline, current):
        """Compare timing between baseline and current"""
        if baseline == 0:
            return
            
        # Calculate percentage change
        change_pct = ((current - baseline) / baseline) * 100
        
        # Define thresholds (configurable)
        regression_warning = 10  # 10% slower is a warning
        regression_critical = 20  # 20% slower is critical
        improvement_warning = 5  # 5% faster is a minor improvement
        improvement_critical = 15  # 15% faster is significant improvement
        
        if change_pct > regression_warning:
            severity = "critical" if change_pct > regression_critical else "warning"
            self.regressions.append({
                "benchmark": benchmark_name,
                "baseline_ms": baseline,
                "current_ms": current,
                "change_pct": change_pct,
                "severity": severity
            })
            print(f"  ⚠️  REGRESSION: {benchmark_name} (+{change_pct:.1f}%)")
        elif change_pct < -improvement_warning:
            severity = "critical" if change_pct < -improvement_critical else "warning"
            self.improvements.append({
                "benchmark": benchmark_name,
                "baseline_ms": baseline,
                "current_ms": current,
                "change_pct": change_pct,
                "severity": severity
            })
            print(f"  ✅ IMPROVEMENT: {benchmark_name} ({change_pct:.1f}%)")
        else:
            print(f"  ✓ OK: {benchmark_name} ({change_pct:+.1f}%)")
    
    def generate_report(self, output_file="performance_report.json"):
        """Generate a comprehensive performance report"""
        report = {
            "timestamp": datetime.now().isoformat(),
            "summary": {
                "total_benchmarks": len(self.current_data.get("benchmarks", {})),
                "regressions": len(self.regressions),
                "improvements": len(self.improvements),
                "stable": len(self.current_data.get("benchmarks", {})) - len(self.regressions) - len(self.improvements)
            },
            "regressions": self.regressions,
            "improvements": self.improvements,
            "details": {
                "baseline": self.baseline_data,
                "current": self.current_data
            }
        }
        
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"\nPerformance report saved to {output_file}")
        return report
    
    def print_summary(self):
        """Print a human-readable summary"""
        print("\n" + "="*60)
        print("PERFORMANCE REGRESSION DETECTION SUMMARY")
        print("="*60)
        
        print(f"\nBenchmarks analyzed: {len(self.current_data.get('benchmarks', {}))}")
        print(f"Regressions found: {len(self.regressions)}")
        print(f"Improvements found: {len(self.improvements)}")
        
        if self.regressions:
            print("\n⚠️  REGRESSIONS:")
            for reg in self.regressions:
                print(f"  - {reg['benchmark']}: +{reg['change_pct']:.1f}% "
                      f"({reg['baseline_ms']:.1f}ms → {reg['current_ms']:.1f}ms) "
                      f"[{reg['severity']}]")
        
        if self.improvements:
            print("\n✅ IMPROVEMENTS:")
            for imp in self.improvements:
                print(f"  - {imp['benchmark']}: {imp['change_pct']:.1f}% "
                      f"({imp['baseline_ms']:.1f}ms → {imp['current_ms']:.1f}ms) "
                      f"[{imp['severity']}]")
        
        if not self.regressions and not self.improvements:
            print("\n✓ All benchmarks are within acceptable thresholds!")
        
        print("="*60)
        
        # Return exit code based on findings
        if self.regressions:
            return 1  # Regressions found
        return 0  # No regressions


def main():
    parser = argparse.ArgumentParser(
        description="Performance Regression Detection Tool for Surf Engine"
    )
    parser.add_argument(
        "--baseline", "-b",
        default="baseline.json",
        help="Baseline performance data file"
    )
    parser.add_argument(
        "--current", "-c",
        default="current.json",
        help="Current performance data file"
    )
    parser.add_argument(
        "--run-benchmarks", "-r",
        action="store_true",
        help="Run benchmarks before comparison"
    )
    parser.add_argument(
        "--output", "-o",
        default="performance_report.json",
        help="Output report file"
    )
    parser.add_argument(
        "--output-dir",
        default="build-perf",
        help="Output directory for benchmarks"
    )
    
    args = parser.parse_args()
    
    detector = PerformanceRegressionDetector(
        baseline_file=args.baseline,
        current_file=args.current
    )
    
    # Load baseline data
    if not detector.load_baseline():
        print("Warning: No baseline data found. This may be the first run.")
    
    # Run benchmarks if requested
    if args.run_benchmarks:
        detector.run_benchmarks(output_dir=args.output_dir)
    
    # Load current data
    if not detector.load_current():
        print("Error: No current data found. Run benchmarks first with -r flag.")
        sys.exit(1)
    
    # Compare benchmarks
    detector.compare_benchmarks()
    
    # Generate report
    report = detector.generate_report(output_file=args.output)
    
    # Print summary
    exit_code = detector.print_summary()
    
    sys.exit(exit_code)


if __name__ == "__main__":
    main()