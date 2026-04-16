#!/usr/bin/env bash
# Close stale Cursor-agent PRs superseded by main (VM native tests, run_vulkan, Win32 DLL work).
# Requires: gh auth login with a token that can close PRs (not the read-only integration token).
set -euo pipefail

REPO="${GITHUB_REPO:-timfox/idTech3}"
# PRs #31–#44 (agent batches) and #29 (stale commented-todo); adjust if the list changes.
PRS=(44 41 39 38 37 36 35 34 33 31 29)

COMMENT="Closed: work landed on main (unit_vm_native_module, scripts/run_vulkan.sh + test_run_vulkan_script, Win32 native load diagnostics). Stale duplicate / draft PR."

for n in "${PRS[@]}"; do
  echo "Closing PR #${n}..."
  if gh pr close "$n" -R "$REPO" -c "$COMMENT" 2>/dev/null; then
    echo "  OK #${n}"
  else
    echo "  SKIP/FAIL #${n} (already closed or no permission - run: gh auth login)"
  fi
done

echo "Done. Verify: gh pr list -R $REPO --state open"
