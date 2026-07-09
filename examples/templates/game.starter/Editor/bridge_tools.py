#!/usr/bin/env python3
"""Starter Editor script — run from Radiant Python Script Editor after install_radiant_gamepack."""
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
bridge = here / "bridge_tools.py"
if bridge.is_file():
    sys.path.insert(0, str(here))
    import bridge_tools
    raise SystemExit(bridge_tools.main())

print("[Editor] bridge_tools.py not installed — run ./scripts/install_radiant_gamepack.sh")
