#!/usr/bin/env python3
"""Regression guards for Vulkan PBR/post-process specialization maps.

These source-level checks cover crash-prone map/count drift without requiring a
GPU or Vulkan loader in CI.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def read_text(path: Path) -> str:
    if not path.is_file():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8")


def assert_contiguous(values: list[int], context: str) -> None:
    expected = list(range(min(values), max(values) + 1))
    if values != expected:
        fail(f"{context}: expected contiguous IDs {expected}, got {values}")


def check_main_fragment_specs(vk_create_pipeline: str, gen_frag: str) -> None:
    pbr_capacity_match = re.search(
        r"#ifdef USE_VK_PBR\s+"
        r"/\*.*?\*/\s*"
        r"VkSpecializationMapEntry\s+spec_entries\[(\d+)\];\s*"
        r"_Static_assert\([^;]*?>=\s*(\d+)u",
        vk_create_pipeline,
        re.DOTALL,
    )
    if not pbr_capacity_match:
        fail("vk_create_pipeline.c: could not parse USE_VK_PBR spec_entries capacity/static assert")
    pbr_capacity = int(pbr_capacity_match.group(1))
    pbr_static_assert_count = int(pbr_capacity_match.group(2))

    non_pbr_capacity_match = re.search(
        r"#else\s+"
        r"VkSpecializationMapEntry\s+spec_entries\[(\d+)\];\s*"
        r"_Static_assert\([^;]*?>=\s*(\d+)u",
        vk_create_pipeline,
        re.DOTALL,
    )
    if not non_pbr_capacity_match:
        fail("vk_create_pipeline.c: could not parse non-PBR spec_entries capacity/static assert")
    non_pbr_capacity = int(non_pbr_capacity_match.group(1))
    non_pbr_static_assert_count = int(non_pbr_capacity_match.group(2))

    block_match = re.search(
        r"// fragment module specialization data(?P<body>.*?)#undef ADD_FRAG_SPEC",
        vk_create_pipeline,
        re.DOTALL,
    )
    if not block_match:
        fail("vk_create_pipeline.c: could not find ADD_FRAG_SPEC block")

    base_specs: list[tuple[int, str]] = []
    pbr_specs: list[tuple[int, str]] = []
    in_pbr_block = False
    for line in block_match.group("body").splitlines():
        stripped = line.strip()
        if stripped == "#ifdef USE_VK_PBR":
            in_pbr_block = True
            continue
        if stripped == "#endif" and in_pbr_block:
            in_pbr_block = False
            continue

        match = re.search(r"ADD_FRAG_SPEC\(\s*(\d+)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", line)
        if not match:
            continue
        spec = (int(match.group(1)), match.group(2))
        if in_pbr_block:
            pbr_specs.append(spec)
        else:
            base_specs.append(spec)

    if not base_specs or not pbr_specs:
        fail("vk_create_pipeline.c: expected both base and USE_VK_PBR ADD_FRAG_SPEC entries")

    all_specs = base_specs + pbr_specs
    all_ids = [cid for cid, _field in all_specs]
    if len(all_ids) != len(set(all_ids)):
        fail(f"vk_create_pipeline.c: duplicate ADD_FRAG_SPEC constant IDs: {all_ids}")
    assert_contiguous(all_ids, "vk_create_pipeline.c ADD_FRAG_SPEC IDs")

    base_count = len(base_specs)
    pbr_count = len(all_specs)
    if non_pbr_capacity < base_count or non_pbr_static_assert_count < base_count:
        fail(
            "vk_create_pipeline.c: non-PBR spec_entries capacity/static assert "
            f"({non_pbr_capacity}/{non_pbr_static_assert_count}) below base count {base_count}"
        )
    if pbr_capacity < pbr_count or pbr_static_assert_count < pbr_count:
        fail(
            "vk_create_pipeline.c: PBR spec_entries capacity/static assert "
            f"({pbr_capacity}/{pbr_static_assert_count}) below PBR count {pbr_count}"
        )

    if "if ( spec_entry_count > (int)( sizeof( spec_entries ) / sizeof( spec_entries[0] ) ) ) {" not in vk_create_pipeline:
        fail("vk_create_pipeline.c: missing runtime spec_entry_count overflow guard")
    if "fragment specialization map overflow" not in vk_create_pipeline:
        fail("vk_create_pipeline.c: missing diagnostic for fragment specialization map overflow")

    shader_specs = {
        field: int(cid)
        for cid, field in re.findall(
            r"layout\s*\(\s*constant_id\s*=\s*(\d+)\s*\)\s*const\s+\w+\s+([A-Za-z_][A-Za-z0-9_]*)\b",
            gen_frag,
        )
    }
    for cid, field in pbr_specs:
        shader_cid = shader_specs.get(field)
        if shader_cid != cid:
            fail(
                "gen_frag.tmpl/vk_create_pipeline.c: specialization constant drift for "
                f"{field}: shader={shader_cid}, ADD_FRAG_SPEC={cid}"
            )

    print(
        "PASS: vk_create_pipeline fragment spec map "
        f"base={base_count}, PBR={pbr_count}, capacity={pbr_capacity}"
    )


def check_post_process_specs(vk_post_process_pipeline: str) -> None:
    count_match = re.search(r"#define\s+VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT\s+(\d+)", vk_post_process_pipeline)
    if not count_match:
        fail("vk_post_process_pipeline.c: missing VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT")
    expected_count = int(count_match.group(1))

    if "VkSpecializationMapEntry spec_entries[VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT];" not in vk_post_process_pipeline:
        fail("vk_post_process_pipeline.c: spec_entries[] should be sized by VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT")
    if "sizeof( spec_entries ) / sizeof( spec_entries[0] ) >= (size_t)VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT" not in vk_post_process_pipeline:
        fail("vk_post_process_pipeline.c: missing static assert against VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT")
    if "frag_spec_info.mapEntryCount = VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT;" not in vk_post_process_pipeline:
        fail("vk_post_process_pipeline.c: mapEntryCount should use VK_POST_PROCESS_FRAG_SPEC_MAP_COUNT")

    ids: list[int] = []
    indexes: list[int] = []
    for index, cid in re.findall(
        r"spec_entries\[(\d+)\]\.constantID\s*=\s*(\d+)\s*;",
        vk_post_process_pipeline,
    ):
        indexes.append(int(index))
        ids.append(int(cid))

    if len(ids) != expected_count:
        fail(
            "vk_post_process_pipeline.c: expected "
            f"{expected_count} constantID assignments, got {len(ids)}"
        )
    assert_contiguous(indexes, "vk_post_process_pipeline.c spec_entries indexes")
    assert_contiguous(ids, "vk_post_process_pipeline.c constant IDs")
    if indexes != ids:
        fail(f"vk_post_process_pipeline.c: indexes and constant IDs drifted: indexes={indexes}, ids={ids}")

    print(f"PASS: vk_post_process_pipeline fragment spec map count={expected_count}")


def main() -> None:
    project_root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]

    vk_create_pipeline = read_text(project_root / "src/renderers/vulkan/vk_create_pipeline.c")
    gen_frag = read_text(project_root / "src/renderers/vulkan/shaders/glsl/gen_frag.tmpl")
    vk_post_process_pipeline = read_text(project_root / "src/renderers/vulkan/vk_post_process_pipeline.c")

    check_main_fragment_specs(vk_create_pipeline, gen_frag)
    check_post_process_specs(vk_post_process_pipeline)
    print("PASS: test_vulkan_forward_plus_pbr_guards")


if __name__ == "__main__":
    main()
