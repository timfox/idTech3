#!/usr/bin/env python3
"""Headless regression guards for Vulkan Forward+ resource wiring.

The Forward+ path depends on live Vulkan objects, render-target sizing, and
descriptor pool lifetime, so these checks pin the source-level invariants that
are impractical to exercise deterministically in CI without GPU hardware.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def read_text(path: Path) -> str:
    if not path.is_file():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8")


def extract_function(source: str, name: str) -> str:
    pattern = re.compile(
        rf"^[\t A-Za-z_][^\n;{{}}]*\b{re.escape(name)}\s*\([^;]*?\)\s*\{{",
        re.MULTILINE | re.DOTALL,
    )
    match = pattern.search(source)
    if not match:
        fail(f"could not find function {name}")

    start = match.start()
    brace = source.find("{", match.start())
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]

    fail(f"could not extract complete body for {name}")
    return ""


def assert_contains(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        fail(f"{context}: expected to find {needle!r}")


def assert_not_contains(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        fail(f"{context}: unexpected {needle!r}")


def strip_c_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    source = re.sub(r"//.*", "", source)
    return source


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if re.search(pattern, haystack, re.DOTALL) is None:
        fail(f"{context}: expected pattern {pattern!r}")


def assert_count(haystack: str, needle: str, expected: int, context: str) -> None:
    actual = haystack.count(needle)
    if actual != expected:
        fail(f"{context}: expected {expected} occurrences of {needle!r}, found {actual}")


def assert_order(haystack: str, first: str, second: str, context: str) -> None:
    first_at = haystack.find(first)
    second_at = haystack.find(second)
    if first_at == -1 or second_at == -1 or first_at >= second_at:
        fail(f"{context}: expected {first!r} before {second!r}")


def main() -> int:
    if len(sys.argv) > 2:
        fail("usage: test_vulkan_forward_plus_guards.py [project-root]")

    project_root = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parents[2]

    vk_forward_plus = read_text(project_root / "src/renderers/vulkan/vk_forward_plus.c")
    vk_forward_plus_h = read_text(project_root / "src/renderers/vulkan/vk_forward_plus.h")
    vk_resource_destroy = read_text(project_root / "src/renderers/vulkan/vk_resource_destroy.c")
    vk_shutdown = read_text(project_root / "src/renderers/vulkan/vk_shutdown.c")
    vk_frame_submit = read_text(project_root / "src/renderers/vulkan/vk_frame_submit.c")
    vk_draw_state = read_text(project_root / "src/renderers/vulkan/vk_draw_state.c")
    tr_backend = read_text(project_root / "src/renderers/vulkan/tr_backend.c")

    layout_body = extract_function(vk_forward_plus, "vk_forward_plus_create_set_layout")
    assert_count(
        layout_body,
        "descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;",
        3,
        "Forward+ descriptor layout must expose light, tile, and parameter SSBOs",
    )
    assert_count(
        layout_body,
        "stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;",
        3,
        "Forward+ SSBOs must be visible to both tile compute and PBR fragment shading",
    )
    assert_contains(layout_body, "layout_ci.bindingCount = 3;", "Forward+ descriptor layout binding count")

    graphics_desc_body = extract_function(vk_forward_plus, "vk_forward_plus_init_graphics_descriptors")
    assert_contains(
        graphics_desc_body,
        "if ( vk.set_layout_forward_plus == VK_NULL_HANDLE || vk.descriptor_pool == VK_NULL_HANDLE )",
        "graphics descriptor init must wait for layout and descriptor pool",
    )
    assert_contains(
        graphics_desc_body,
        "if ( vk_fp_graphics_descriptor == VK_NULL_HANDLE )",
        "graphics descriptor init must allocate once and then reuse the set",
    )
    assert_order(
        graphics_desc_body,
        "vk_fp_create_dummy_buffers();",
        "if ( r_forwardPlus && r_forwardPlus->integer",
        "dummy SSBOs must exist before selecting real-or-dummy bindings",
    )
    assert_regex(
        graphics_desc_body,
        r"if\s*\(\s*r_forwardPlus\s*&&\s*r_forwardPlus->integer\s*&&\s*"
        r"vk\.forward_plus\.buffer\s*!=\s*VK_NULL_HANDLE\s*&&\s*"
        r"vk\.forward_plus\.tile_buffer\s*!=\s*VK_NULL_HANDLE\s*&&\s*"
        r"vk\.forward_plus\.param_buffer\s*!=\s*VK_NULL_HANDLE\s*\)\s*\{"
        r".*vk_fp_write_graphics_descriptor\s*\(\s*vk\.forward_plus\.buffer\s*,\s*"
        r"vk\.forward_plus\.tile_buffer\s*,\s*vk\.forward_plus\.param_buffer\s*\)\s*;"
        r".*\}\s*else\s*\{\s*vk_fp_write_graphics_descriptor\s*\(\s*"
        r"vk_fp_dummy_light_buf\s*,\s*vk_fp_dummy_tile_buf\s*,\s*vk_fp_dummy_param_buf\s*\)\s*;",
        "PBR graphics descriptor must use real SSBOs only when the full Forward+ set is live",
    )

    resize_body = extract_function(vk_forward_plus, "vk_fp_ensure_tile_for_render_resolution")
    assert_contains(resize_body, "if ( !r_forwardPlus || !r_forwardPlus->integer )", "tile resize cvar gate")
    assert_contains(
        resize_body,
        "if ( vk.forward_plus.tile_pipeline == VK_NULL_HANDLE || vk.forward_plus.buffer == VK_NULL_HANDLE )",
        "tile resize must wait for initialized compute/light resources",
    )
    assert_contains(resize_body, "if ( !vk.device || vk.device_lost )", "tile resize device-loss guard")
    assert_contains(
        resize_body,
        "changed = ( tiles_x != vk.forward_plus.tiles_x || tiles_y != vk.forward_plus.tiles_y ||",
        "tile resize must detect render-target grid changes",
    )
    assert_contains(resize_body, "bci.size = tile_bytes;", "tile resize must size the replacement SSBO from current grid")
    assert_contains(resize_body, "bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;", "tile resize storage usage")
    assert_contains(
        resize_body,
        "[VK][Forward+] tile buffer create failed (%d); keeping previous tile SSBO",
        "tile resize create failure must preserve previous SSBO",
    )
    assert_contains(
        resize_body,
        "[VK][Forward+] tile buffer memory alloc failed (%d); keeping previous tile SSBO",
        "tile resize allocation failure must preserve previous SSBO",
    )
    assert_contains(
        resize_body,
        "[VK][Forward+] tile buffer bind failed (%d); keeping previous tile SSBO",
        "tile resize bind failure must preserve previous SSBO",
    )
    assert_order(
        resize_body,
        "SET_OBJECT_NAME( new_tile, \"forward+ tile indices\", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT );",
        "vk_fp_destroy_tile_buffer_only();",
        "old tile SSBO must only be destroyed after the replacement is fully bound",
    )
    assert_order(
        resize_body,
        "vk.forward_plus.tile_capacity_tiles = total_tiles;",
        "vk_fp_update_compute_descriptor_tile_binding();",
        "compute descriptor must be refreshed after new tile state is installed",
    )
    assert_order(
        resize_body,
        "vk_fp_update_compute_descriptor_tile_binding();",
        "vk_forward_plus_init_graphics_descriptors();",
        "graphics descriptor fallback/real binding must refresh after compute binding",
    )

    create_body = extract_function(vk_forward_plus, "vk_fp_create_buffers_and_compute")
    assert_contains(
        create_body,
        "const uint32_t max_lights = (uint32_t)MAX_DLIGHTS;",
        "Forward+ light SSBO capacity must stay aligned with tess.dlightBits",
    )
    assert_contains(
        create_body,
        "vk.forward_plus.max_per_tile = vk_fp_effective_max_per_tile();",
        "Forward+ init must clamp max lights per tile through the shared helper",
    )
    assert_contains(
        create_body,
        "if ( vk.modules.forward_plus_tile_cull_cs == VK_NULL_HANDLE )",
        "Forward+ init must handle missing tile-cull shader module",
    )
    assert_contains(
        create_body,
        "vk_fp_destroy_light_buffer();",
        "Forward+ init failure paths must tear down the already-created light buffers",
    )
    assert_contains(
        create_body,
        "vk.forward_plus.tile_capacity_tiles = total_tiles;",
        "Forward+ init must publish tile capacity after descriptor setup",
    )
    assert_order(
        create_body,
        "qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );",
        "vk.forward_plus.tiles_x = tiles_x;",
        "compute descriptor set must be populated before tile grid state is advertised",
    )
    assert_order(
        create_body,
        "vk.forward_plus.tile_capacity_tiles = total_tiles;",
        "vk_forward_plus_init_graphics_descriptors();",
        "graphics descriptors must be refreshed after compute resources are fully initialized",
    )

    upload_body = extract_function(vk_forward_plus, "vk_forward_plus_upload_refdef")
    assert_contains(upload_body, "vk.forward_plus.last_upload_bytes == 0u", "upload must skip empty light payloads")
    assert_order(
        upload_body,
        "VK_ACCESS_HOST_WRITE_BIT",
        "qvkCmdCopyBuffer( vk.cmd->command_buffer, vk.forward_plus.staging, vk.forward_plus.buffer, 1, &region );",
        "upload must barrier host-visible staging before copying to device-local light SSBO",
    )
    assert_order(
        upload_body,
        "qvkCmdCopyBuffer( vk.cmd->command_buffer, vk.forward_plus.staging, vk.forward_plus.buffer, 1, &region );",
        "b[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;",
        "upload must barrier transfer writes before shader reads",
    )

    dispatch_body = extract_function(vk_forward_plus, "vk_forward_plus_dispatch_tile_cull")
    assert_contains(
        dispatch_body,
        "if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !vk.inRenderPass )",
        "tile cull dispatch must require a live command buffer/render pass context",
    )
    assert_contains(dispatch_body, "barriers[2].buffer = vk.forward_plus.tile_buffer;", "tile dispatch write barrier target")
    assert_order(
        dispatch_body,
        "qvkCmdDispatch( vk.cmd->command_buffer, ( push.total_tiles + 63u ) / 64u, 1, 1 );",
        "barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;",
        "tile dispatch must publish compute writes before later shader reads",
    )
    assert_contains(
        dispatch_body,
        "push.luminance_sort = ( r_forwardPlusLuminanceSort && r_forwardPlusLuminanceSort->integer ) ? 1u : 0u;",
        "tile dispatch must preserve the runtime luminance-sort toggle",
    )

    destroy_buffers_body = extract_function(vk_forward_plus, "vk_fp_destroy_buffers")
    for needle, context in [
        ("vk.forward_plus.tile_buffer = VK_NULL_HANDLE;", "tile buffer reset"),
        ("vk.forward_plus.tile_memory = VK_NULL_HANDLE;", "tile memory reset"),
        ("vk.forward_plus.param_buffer = VK_NULL_HANDLE;", "param buffer reset"),
        ("vk.forward_plus.param_memory = VK_NULL_HANDLE;", "param memory reset"),
        ("vk.forward_plus.param_mapped = NULL;", "mapped param pointer reset"),
        ("vk.forward_plus.param_buffer_size = 0u;", "param size reset"),
        ("vk.forward_plus.tile_capacity_tiles = 0u;", "tile capacity reset"),
        ("vk.forward_plus.descriptor = VK_NULL_HANDLE;", "compute descriptor reset"),
    ]:
        assert_contains(destroy_buffers_body, needle, context)

    shutdown_body = extract_function(vk_forward_plus, "vk_forward_plus_shutdown")
    assert_order(shutdown_body, "vk_fp_destroy_buffers();", "vk_fp_destroy_light_buffer();", "shutdown buffer order")
    assert_order(shutdown_body, "vk_fp_destroy_light_buffer();", "vk.forward_plus.last_packed_count = 0u;", "shutdown state reset order")
    assert_contains(shutdown_body, "vk.forward_plus.tiles_x = 0u;", "shutdown tiles_x reset")
    assert_contains(shutdown_body, "vk.forward_plus.tiles_y = 0u;", "shutdown tiles_y reset")
    assert_order(shutdown_body, "vk.forward_plus.tiles_y = 0u;", "vk_fp_destroy_dummy_buffers();", "dummy descriptor buffers shutdown")

    pool_destroyed_body = extract_function(vk_forward_plus, "vk_forward_plus_on_descriptor_pool_destroyed")
    assert_contains(pool_destroyed_body, "vk_fp_graphics_descriptor = VK_NULL_HANDLE;", "graphics descriptor reset on pool destroy")
    assert_contains(pool_destroyed_body, "vk.forward_plus.descriptor = VK_NULL_HANDLE;", "compute descriptor reset on pool destroy")

    destroy_pipelines_body = extract_function(vk_resource_destroy, "vk_destroy_pipelines")
    assert_order(
        destroy_pipelines_body,
        "vk_forward_plus_destroy_compute_pipeline();",
        "for ( i = 0; i < vk.pipelines_count; i++ )",
        "global pipeline teardown must destroy Forward+ compute state before walking graphics pipelines",
    )

    begin_frame_body = extract_function(vk_frame_submit, "vk_begin_frame")
    begin_frame_code = strip_c_comments(begin_frame_body)
    assert_contains(
        begin_frame_body,
        "if ( r_forwardPlusShade && r_forwardPlusShade->modified )",
        "Forward+ shade cvar changes must be handled at frame begin",
    )
    assert_contains(
        begin_frame_body,
        "vk_destroy_world_graphics_pipelines();",
        "Forward+ shade cvar changes must invalidate world graphics pipelines",
    )
    assert_not_contains(
        begin_frame_code,
        "vk_destroy_pipelines(",
        "Forward+ shade cvar changes must not tear down unrelated post-process pipelines",
    )

    assert_order(
        tr_backend,
        "vk_forward_plus_ensure_render_resolution();",
        "vk_forward_plus_update_for_refdef();",
        "draw-surfs flow must resize tile SSBO before packing refdef data",
    )
    assert_order(
        tr_backend,
        "vk_forward_plus_update_for_refdef();",
        "RB_BeginDrawingView();",
        "draw-surfs flow must pack Forward+ lights before the render pass begins",
    )
    assert_order(
        tr_backend,
        "RB_BeginDrawingView();",
        "vk_forward_plus_upload_refdef();",
        "draw-surfs flow must upload Forward+ lights after the command buffer/render pass starts",
    )
    assert_order(
        tr_backend,
        "vk_forward_plus_upload_refdef();",
        "vk_forward_plus_dispatch_tile_cull();",
        "draw-surfs flow must dispatch tile cull after light SSBO upload",
    )

    assert_contains(
        vk_draw_state,
        "vk.cmd->descriptor_set.current[VK_DESC_FORWARD_PLUS] = fp_set;",
        "draw state must bind the Forward+ graphics descriptor when available",
    )
    assert_contains(
        vk_frame_submit,
        "vk.cmd->descriptor_set.current[VK_DESC_FORWARD_PLUS] = fp_set;",
        "frame begin must seed the Forward+ graphics descriptor when available",
    )
    assert_contains(
        vk_forward_plus_h,
        "/* Resize tile SSBO when FBO / r_renderScale resolution changes (no vid_restart). */",
        "Forward+ public resize invariant must remain documented",
    )

    assert_order(
        vk_shutdown,
        "qvkDestroyDescriptorPool( vk.device, vk.descriptor_pool, NULL );",
        "vk_forward_plus_on_descriptor_pool_destroyed();",
        "descriptor pool shutdown must invalidate Forward+ descriptor handles immediately",
    )
    assert_order(
        vk_shutdown,
        "vk_forward_plus_on_descriptor_pool_destroyed();",
        "vk_forward_plus_destroy_graphics_layout();",
        "Forward+ graphics layout must outlive descriptor-pool handle reset",
    )
    assert_order(
        vk_shutdown,
        "vk_release_geometry_buffers();",
        "vk_forward_plus_shutdown();",
        "Forward+ buffers must be released during Vulkan shutdown",
    )

    print("PASS: test_vulkan_forward_plus_guards")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
