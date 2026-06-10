#!/usr/bin/env python3
"""Watch studio_exportents.cfg and print when Studio exports new entities (file-bridge IPC)."""
from __future__ import annotations

import os
import sys
import time
from pathlib import Path

# Editor/ is sibling to game.idproj when installed into a mod
ROOT = Path(__file__).resolve().parent.parent
CFG = ROOT / "studio_exportents.cfg"


def main() -> int:
    interval = float(os.environ.get("BRIDGE_WATCH_MS", "500")) / 1000.0
    last_mtime = 0.0
    print(f"[bridge_watch] watching {CFG} (interval {interval}s)")
    while True:
        try:
            if CFG.is_file():
                m = CFG.stat().st_mtime
                if m > last_mtime:
                    last_mtime = m
                    print(f"[bridge_watch] updated: {CFG}")
                    print(CFG.read_text(encoding="utf-8", errors="replace"))
        except KeyboardInterrupt:
            print("\n[bridge_watch] stopped")
            return 0
        time.sleep(interval)


if __name__ == "__main__":
    raise SystemExit(main())
