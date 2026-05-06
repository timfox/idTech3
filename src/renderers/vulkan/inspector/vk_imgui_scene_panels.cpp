/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Profiler, viewport, shader browser, objects, and inspector panels (scene API).
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"

extern "C" void VkImgui_DrawProfiler(void) {
	if (!vkWindows.profiler.open) return;
	ImGuiIO &io = ImGui::GetIO();
	const float fps = io.Framerate > 0.0f ? io.Framerate : 0.0f;
	const float ms = fps > 0.0f ? 1000.0f / fps : 0.0f;
	ImGui::Begin("GPU Profiler", (bool *)&vkWindows.profiler.open);

	ImGui::SeparatorText( "CPU / overlay" );
	ImGui::Text( "Instant: %.3f ms/frame   %.1f FPS", ms, fps );

	static float frameTimes[120] = {};
	static int frameIdx = 0;
	int i;
	float histMin;
	float histMax;
	float histSum;

	frameTimes[frameIdx] = ms;
	frameIdx = ( frameIdx + 1 ) % 120;
	histMin = frameTimes[0];
	histMax = frameTimes[0];
	histSum = 0.0f;
	for ( i = 0; i < 120; i++ ) {
		const float v = frameTimes[i];
		if ( v < histMin ) {
			histMin = v;
		}
		if ( v > histMax ) {
			histMax = v;
		}
		histSum += v;
	}
	ImGui::Text( "Last 120 frames: min %.3f   avg %.3f   max %.3f ms", histMin, histSum / 120.0f, histMax );
	ImGui::PlotLines( "Frame time (ms)", frameTimes, 120, frameIdx, nullptr, 0.0f, 33.0f, ImVec2( 0.0f, 96.0f ) );

#ifdef USE_VULKAN
	ImGui::SeparatorText( "Scene (last refdef)" );
	ImGui::Text( "Ref entities: %d", VkImgScene_RefEntityCount() );
	ImGui::Text( "Dynamic lights: %u", VkImgScene_DlightCount() );
	ImGui::Text( "Draw surfs (batch): %d", VkImgScene_RefdefNumDrawSurfs() );
#endif

	ImGui::End();
}

#ifdef USE_VULKAN
static const char *VkImgui_ReTypeLabel( refEntityType_t rt )
{
	switch ( rt ) {
	case RT_MODEL:
		return "MODEL";
	case RT_POLY:
		return "POLY";
	case RT_SPRITE:
		return "SPRITE";
	case RT_BEAM:
		return "BEAM";
	case RT_RAIL_CORE:
		return "RAIL_CORE";
	case RT_RAIL_RINGS:
		return "RAIL_RINGS";
	case RT_LIGHTNING:
		return "LIGHTNING";
	case RT_PORTALSURFACE:
		return "PORTAL";
	default:
		return "OTHER";
	}
}

/* Aligns with modtype_t order in tr_local.h (C-only); keep numeric to avoid including tr_local in C++. */
static const char *VkImgui_ModTypeLabel( int mt )
{
	switch ( mt ) {
	case 0:
		return "BAD";
	case 1:
		return "BRUSH";
	case 2:
		return "MESH_MD3";
	case 3:
		return "MDR";
	case 4:
		return "IQM";
	case 5:
		return "GLTF";
	default:
		return "?";
	}
}

static qboolean VkImgui_SceneKeywordMatch( const char *kw, int entityIdx, const char *modelPath,
	const char *reTypeLabel )
{
	char idxBuf[16];

	if ( !kw || kw[0] == '\0' ) {
		return qtrue;
	}
	Com_sprintf( idxBuf, sizeof( idxBuf ), "%d", entityIdx );
	if ( Q_stristr( idxBuf, kw ) ) {
		return qtrue;
	}
	if ( modelPath && modelPath[0] && Q_stristr( modelPath, kw ) ) {
		return qtrue;
	}
	if ( reTypeLabel && reTypeLabel[0] && Q_stristr( reTypeLabel, kw ) ) {
		return qtrue;
	}
	return qfalse;
}

static void VkImgui_ClearInspectorSelectionScene( void )
{
	vkInspector.inspectorSelectionKind = VK_INSP_KIND_NONE;
	vkInspector.inspectorEntityIndex = -1;
	vkInspector.inspectorDlightIndex = -1;
	vkInspector.shader.active = qfalse;
}
#endif

