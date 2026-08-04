#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CFG="$ROOT/examples/demo_game/mod/sponza_material_parity.cfg"
BENCHMARK_CFG="$ROOT/examples/demo_game/mod/sponza_benchmark.cfg"
USD_BRIDGE="$ROOT/renderers/common/tr_model_freeusd.cpp"
SPONZA_USDA="$ROOT/sponza/main_sponza/NewSponza_Main_USD_Zup_003.usda"

fail() { echo "FAIL: $*" >&2; exit 1; }

[[ -f "$CFG" ]] || fail "Sponza parity config missing"
[[ -f "$BENCHMARK_CFG" ]] || fail "Sponza benchmark config missing"
[[ -f "$USD_BRIDGE" ]] || fail "FreeUSD material bridge missing"
[[ -f "$SPONZA_USDA" ]] || fail "repo Sponza USDA payload missing"
grep -q 'district_load world/sponza_fixture.usda' "$CFG" || fail "USDA fixture not loaded"
grep -q 'set r_districtCamera 1' "$CFG" || fail "dedicated USDA camera not enabled"
grep -q 'set r_districtOnly 1' "$CFG" || fail "host BSP not isolated"
grep -q 'set r_freeusdMeshBudget 250000' "$CFG" || fail "mesh budget not fixed"
grep -q 'set r_freeusdShaderMap 1' "$CFG" || fail "USDA shader/material bridge not enabled"
grep -q 'set r_oitClassify 1' "$CFG" || fail "OIT material classification not enabled"
grep -q 'set r_oitForwardPlus 1' "$CFG" || fail "transparent materials are not lit through clustered Forward+"
grep -q 'GetRelationshipTargets( binding )' "$USD_BRIDGE" || fail "authored material bindings are not inspected"
grep -q 'GetParentPath()' "$USD_BRIDGE" || fail "enclosing material bindings are not inspected"
grep -q 'GeomSubset' "$USD_BRIDGE" || fail "USDA GeomSubset ownership is not imported"
grep -q 'R_MeshImport_FinalizeMD3Multi' "$ROOT/renderers/common/tr_model_freeusd_register.c" || fail "material subsets are not emitted as independent render surfaces"
grep -q 'firstTri' "$ROOT/renderers/common/tr_model_freeusd.cpp" || fail "material surface triangle ranges are missing"
grep -q 'GLS_SRCBLEND_SRC_ALPHA' "$ROOT/renderers/common/tr_model_mesh_import.c" || fail "USDA opacity is not translated to blend state"
grep -q 'GLS_ATEST_GE_80' "$ROOT/renderers/common/tr_model_mesh_import.c" || fail "USDA opacityThreshold is not translated to alpha-test state"
grep -q 'vk_create_normal_texture' "$ROOT/renderers/common/tr_model_mesh_import.c" || fail "USDA normal texture is not bound to the Vulkan material stage"
grep -q 'vk_create_emissive_texture' "$ROOT/renderers/common/tr_model_mesh_import.c" || fail "USDA emissive texture is not bound to the Vulkan material stage"
grep -q 'GetNormalTextureAssetPath' "$USD_BRIDGE" || fail "USDA normal texture connections are not resolved"
grep -q 'GetEmissiveTextureAssetPath' "$USD_BRIDGE" || fail "USDA emissive texture connections are not resolved"
grep -q 'vk_create_usda_orm_texture' "$ROOT/renderers/common/tr_model_mesh_import.c" || fail "USDA metallic/roughness channels are not packed into ORMS"
grep -q 'GetMetallicTextureAssetPath' "$USD_BRIDGE" || fail "USDA metallic texture connections are not resolved"
grep -q 'GetRoughnessTextureAssetPath' "$USD_BRIDGE" || fail "USDA roughness texture connections are not resolved"
grep -q 'PreviewSurface texture channels' "$ROOT/runtime/client/shell/cl_usd.cpp" || fail "USDA channel-level audit is missing"
grep -q 'R_Freeusd_AssetPathToShaderQpath( texPath, usdQpath' "$USD_BRIDGE" || fail "relative texture paths are not resolved"
# Keep the fixture useful as a material-parity proof, rather than only a mesh
# import proof. These channels are authored by the Sponza PreviewSurface graph.
for channel in BaseColor Normal Roughness Metalness; do
	rg -q "inputs:file = @[^@]*_${channel}\\.(png|jpg|jpeg)@" "$SPONZA_USDA" || fail "Sponza USDA has no ${channel} texture bindings"
done
for name in forward deferred wboit ssr rtx; do
	grep -q "screenshot sponza_material_parity_${name}" "$CFG" || fail "missing ${name} capture"
done

echo "Sponza material parity capture contract passed."
