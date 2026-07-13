#!/usr/bin/env bash
# Fail CI when compiler warnings come from project-owned source files.
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <build-log>" >&2
  exit 2
fi

log="$1"
if [ ! -f "$log" ]; then
  echo "check_first_party_warnings: missing log: $log" >&2
  exit 2
fi

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

# GCC/Clang: path:line[:col]: warning:
# MSVC: path(line[,col]): warning Cxxxx:
grep -E '(^|[[:space:]])([^[:space:]]+):[0-9]+(:[0-9]+)?: warning:|(^|[[:space:]])([^[:space:]]+)\([0-9]+(,[0-9]+)?\): warning ' "$log" > "$tmp" || true

if [ ! -s "$tmp" ]; then
  echo "check_first_party_warnings: no compiler warnings found"
  exit 0
fi

first_party='(^|[[:space:]])(\./|/[^[:space:]]*/)?(engine|runtime|modules|extensions|renderers|tools|examples|tests|cmake|scripts)/'
# Ignore third-party trees and warning lines that only appear via include paths under renderers/
ignored='/(third_party|build[^/[:space:]]*|external|_deps|CMakeFiles|tinyexr)/|(^|[[:space:]])third_party/|/\\.\\./\\.\\./external/'

if grep -E "$first_party" "$tmp" | grep -Ev "$ignored" > "$tmp.first"; then
  echo "check_first_party_warnings: first-party compiler warnings found:" >&2
  cat "$tmp.first" >&2
  rm -f "$tmp.first"
  exit 1
fi
rm -f "$tmp.first"

echo "check_first_party_warnings: warnings found only in ignored third-party/generated paths"