extern "C" void VkImgui_DrawViewport(void) {
	if ( !vkWindows.viewport.open ) return;
	ImGui::Begin( "Viewport", (bool *)&vkWindows.viewport.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip( "Reads tr.refdef from the last assembled scene (same frame as Objects)." );
	}
#ifdef USE_VULKAN
	{
		char wn[MAX_QPATH];
		int rx, ry, rw, rh;
		float fovx, fovy;
		float vo[3], va0[3];

		VkImgScene_WorldName( wn, sizeof( wn ) );
		ImGui::SeparatorText( "Refdef" );
		if ( !VkImgScene_WorldLoaded() ) {
			ImGui::TextDisabled( "No BSP world (menus or loading)." );
		} else {
			ImGui::TextWrapped( "%s", wn );
			VkImgScene_RefdefViewport( &rx, &ry, &rw, &rh );
			VkImgScene_RefdefFov( &fovx, &fovy );
			ImGui::Text( "Rects: xy %d %d size %dx%d", rx, ry, rw, rh );
			ImGui::Text( "FOV: %.2f x %.2f deg", fovx, fovy );
			ImGui::Text( "Time: %d ms  rdflags 0x%x", VkImgScene_RefdefTime(), VkImgScene_RefdefRdFlags() );
			VkImgScene_RefdefViewOrg( vo );
			VkImgScene_RefdefViewAxis0( va0 );
			ImGui::Text( "View org: %.2f %.2f %.2f", vo[0], vo[1], vo[2] );
			ImGui::Text( "View axis X: %.2f %.2f %.2f", va0[0], va0[1], va0[2] );
			ImGui::SeparatorText( "Scene batch" );
			ImGui::Text( "Entities: %d   DLights: %u", VkImgScene_RefEntityCount(), VkImgScene_DlightCount() );
			ImGui::Text( "DrawSurfs: %d   LitSurfs: %d", VkImgScene_RefdefNumDrawSurfs(),
				VkImgScene_RefdefNumLitSurfs() );
		}
		ImGui::SeparatorText( "Display" );
		ImGui::Text( "Game swaps to framebuffer; inspector uses docking passthrough (no embedded RT)." );
	}
#else
	ImGui::TextWrapped( "Viewport diagnostics require Vulkan backend." );
#endif
	ImGui::End();
}

extern "C" void VkImgui_DrawShaderEditor(void) {
	if ( !vkWindows.shader.open ) return;
	ImGui::Begin( "Shaders", (bool *)&vkWindows.shader.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip( "Browses tr.sortedShaders[]. Live .shader editing stays on the console (!)" );
	}
#ifdef USE_VULKAN
	ImGui::InputTextWithHint( "Filter##shaderfilter", "substring...", vkInspector.searchKeyword, sizeof( vkInspector.searchKeyword ) );
	if ( ImGui::Button( "Reload shader scripts" ) ) {
		ri.Cmd_ExecuteText( EXEC_APPEND, "r_reloadShaders\n" );
	}
	ImGui::SameLine();
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip( "Parses *.shader again; fallback vid_restart if nothing changes." );
	}
	ImGui::Separator();
	ImGui::Text( "Total sorted: %d", VkImgScene_NumSortedShaders() );
	ImGui::BeginChild( "shader_list", ImVec2( 0.0f, 0.0f ), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar );
	{
		const char *filt = vkInspector.searchKeyword;
		ImGuiListClipper clipper;
		int n;

		n = VkImgScene_NumSortedShaders();
		if ( n < 0 ) {
			n = 0;
		}
		clipper.Begin( n );
		while ( clipper.Step() ) {
			int row;

			for ( row = clipper.DisplayStart; row < clipper.DisplayEnd; row++ ) {
				vkImgSceneShader_t shs;

				if ( !VkImgScene_SortedShaderSnapshot( row, &shs ) ) {
					continue;
				}
				if ( filt[0] != '\0' && !Q_stristr( shs.name, filt ) ) {
					continue;
				}
				ImGui::PushID( row );
				{
					bool sel = vkInspector.shader.active && vkInspector.shader.index == row;

					if ( ImGui::Selectable( shs.name, sel ) ) {
						vkInspector.shader.active = qtrue;
						vkInspector.shader.index = row;
						vkInspector.inspectorSelectionKind = VK_INSP_KIND_NONE;
						vkInspector.inspectorEntityIndex = -1;
						vkInspector.inspectorDlightIndex = -1;
					}
				}
				ImGui::PopID();
			}
		}
	}
	ImGui::EndChild();
#else
	ImGui::Text( "Shader browser requires Vulkan renderer build." );
#endif
	ImGui::End();
}

