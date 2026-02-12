#!/usr/bin/env python3
"""
SPDX/license header scanner for the idTech3 source tree.

This script walks the prose under src/ (excluding third-party trees) and ensures every
tracked C/C++/CMake file already advertises either an SPDX identifier or the classic
GNU General Public License notice.
"""

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LICENSE_HINTS = ("SPDX-License-Identifier:", "GNU General Public License")
CHECK_EXTENSIONS = {
    ".c",
    ".cpp",
    ".cc",
    ".cxx",
    ".h",
    ".hpp",
    ".hh",
    ".inl",
    ".cmake",
    ".glsl",
    ".vert",
    ".frag",
    ".geom",
    ".txt",
    ".py",
    ".sh",
}

EXCLUDED_PREFIXES = (
    ROOT / ".git",
    ROOT / "content",
    ROOT / "release",
    ROOT / "build-vk-Release",
    ROOT / "build-gl-Release",
    ROOT / "build-coverage",
    ROOT / "src" / "external",
    ROOT / "src" / "renderers" / "vulkanrenderer" / "shaders",
    ROOT / "src" / "renderers" / "rendercommon",
    ROOT / "src" / "audio",
    ROOT / "src" / "tools",
)


def should_check(path: Path) -> bool:
    if path.suffix.lower() not in CHECK_EXTENSIONS:
        return False
    for prefix in EXCLUDED_PREFIXES:
        try:
            path.relative_to(prefix)
        except ValueError:
            continue
        return False
    try:
        path.relative_to(ROOT / ".git")
        return False
    except ValueError:
        pass
    return True


def git_status_changed(root: Path) -> set:
    files = set()
    try:
        proc = subprocess.run(
            ["git", "status", "--porcelain"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=True,
            cwd=root,
            text=True,
        )
    except subprocess.CalledProcessError:
        return files

    for line in proc.stdout.splitlines():
        if not line:
            continue
        path = line[3:]
        if "->" in path:
            path = path.split("->")[-1].strip()
        files.add((root / path.strip()).resolve())
    return files


def git_diff_from_base(root: Path) -> set:
    files = set()
    base_refs = ("origin/main", "origin/master", "main", "master")
    base = None
    for ref in base_refs:
        try:
            base = (
                subprocess.run(
                    ["git", "merge-base", "HEAD", ref],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.DEVNULL,
                    check=True,
                    cwd=root,
                    text=True,
                )
                .stdout.strip()
            )
        except subprocess.CalledProcessError:
            continue
        if base:
            break

    if not base:
        try:
            base = (
                subprocess.run(
                    ["git", "rev-parse", "HEAD^"],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.DEVNULL,
                    check=True,
                    cwd=root,
                    text=True,
                )
                .stdout.strip()
            )
        except subprocess.CalledProcessError:
            return files

    try:
        diff = subprocess.run(
            ["git", "diff", "--name-only", base, "HEAD"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=True,
            cwd=root,
            text=True,
        )
    except subprocess.CalledProcessError:
        return files

    for entry in diff.stdout.splitlines():
        if not entry:
            continue
        files.add((root / entry.strip()).resolve())
    return files


def gather_candidate_files(root: Path) -> set:
    candidates = git_status_changed(root)
    candidates.update(git_diff_from_base(root))
    return candidates


def look_for_license(file_path: Path) -> bool:
    try:
        with file_path.open("r", errors="ignore") as fh:
            header_lines = []
            for _ in range(20):
                line = fh.readline()
                if not line:
                    break
                header_lines.append(line)
    except Exception as exc:
        print(f"spdx_scan: failed to read {file_path}: {exc}", file=sys.stderr)
        return False

    header = "".join(header_lines)
    return any(hint in header for hint in LICENSE_HINTS)


def main():
    parser = argparse.ArgumentParser(description="Quick SPDX/license scanner")
    parser.add_argument(
        "--path",
        type=Path,
        default=ROOT,
        help="Repository root to scan (defaults to repository root)",
    )
    args = parser.parse_args()
    repo = args.path.resolve()

    candidates = gather_candidate_files(repo)
    if not candidates:
        print("spdx_scan: no modified files detected; skipping license check")
        return

    missing = []
    for candidate in sorted(candidates):
        if not candidate.exists():
            continue
        if not should_check(candidate):
            continue
        if not look_for_license(candidate):
            missing.append(candidate.relative_to(repo))

    if missing:
        print("spdx_scan: warning - files missing SPDX/GPL hints:")
        for entry in missing:
            print(f"  {entry}")
        sys.exit(1)

    print("spdx_scan: SPDX/license scan passed")


if __name__ == "__main__":
    main()
