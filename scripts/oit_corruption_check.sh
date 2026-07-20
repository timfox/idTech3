#!/usr/bin/env bash
# Static gate: OIT corruption fix (WBOIT lifecycle / resolve layout).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fail=0

need() {
  local f="$1"
  if [[ ! -f "$ROOT/$f" ]]; then
    echo "FAIL missing: $f"
    fail=1
  else
    echo "OK  $f"
  fi
}

need "config/repro_oit_corruption.cfg"
need "config/demo_oit_isolation.cfg"
need "scripts/repro_oit_corruption.sh"
need "docs/MOMENT_OIT_STOCHASTIC_ALPHA.md"

CFG="$ROOT/config/repro_oit_corruption.cfg"
grep -q 'seta r_oit 1' "$CFG" || { echo "FAIL r_oit 1"; fail=1; }
grep -q 'seta r_taa 0' "$CFG" || { echo "FAIL TAA off for isolation"; fail=1; }
grep -q 'NOT boot default\|not.*boot default' "$CFG" || { echo "FAIL must label non-default"; fail=1; }

# Resolve must use UNDEFINED discard path (not COLOR_ATTACHMENT DONT_CARE race)
if grep -n 'OIT resolve: discard prior color' -A12 "$ROOT/renderers/vulkan/vk_render_pass.c" | grep -q 'VK_IMAGE_LAYOUT_UNDEFINED'; then
  echo "OK  oit_resolve initialLayout UNDEFINED"
else
  echo "FAIL oit_resolve must use UNDEFINED initialLayout"
  fail=1
fi
if grep -n 'OIT resolve: discard prior color' -A12 "$ROOT/renderers/vulkan/vk_render_pass.c" | grep -q 'SHADER_READ_ONLY_OPTIMAL'; then
  echo "OK  oit_resolve finalLayout SHADER_READ"
else
  echo "FAIL oit_resolve finalLayout must be SHADER_READ_ONLY"
  fail=1
fi

grep -q 'VK_OIT_FRAME_UNTOUCHED' "$ROOT/renderers/vulkan/vk.h" || { echo "FAIL frame state missing"; fail=1; }
grep -q 'oit_capture' "$ROOT/renderers/vulkan/vk_transparency_route.c" || { echo "FAIL oit_capture cmd"; fail=1; }
grep -q 'r_oitDirectTest' "$ROOT/renderers/vulkan/tr_init.c" || { echo "FAIL r_oitDirectTest"; fail=1; }
grep -q 'weapon isolation' "$ROOT/renderers/vulkan/tr_backend.c" || { echo "FAIL weapon isolation log"; fail=1; }

# Boot must not force OIT corruption repro
for cfg in modern_vulkan.cfg modern_vulkan_stable.cfg gfx_safe.cfg; do
  if [[ -f "$ROOT/config/$cfg" ]]; then
    if grep -E '^[[:space:]]*exec[[:space:]]+repro_oit_corruption\.cfg' "$ROOT/config/$cfg" >/dev/null 2>&1; then
      echo "FAIL $cfg must not exec repro_oit_corruption"
      fail=1
    else
      echo "OK  $cfg does not exec repro"
    fi
  fi
done

# Single generation bump site after FB (attachments must not also bump)
att_bumps=$(grep -c 'oitAttachmentGeneration++' "$ROOT/renderers/vulkan/vk_attachments.c" || true)
fb_bumps=$(grep -c 'oitAttachmentGeneration++' "$ROOT/renderers/vulkan/vk_framebuffers.c" || true)
if [[ "$att_bumps" -eq 0 && "$fb_bumps" -eq 1 ]]; then
  echo "OK  single oitAttachmentGeneration bump in framebuffers"
else
  echo "FAIL expected 0 bumps in attachments, 1 in framebuffers (got att=$att_bumps fb=$fb_bumps)"
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  echo "oit_corruption_check: FAIL"
  exit 1
fi
echo "oit_corruption_check: PASS (static)"
exit 0