extern "C" void VkImgui_DrawObjects(void) {
	if ( !vkWindows.objects.open ) return;
	ImGui::Begin( "Objects", (bool *)&vkWindows.objects.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip(
			"Reads tr.world + current tr.refdef batch (same data the backend just rendered)." );
	}
#ifdef USE_VULKAN
	ImGui::InputTextWithHint( "Filter##objfilter", "model path / index...", vkInspector.searchKeyword,
		sizeof( vkInspector.searchKeyword ) );
	if ( ImGui::SmallButton( "Clear selection##obj" ) ) {
		VkImgui_ClearInspectorSelectionScene();
	}
	ImGui::SameLine();
	ImGui::TextDisabled( "Filter matches index, path, reType, or dlight line" );
	ImGui::Separator();

	if ( ImGui::TreeNodeEx( "World (BSP)", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		qboolean selWorld;
		char wname[MAX_QPATH];

		selWorld = vkInspector.inspectorSelectionKind == VK_INSP_KIND_WORLD;
		ImGui::Text( "Clusters: %d  clusterBytes: %d  Surfaces: %d", VkImgScene_WorldNumClusters(),
			VkImgScene_WorldClusterBytes(), VkImgScene_WorldNumSurfaces() );
		VkImgScene_WorldName( wname, sizeof( wname ) );
		if ( VkImgScene_WorldLoaded() && wname[0] ) {
			if ( ImGui::Selectable( wname, selWorld ) ) {
				vkInspector.inspectorSelectionKind = VK_INSP_KIND_WORLD;
				vkInspector.inspectorEntityIndex = -1;
				vkInspector.inspectorDlightIndex = -1;
				vkInspector.shader.active = qfalse;
			}
		} else {
			ImGui::TextDisabled( "No map" );
		}
		ImGui::TreePop();
	}

	if ( ImGui::TreeNodeEx( "Ref entities", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		int i;
		const int numEnt = VkImgScene_RefEntityCount();
		if ( numEnt <= 0 ) {
			ImGui::TextDisabled( "None this scene." );
		} else {
			for ( i = 0; i < numEnt; i++ ) {
				vkImgSceneRefEntity_t snap;
				char pathLabel[MAX_QPATH];
				const char *pathShown;
				char lineLabel[384];

				pathLabel[0] = '\0';
				VkImgScene_RefEntityModelPath( i, pathLabel, sizeof( pathLabel ) );
				pathShown = pathLabel[0] ? pathLabel : "(no model)";
				if ( !VkImgScene_RefEntitySnapshot( i, &snap ) ) {
					continue;
				}

				if ( !VkImgui_SceneKeywordMatch( vkInspector.searchKeyword, i, pathShown,
					    VkImgui_ReTypeLabel( (refEntityType_t)snap.reType ) ) ) {
					continue;
				}
				Com_sprintf( lineLabel, sizeof( lineLabel ), "[%d] %s  %s", i,
					VkImgui_ReTypeLabel( (refEntityType_t)snap.reType ), pathShown );
				ImGui::PushID( i );
				{
					bool sel;

					sel = vkInspector.inspectorSelectionKind == VK_INSP_KIND_ENTITY &&
					    vkInspector.inspectorEntityIndex == i;
					if ( ImGui::Selectable( lineLabel, sel ) ) {
						vkInspector.inspectorSelectionKind = VK_INSP_KIND_ENTITY;
						vkInspector.inspectorEntityIndex = i;
						vkInspector.inspectorDlightIndex = -1;
						vkInspector.shader.active = qfalse;
					}
				}
				ImGui::PopID();
			}
		}
		ImGui::TreePop();
	}

	if ( ImGui::TreeNodeEx( "Dynamic lights", ImGuiTreeNodeFlags_DefaultOpen ) ) {
		unsigned int di;
		const unsigned dc = VkImgScene_DlightCount();
		if ( dc == 0 ) {
			ImGui::TextDisabled( "None this scene." );
		} else {
			for ( di = 0; di < dc; di++ ) {
				vkImgSceneDlight_t dl;
				char lineLabel[128];
				const int idx = (int)di;
				if ( !VkImgScene_DlightSnapshot( idx, &dl ) ) {
					continue;
				}
				Com_sprintf( lineLabel, sizeof( lineLabel ), "[%d] rad %.0f rgb (%.2f %.2f %.2f)", idx,
					dl.radius, dl.color[0], dl.color[1], dl.color[2] );
				if ( vkInspector.searchKeyword[0] != '\0' &&
					!Q_stristr( lineLabel, vkInspector.searchKeyword ) ) {
					continue;
				}
				ImGui::PushID( idx + 4096 );
				{
					bool sel;

					sel = vkInspector.inspectorSelectionKind == VK_INSP_KIND_DLIGHT &&
					    vkInspector.inspectorDlightIndex == idx;
					if ( ImGui::Selectable( lineLabel, sel ) ) {
						vkInspector.inspectorSelectionKind = VK_INSP_KIND_DLIGHT;
						vkInspector.inspectorDlightIndex = idx;
						vkInspector.inspectorEntityIndex = -1;
						vkInspector.shader.active = qfalse;
					}
				}
				ImGui::PopID();
			}
		}
		ImGui::TreePop();
	}

	if ( ImGui::TreeNodeEx( "Registered models (tr.models)", ImGuiTreeNodeFlags_None ) ) {
		int mi;
		const int nm = VkImgScene_NumModels();
		int cap;

		ImGui::TextDisabled( "First chunk of loaded model slots (not filtered by visibility)." );
		cap = nm;
		if ( cap > 96 ) {
			cap = 96;
		}
		for ( mi = 1; mi < cap; mi++ ) {
			vkImgSceneModelSlot_t ms;

			if ( !VkImgScene_ModelSlot( mi, &ms ) ) {
				continue;
			}
			if ( vkInspector.searchKeyword[0] != '\0' && !Q_stristr( ms.name, vkInspector.searchKeyword ) ) {
				continue;
			}
			ImGui::PushID( mi + 8192 );
			ImGui::BulletText( "%s [%s]", ms.name, VkImgui_ModTypeLabel( ms.modType ) );
			ImGui::PopID();
		}
		if ( nm > cap ) {
			ImGui::TextDisabled( "... %d more (raise cap in vk_imgui_scene_panels.cpp)", nm - cap );
		}
		ImGui::TreePop();
	}
#else
	ImGui::TextWrapped( "Object browser requires Vulkan + tr.refdef." );
#endif
	ImGui::End();
}

extern "C" void VkImgui_DrawInspector(void) {
	if ( !vkWindows.inspector.open ) return;
	ImGui::Begin( "Inspector", (bool *)&vkWindows.inspector.open );
	ImGui::TextDisabled( "(?)" );
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip(
			"Selection comes from Objects, Shaders list, or clear with Objects panel button." );
	}
	ImGui::Separator();
#ifdef USE_VULKAN
	if ( vkInspector.inspectorSelectionKind == VK_INSP_KIND_WORLD && VkImgScene_WorldLoaded() ) {
		char wn[MAX_QPATH];
		char wb[MAX_QPATH];

		VkImgScene_WorldName( wn, sizeof( wn ) );
		VkImgScene_WorldBaseName( wb, sizeof( wb ) );
		ImGui::SeparatorText( "World / BSP" );
		ImGui::TextWrapped( "%s", wn );
		ImGui::Text( "Base name: %s", wb );
		ImGui::Text( "Clusters: %d  clusterBytes: %d", VkImgScene_WorldNumClusters(),
			VkImgScene_WorldClusterBytes() );
		ImGui::Text( "Surfaces: %d  Shaders (map): %d", VkImgScene_WorldNumSurfaces(),
			VkImgScene_WorldNumMapShaders() );
		ImGui::Text( "Submodels: %d", VkImgScene_WorldNumBModels() );
	} else if ( vkInspector.inspectorSelectionKind == VK_INSP_KIND_ENTITY &&
		    vkInspector.inspectorEntityIndex >= 0 &&
		    vkInspector.inspectorEntityIndex < VkImgScene_RefEntityCount() ) {
		vkImgSceneRefEntity_t se;
		char mpath[MAX_QPATH];

		if ( VkImgScene_RefEntitySnapshot( vkInspector.inspectorEntityIndex, &se ) ) {
			ImGui::SeparatorText( "Ref entity" );
			ImGui::Text( "Index: %d", vkInspector.inspectorEntityIndex );
			ImGui::Text( "reType: %s", VkImgui_ReTypeLabel( (refEntityType_t)se.reType ) );
			ImGui::Text( "renderfx: 0x%x", se.renderfx );
			ImGui::Text( "origin: %.3f %.3f %.3f", se.origin[0], se.origin[1], se.origin[2] );
			ImGui::Text( "frame %d / old %d  backlerp %.3f", se.frame, se.oldframe, se.backlerp );
			ImGui::Text( "model handle: %d", se.hModel );
			VkImgScene_RefEntityModelPath( vkInspector.inspectorEntityIndex, mpath, sizeof( mpath ) );
			if ( mpath[0] ) {
				ImGui::Text( "model path: %s", mpath );
			}
			ImGui::Text( "model type: %s", VkImgui_ModTypeLabel( se.modelModType ) );
			if ( se.customShaderName[0] ) {
				ImGui::Text( "customShader: %s", se.customShaderName );
			} else if ( se.customShader ) {
				ImGui::Text( "customShader handle: %d", se.customShader );
			}
			if ( se.customSkin ) {
				ImGui::Text( "customSkin handle: %d", se.customSkin );
			}
			ImGui::SeparatorText( "Renderer lighting (entity)" );
			ImGui::Text( "ambient: %.2f %.2f %.2f", se.ambientLight[0], se.ambientLight[1], se.ambientLight[2] );
			ImGui::Text( "directed: %.2f %.2f %.2f", se.directedLight[0], se.directedLight[1],
				se.directedLight[2] );
			ImGui::Text( "lightDir: %.3f %.3f %.3f", se.lightDir[0], se.lightDir[1], se.lightDir[2] );
			if ( se.morphChannelCount > 0 ) {
				int c;
				int morphCap;

				ImGui::SeparatorText( "Morph (IQM/GLTF)" );
				ImGui::Text( "Channels: %d", se.morphChannelCount );
				morphCap = se.morphChannelCount;
				if ( morphCap > VK_IMGUI_SCENE_MORPH_MAX ) {
					morphCap = VK_IMGUI_SCENE_MORPH_MAX;
				}
				for ( c = 0; c < morphCap; c++ ) {
					ImGui::Text( "[%d] #%08x weight %.4f prev %.4f", c,
						se.morphHashes[c], se.morphWeights[c],
						se.morphWeightPrev[c] );
				}
			}
		}
	} else if ( vkInspector.inspectorSelectionKind == VK_INSP_KIND_DLIGHT &&
		    vkInspector.inspectorDlightIndex >= 0 &&
		    vkInspector.inspectorDlightIndex < (int)VkImgScene_DlightCount() ) {
		vkImgSceneDlight_t dl;

		if ( VkImgScene_DlightSnapshot( vkInspector.inspectorDlightIndex, &dl ) ) {
			ImGui::SeparatorText( "Dynamic light" );
			ImGui::Text( "Index: %d", vkInspector.inspectorDlightIndex );
			ImGui::Text( "origin: %.3f %.3f %.3f", dl.origin[0], dl.origin[1], dl.origin[2] );
			ImGui::Text( "axis end: %.3f %.3f %.3f", dl.origin2[0], dl.origin2[1], dl.origin2[2] );
			ImGui::Text( "radius: %.2f  linear:%s", dl.radius, dl.linear ? "y" : "n" );
			ImGui::Text( "additive:%s", dl.additive ? "y" : "n" );
			ImGui::Text( "color: %.3f %.3f %.3f", dl.color[0], dl.color[1], dl.color[2] );
		}
	} else if ( vkInspector.shader.active && vkInspector.shader.index >= 0 &&
		    vkInspector.shader.index < VkImgScene_NumSortedShaders() ) {
		vkImgSceneShader_t shsnap;

		if ( VkImgScene_SortedShaderSnapshot( vkInspector.shader.index, &shsnap ) ) {
			ImGui::SeparatorText( "Shader" );
			ImGui::TextWrapped( "%s", shsnap.name );
			ImGui::Text( "sorted index: %d  internal index %d", vkInspector.shader.index,
				shsnap.indexInternal );
			ImGui::Text( "explicit: %s  sky:%s merge:%s",
				shsnap.explicitlyDefined ? "y" : "n",
				shsnap.isSky ? "y" : "n",
				shsnap.entityMergable ? "y" : "n" );
			ImGui::Text( "cull type: %d  stages: %d", shsnap.cullType, shsnap.numUnfoggedPasses );
			ImGui::Text( "surfaceFlags 0x%x  contents 0x%x", shsnap.surfaceFlags, shsnap.contentFlags );
			ImGui::TextDisabled( "Remap/copy to new name: use radiant + .shader or engine console." );
		} else {
			ImGui::TextDisabled( "Invalid shader row." );
		}
	} else {
		ImGui::TextDisabled( "No selection." );
		ImGui::BulletText( "Pick a row in Objects (world / entity / dlight)." );
		ImGui::BulletText( "Pick a shader in the Shaders panel." );
	}
#else
	ImGui::TextWrapped( "Inspector requires Vulkan renderer + ImGui overlay." );
#endif
	ImGui::End();
}

#endif /* USE_IMGUI */
