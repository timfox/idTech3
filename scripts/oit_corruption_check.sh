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
grep -q 'resolve refused: OIT attachments UNTOUCHED' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || {
  echo "FAIL resolve must refuse UNTOUCHED frame state"; fail=1; }
grep -q 'mode == 14' "$ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag" || {
  echo "FAIL constant-color diagnostic (mode 14)"; fail=1; }
grep -q 'mode == 15' "$ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag" || {
  echo "FAIL UV addressing diagnostic (mode 15)"; fail=1; }
grep -q 'directTest >= 2' "$ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag" || {
  echo "FAIL DirectTest synthetic gradient path"; fail=1; }
grep -q 'vk_reactive_mask_stamp_from_reveal' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || {
  echo "FAIL reactive stamp after OIT"; fail=1; }
# Reactive stamp must not sit between classify buckets (single call after loop)
stamp_count=$(grep -c 'vk_reactive_mask_stamp_from_reveal' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || true)
if [[ "$stamp_count" -eq 1 ]]; then
  echo "OK  single reactive stamp after OIT buckets"
else
  echo "FAIL expected 1 reactive stamp in vk_postfx_passes.c (got $stamp_count)"
  fail=1
fi
grep -q 'repro_oit_corruption.cfg' "$ROOT/scripts/compile_engine.sh" || {
  echo "FAIL compile_engine.sh must ship repro_oit_corruption.cfg"; fail=1; }
grep -q 'demo_oit_isolation.cfg' "$ROOT/scripts/compile_engine.sh" || {
  echo "FAIL compile_engine.sh must ship demo_oit_isolation.cfg"; fail=1; }
grep -q 'seta sv_pure 0' "$ROOT/config/repro_oit_corruption.cfg" || {
  echo "FAIL repro cfg must set sv_pure 0 (avoids Unpure reconnect crash)"; fail=1; }
grep -q 'CVAR_ARCHIVE );' "$ROOT/runtime/server/core/sv_init.c" && \
  grep -A1 'sv_p2pHostMigration = Cvar_Get' "$ROOT/runtime/server/core/sv_init.c" | grep -q 'CVAR_ARCHIVE' || {
  echo "FAIL sv_p2p* must not use CVAR_SERVERINFO (overflow)"; fail=1; }
if grep -E 'sv_p2pHostMigration = Cvar_Get\(.*"sv_p2pHostMigration".*CVAR_SERVERINFO' "$ROOT/runtime/server/core/sv_init.c" >/dev/null; then
  echo "FAIL sv_p2pHostMigration still CVAR_SERVERINFO"
  fail=1
else
  echo "OK  sv_p2p* not stuffed into CVAR_SERVERINFO"
fi
grep -q 'flags &= ~(int)CVAR_SERVERINFO' "$ROOT/runtime/server/core/sv_init.c" || {
  echo "FAIL must clear leftover CVAR_SERVERINFO on sv_p2p*"; fail=1; }
grep -q 'NET_P2P_IsEnabled' "$ROOT/runtime/server/core/sv_main.c" || {
  echo "FAIL SV_AddP2PServerInfo must skip when net_p2p off"; fail=1; }
grep -q 'g_airControl->flags &= ~(int)CVAR_SERVERINFO' "$ROOT/runtime/server/gameplay/sv_enhanced.c" || {
  echo "FAIL enhanced movement must leave SERVERINFO budget for OA keys"; fail=1; }
grep -q 'seta net_p2p 0' "$ROOT/config/repro_oit_corruption.cfg" || {
  echo "FAIL repro must disable net_p2p"; fail=1; }
grep -q 'vk_forward_plus_refresh_viewport_params' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || {
  echo "FAIL OIT must refresh Forward+ viewport params"; fail=1; }
grep -q 'fogOld, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || \
grep -q 'fog_scene_layout' "$ROOT/renderers/vulkan/vk_postfx_passes.c" || {
  echo "FAIL fog_scene copy must track layout before TRANSFER_DST"; fail=1; }
grep -q 'pre-deferred-composite' "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" || {
  echo "FAIL deferred composite must not COLOR_ATTACHMENT-pre-transition post_bloom"; fail=1; }
if grep -n 'vk_dgb_composite_lit_to_color' -A20 "$ROOT/renderers/vulkan/vk_deferred_gbuffer.c" | grep -q 'COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL\|SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL'; then
  echo "FAIL deferred composite still pre-transitions to COLOR_ATTACHMENT"
  fail=1
else
  echo "OK  deferred composite does not pre-transition to COLOR_ATTACHMENT"
fi
grep -q 'fog_scene_layout' "$ROOT/renderers/vulkan/vk.h" || {
  echo "FAIL fog_scene_layout tracking missing"; fail=1; }
grep -q 'RDF_NOWORLDMODEL' "$ROOT/renderers/vulkan/vk_distortion.c" || {
  echo "FAIL distortion must skip weapon pass"; fail=1; }
grep -q 'vk_copy_color_to_fog_scene' "$ROOT/renderers/vulkan/vk_distortion.c" || {
  echo "FAIL distortion must ping-pong via fog_scene"; fail=1; }
grep -q 'Same-image sample+store' "$ROOT/renderers/vulkan/vk_distortion.c" && {
  echo "FAIL distortion still documents same-image GENERAL sample"; fail=1; }
grep -q 'oit_accum_additive_pipeline' "$ROOT/renderers/vulkan/vk.h" || {
  echo "FAIL additive OIT accum pipeline missing"; fail=1; }
grep -q 'colorWriteMask = 0' "$ROOT/renderers/vulkan/vk_pipeline_helpers.c" || {
  echo "FAIL additive reveal write-mask off missing"; fail=1; }
grep -qE 'alpha < 1e-3' "$ROOT/renderers/vulkan/shaders/glsl/oit_accum.frag" || {
  echo "FAIL soft-alpha discard floor missing"; fail=1; }
grep -q 'smoothstep( 0.0, .*coverage )' "$ROOT/renderers/vulkan/shaders/glsl/oit_resolve.frag" || {
  echo "FAIL resolve soft coverage missing"; fail=1; }

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
