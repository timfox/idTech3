import os
import sys
import subprocess
import unittest

class BenchJsonToCsvTests(unittest.TestCase):
    def test_include_per_iter_header_and_data(self):
        fixture = os.path.join(os.path.dirname(__file__), "fixtures", "bench_with_iters.json")
        script = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "tools", "bench_json_to_csv.py"))
        proc = subprocess.run([sys.executable, script, fixture, "--include-per-iter"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        out = proc.stdout.strip().splitlines()
        self.assertGreaterEqual(len(out), 2)
        header = out[0].split(",")
        self.assertIn("pathTracer_perIterMs", header)
        self.assertIn("memory_per_iter_mb", header)
        # Data row length should match header length
        data = out[1].split(",")
        self.assertEqual(len(data), len(header))
        # Ensure per-iteration JSON strings exist for the perIter columns
        self.assertTrue(data[8].startswith('["') and data[8].endswith('"]'))
        self.assertTrue(data[9].startswith('["') and data[9].endswith('"]'))

if __name__ == '__main__':
    unittest.main()

