/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

PostFX inspector panel (bloom, tonemap, SSR, etc.).
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"
#include "vk_imgui_draw_defaults.hpp"

typedef struct {
	float threshold;
	float intensity;
	int tonemapMode;
	float exposure;
	float vignette;
	float vigRadius;
	float chromAb;
	float grain;
	float maxDist;
	float stepSz;
	float thick;
	float ssrIntensity;
} vkPostFxPanelState_t;

static const vkPostFxPanelState_t vkPostFxDefaults = {
	0.6f, 0.5f, 2, 1.0f,
	0.0f, 0.75f, 0.0f, 0.0f,
	100.0f, 1.0f, 0.5f, 0.8f
};

static vkPostFxPanelState_t vkPostFxState = vkPostFxDefaults;

static void VkImgui_ResetPostFxDefaults( void )
{
	vkPostFxState = vkPostFxDefaults;
	ri.Cvar_SetValue( "r_bloom_threshold", 0.6f );
	ri.Cvar_SetValue( "r_bloom_intensity", 0.5f );
	ri.Cvar_SetValue( "r_bloomKnee", 0.5f );
	ri.Cvar_Set( "r_bloom_threshold_mode", "0" );
	ri.Cvar_Set( "r_bloom_modulate", "0" );
	ri.Cvar_Set( "r_tonemap", "3" );
	ri.Cvar_SetValue( "r_exposure", 0.82f );
	ri.Cvar_Set( "r_post", "1" );
	ri.Cvar_SetValue( "r_vignette", 0.55f );
	ri.Cvar_SetValue( "r_vignette_radius", 0.60f );
	ri.Cvar_SetValue( "r_chromaticAberration", 0.22f );
	ri.Cvar_SetValue( "r_filmGrain", 0.75f );
	ri.Cvar_SetValue( "r_ssr_maxDistance", 100.0f );
	ri.Cvar_SetValue( "r_ssr_stepSize", 1.0f );
	ri.Cvar_SetValue( "r_ssr_thickness", 0.5f );
	ri.Cvar_SetValue( "r_ssr_intensity", 0.8f );
}

extern "C" void VkImgui_DrawPostFXPanel(void) {
	if (!vkWindows.postfx.open) return;
	ImGui::Begin("PostFX", (bool *)&vkWindows.postfx.open);
	VkImgui_DrawDefaultsConfirmation(
		"Defaults##PostFX",
		"ConfirmDefaultsPostFX",
		"PostFX",
		VkImgui_ResetPostFxDefaults
	);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if ( ImGui::IsItemHovered() ) {
		ImGui::SetTooltip( "Apply baseline values. All controls drive engine cvars." );
	}
	ImGui::Separator();
	ImGui::BeginChild("PostFXScrollRegion", ImVec2(0.0f, 0.0f), qfalse, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	/* Pipeline toggle */
	{
		int on = ri.Cvar_VariableIntegerValue( "r_post" );
		bool enabled = ( on != 0 );
		if ( ImGui::Checkbox( "Enable Post-Processing", &enabled ) ) {
			ri.Cvar_Set( "r_post", enabled ? "1" : "0" );
		}
		int dbg = ri.Cvar_VariableIntegerValue( "r_post_debug" );
		const char *dbgModes[] = { "Final", "Pre-tonemap HDR", "Luminance heatmap",
			"Panini logical UV (97)", "Panini remapped UV (98)", "Panini OOB mask (99)" };
		int dbgCombo = ( dbg == 97 ) ? 3 : ( dbg == 98 ) ? 4 : ( dbg == 99 ) ? 5 : ( dbg >= 1 && dbg <= 2 ) ? dbg : 0;
		if ( ImGui::Combo( "Debug View", &dbgCombo, dbgModes, 6 ) ) {
			int v = ( dbgCombo == 3 ) ? 97 : ( dbgCombo == 4 ) ? 98 : ( dbgCombo == 5 ) ? 99 : dbgCombo;
			ri.Cvar_Set( "r_post_debug", va( "%d", v ) );
		}
	}

	if (ImGui::CollapsingHeader("Sky / Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
		int atmOn = ri.Cvar_VariableIntegerValue( "r_atmosphere" );
		bool atmEnabled = ( atmOn != 0 );
		if ( ImGui::Checkbox( "Procedural atmospheric sky", &atmEnabled ) ) {
			ri.Cvar_Set( "r_atmosphere", atmEnabled ? "1" : "0" );
		}
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() ) {
			ImGui::SetTooltip( "Rayleigh+Mie scattering sky. Replaces grey sky when no HDR skybox. Use r_skyboxHDR for custom EXR panorama." );
		}
		if ( atmEnabled ) {
			float sunX = VkImgui_CvarFloat( "r_atmosphere_sunDirX" );
			float sunY = VkImgui_CvarFloat( "r_atmosphere_sunDirY" );
			float sunZ = VkImgui_CvarFloat( "r_atmosphere_sunDirZ" );
			float sunInt = VkImgui_CvarFloat( "r_atmosphere_sunIntensity" );
			float atmScale = VkImgui_CvarFloat( "r_atmosphere_scale" );
			VkImgui_CvarSlider( "Sun dir X", "r_atmosphere_sunDirX", sunX, -1.0f, 1.0f );
			VkImgui_CvarSlider( "Sun dir Y", "r_atmosphere_sunDirY", sunY, -1.0f, 1.0f );
			VkImgui_CvarSlider( "Sun dir Z", "r_atmosphere_sunDirZ", sunZ, -1.0f, 1.0f );
			VkImgui_CvarSlider( "Sun intensity", "r_atmosphere_sunIntensity", sunInt, 1.0f, 100.0f );
			VkImgui_CvarSlider( "Sky scale (HDR)", "r_atmosphere_scale", atmScale, 0.5f, 16.0f );
		}
	}

	if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
		int bloomOn = ri.Cvar_VariableIntegerValue( "r_bloom" );
		bool bloomEnabled = ( bloomOn != 0 );
		if ( ImGui::Checkbox( "Enable Bloom", &bloomEnabled ) ) {
			ri.Cvar_Set( "r_bloom", bloomEnabled ? "1" : "0" );
		}
		float thresh = VkImgui_CvarFloat( "r_bloom_threshold" );
		float intensity = VkImgui_CvarFloat( "r_bloom_intensity" );
		float knee = VkImgui_CvarFloat( "r_bloomKnee" );
		int threshMode = ri.Cvar_VariableIntegerValue( "r_bloom_threshold_mode" );
		int modulate = ri.Cvar_VariableIntegerValue( "r_bloom_modulate" );
		VkImgui_CvarSlider( "Threshold", "r_bloom_threshold", thresh, 0.0f, 2.0f );
		VkImgui_CvarSlider( "Intensity", "r_bloom_intensity", intensity, 0.0f, 2.0f );
		VkImgui_CvarSlider( "Knee", "r_bloomKnee", knee, 0.0f, 4.0f );
		const char *threshModes[] = { "RGB >= thresh", "Avg >= thresh", "Luma >= thresh" };
		if ( ImGui::Combo( "Threshold Mode", &threshMode, threshModes, 3 ) ) {
			ri.Cvar_Set( "r_bloom_threshold_mode", va( "%d", threshMode ) );
		}
		const char *modModes[] = { "Off", "Color^2", "Color * Luma" };
		if ( ImGui::Combo( "Modulate", &modulate, modModes, 3 ) ) {
			ri.Cvar_Set( "r_bloom_modulate", va( "%d", modulate ) );
		}
	}

	if (ImGui::CollapsingHeader("SSAO / HBAO")) {
		int ssaoOn = ri.Cvar_VariableIntegerValue( "r_ssao" );
		bool ssaoEnabled = ( ssaoOn != 0 );
		if ( ImGui::Checkbox( "Enable SSAO/HBAO", &ssaoEnabled ) ) {
			ri.Cvar_Set( "r_ssao", ssaoEnabled ? "1" : "0" );
		}
		if ( ssaoEnabled ) {
			ImGui::SameLine();
			ImGui::TextDisabled( "(?)" );
			if ( ImGui::IsItemHovered() )
				ImGui::SetTooltip( "Screen-space ambient occlusion. Requires r_fbo 1. vid_restart after toggle." );
			int method = ri.Cvar_VariableIntegerValue( "r_ssaoMethod" );
			const char *methods[] = { "SSAO (sphere)", "HBAO (horizon)" };
			if ( ImGui::Combo( "Method", &method, methods, 2 ) )
				ri.Cvar_Set( "r_ssaoMethod", va( "%d", method ) );
			if ( method == 1 ) {
				int dirs = ri.Cvar_VariableIntegerValue( "r_hbaoDirections" );
				int steps = ri.Cvar_VariableIntegerValue( "r_hbaoSteps" );
				if ( ImGui::SliderInt( "HBAO Directions", &dirs, 4, 16 ) )
					ri.Cvar_Set( "r_hbaoDirections", va( "%d", dirs ) );
				if ( ImGui::SliderInt( "HBAO Steps", &steps, 2, 8 ) )
					ri.Cvar_Set( "r_hbaoSteps", va( "%d", steps ) );
			} else {
				int samples = ri.Cvar_VariableIntegerValue( "r_ssaoSamples" );
				if ( ImGui::SliderInt( "SSAO Samples", &samples, 4, 32 ) )
					ri.Cvar_Set( "r_ssaoSamples", va( "%d", samples ) );
			}
			float radius = VkImgui_CvarFloat( "r_ssaoRadius" );
			float bias = VkImgui_CvarFloat( "r_ssaoBias" );
			float intensity = VkImgui_CvarFloat( "r_ssaoIntensity" );
			float power = VkImgui_CvarFloat( "r_ssaoPower" );
			VkImgui_CvarSlider( "Radius", "r_ssaoRadius", radius, 0.1f, 4.0f );
			VkImgui_CvarSlider( "Bias", "r_ssaoBias", bias, 0.0f, 0.1f, "%.4f" );
			VkImgui_CvarSlider( "Intensity", "r_ssaoIntensity", intensity, 0.0f, 4.0f );
			VkImgui_CvarSlider( "Power", "r_ssaoPower", power, 0.5f, 4.0f );
			int blurRadius = ri.Cvar_VariableIntegerValue( "r_ssaoBlurRadius" );
			if ( ImGui::SliderInt( "Blur Radius", &blurRadius, 0, 8 ) )
				ri.Cvar_Set( "r_ssaoBlurRadius", va( "%d", blurRadius ) );
		}
	}

	if (ImGui::CollapsingHeader("Tonemapping", ImGuiTreeNodeFlags_DefaultOpen)) {
		int tonemap = ri.Cvar_VariableIntegerValue( "r_tonemap" );
		float exposure = VkImgui_CvarFloat( "r_exposure" );
		const char *modes[] = { "None", "Reinhard", "ACES", "Filmic", "AgX", "Neutral reference" };
		if ( ImGui::Combo( "Mode", &tonemap, modes, 6 ) ) {
			ri.Cvar_Set( "r_tonemap", va( "%d", tonemap ) );
		}
		VkImgui_CvarSlider( "Exposure", "r_exposure", exposure, 0.01f, 10.0f );
		int expAuto = ri.Cvar_Get( "r_exposure_auto", "0", 0 )->integer;
		bool expAutoOn = ( expAuto != 0 );
		if ( ImGui::Checkbox( "Eye adaptation (auto exposure)", &expAutoOn ) ) {
			ri.Cvar_SetValue( "r_exposure_auto", expAutoOn ? 1.0f : 0.0f );
		}
		if ( expAutoOn ) {
			ImGui::SameLine();
			ImGui::TextDisabled( "(?)" );
			if ( ImGui::IsItemHovered() ) {
				ImGui::SetTooltip( "Temporal adaptation toward target. Luminance pass planned for full Source Lost Coast style." );
			}
			float expTarget = VkImgui_CvarFloat( "r_exposure_auto_target" );
			float expSpeed = VkImgui_CvarFloat( "r_exposure_auto_speed" );
			VkImgui_CvarSlider( "Adapt target", "r_exposure_auto_target", expTarget, 0.1f, 2.0f );
			VkImgui_CvarSlider( "Adapt speed", "r_exposure_auto_speed", expSpeed, 0.1f, 10.0f );
		}
		float lmScale = VkImgui_CvarFloat( "r_hdr_lightmap_scale" );
		VkImgui_CvarSlider( "HDR lightmap scale", "r_hdr_lightmap_scale", lmScale, 0.5f, 8.0f );
		if ( VkImgui_CvarFloat( "r_hdr" ) > 0.0f ) {
			bool lmSrgb = VkImgui_CvarFloat( "r_lightmap_srgb_decode" ) > 0.5f;
			if ( ImGui::Checkbox( "Lightmap sRGB decode", &lmSrgb ) )
				ri.Cvar_Set( "r_lightmap_srgb_decode", lmSrgb ? "1" : "0" );
		}
		float preExp = VkImgui_CvarFloat( "r_pre_exposure_scale" );
		VkImgui_CvarSlider( "Pre-exposure scale", "r_pre_exposure_scale", preExp, 0.1f, 4.0f );
		float postContrast = VkImgui_CvarFloat( "r_post_contrast" );
		float postSaturation = VkImgui_CvarFloat( "r_post_saturation" );
		VkImgui_CvarSlider( "Contrast", "r_post_contrast", postContrast, 0.25f, 4.0f );
		VkImgui_CvarSlider( "Saturation", "r_post_saturation", postSaturation, 0.0f, 3.0f );
	}

	if (ImGui::CollapsingHeader("PBR")) {
		bool pbrOn = ri.Cvar_VariableIntegerValue( "r_pbr" ) != 0;
		if ( ImGui::Checkbox( "Enable PBR", &pbrOn ) ) {
			ri.Cvar_Set( "r_pbr", pbrOn ? "1" : "0" );
		}
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Physically Based Rendering. Requires r_fbo 1. Changes need vid_restart." );
		bool fboOn = ri.Cvar_VariableIntegerValue( "r_fbo" ) != 0;
		if ( !fboOn && pbrOn ) {
			ImGui::SameLine();
			ImGui::TextColored( ImVec4( 1.0f, 0.5f, 0.0f, 1.0f ), "r_fbo 1 required" );
		}
	}

	if (ImGui::CollapsingHeader("Anti-Aliasing")) {
		int msaa = ri.Cvar_VariableIntegerValue( "r_ext_multisample" );
		const char *msaaModes[] = { "Off", "2x", "4x", "8x", "16x" };
		int msaaIdx = ( msaa <= 0 ) ? 0 : ( msaa <= 2 ) ? 1 : ( msaa <= 4 ) ? 2 : ( msaa <= 8 ) ? 3 : 4;
		if ( ImGui::Combo( "MSAA", &msaaIdx, msaaModes, 5 ) ) {
			int v = ( msaaIdx == 0 ) ? 0 : ( msaaIdx == 1 ) ? 2 : ( msaaIdx == 2 ) ? 4 : ( msaaIdx == 3 ) ? 8 : 16;
			ri.Cvar_Set( "r_ext_multisample", va( "%d", v ) );
		}
		if ( msaa > 0 ) {
			bool sampleShading = ri.Cvar_VariableIntegerValue( "r_msaa_sample_shading" ) != 0;
			if ( ImGui::Checkbox( "Sample shading (better alpha, ~2x cost)", &sampleShading ) )
				ri.Cvar_Set( "r_msaa_sample_shading", sampleShading ? "1" : "0" );
			bool alphaToCov = ri.Cvar_VariableIntegerValue( "r_ext_alpha_to_coverage" ) != 0;
			if ( ImGui::Checkbox( "Alpha-to-coverage (foliage, grates)", &alphaToCov ) )
				ri.Cvar_Set( "r_ext_alpha_to_coverage", alphaToCov ? "1" : "0" );
		}
		bool smaaOn = ri.Cvar_VariableIntegerValue( "r_ext_smaa" ) != 0;
		if ( ImGui::Checkbox( "SMAA (post-process)", &smaaOn ) )
			ri.Cvar_Set( "r_ext_smaa", smaaOn ? "1" : "0" );
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "MSAA + SMAA: geometry + alpha edges. MSAA/SMAA changes need vid_restart." );
	}

	if (ImGui::CollapsingHeader("Lens Effects")) {
		float vignette = VkImgui_CvarFloat( "r_vignette" );
		float vigRadius = VkImgui_CvarFloat( "r_vignette_radius" );
		float chromAb = VkImgui_CvarFloat( "r_chromaticAberration" );
		float grain = VkImgui_CvarFloat( "r_filmGrain" );
		VkImgui_CvarSlider( "Vignette", "r_vignette", vignette, 0.0f, 1.0f );
		VkImgui_CvarSlider( "Vignette Radius", "r_vignette_radius", vigRadius, 0.3f, 1.5f );
		VkImgui_CvarSlider( "Chromatic Aberration", "r_chromaticAberration", chromAb, 0.0f, 1.0f );
		VkImgui_CvarSlider( "Film Grain", "r_filmGrain", grain, 0.0f, 1.0f );
	}

	if (ImGui::CollapsingHeader("Detail Textures")) {
		int dtOn = ri.Cvar_VariableIntegerValue( "r_detailtextures" );
		bool dtEnabled = ( dtOn != 0 );
		if ( ImGui::Checkbox( "Enable detail maps", &dtEnabled ) ) {
			ri.Cvar_Set( "r_detailtextures", dtEnabled ? "1" : "0" );
		}
		ImGui::SameLine();
		ImGui::TextDisabled( "(?)" );
		if ( ImGui::IsItemHovered() ) {
			ImGui::SetTooltip( "High-frequency tiling overlay for PBR materials. Use detailMap in shaders or _detail suffix." );
		}
		float dtScale = VkImgui_CvarFloat( "r_detail_scale" );
		VkImgui_CvarSlider( "Detail scale (tiling)", "r_detail_scale", dtScale, 0.5f, 32.0f );
	}

#ifdef USE_VULKAN_RTX
	if ( ImGui::CollapsingHeader( "Path trace (RTX experiment)" ) ) {
		int ptOn = ri.Cvar_VariableIntegerValue( "r_pathtrace" );
		bool ptEnabled = ( ptOn != 0 );
		if ( ImGui::Checkbox( "r_pathtrace", &ptEnabled ) ) {
			ri.Cvar_Set( "r_pathtrace", ptEnabled ? "1" : "0" );
			ImGui::TextDisabled( "Requires vid_restart" );
		}
		if ( ptEnabled ) {
			char archBuf[32];
			Com_sprintf( archBuf, sizeof( archBuf ), "%s", ri.Cvar_VariableString( "r_pathtrace_arch" ) );
			if ( ImGui::BeginCombo( "r_pathtrace_arch", archBuf[0] ? archBuf : "megakernel" ) ) {
				if ( ImGui::Selectable( "megakernel", Q_stricmp( archBuf, "megakernel" ) == 0 ) ) {
					ri.Cvar_Set( "r_pathtrace_arch", "megakernel" );
				}
				if ( ImGui::Selectable( "wavefront", Q_stricmp( archBuf, "wavefront" ) == 0 ) ) {
					ri.Cvar_Set( "r_pathtrace_arch", "wavefront" );
				}
				ImGui::EndCombo();
			}
			int bounces = ri.Cvar_VariableIntegerValue( "r_pathtrace_bounces" );
			int samples = ri.Cvar_VariableIntegerValue( "r_pathtrace_samples" );
			int denoise = ri.Cvar_VariableIntegerValue( "r_pathtrace_denoise" );
			int dbg = ri.Cvar_VariableIntegerValue( "r_pathtrace_debug" );
			float composite = VkImgui_CvarFloat( "r_pathtrace_composite" );
			if ( ImGui::SliderInt( "bounces", &bounces, 1, 8 ) ) {
				ri.Cvar_SetValue( "r_pathtrace_bounces", (float)bounces );
			}
			if ( ImGui::SliderInt( "samples", &samples, 1, 64 ) ) {
				ri.Cvar_SetValue( "r_pathtrace_samples", (float)samples );
			}
			{
				bool denoiseOn = ( denoise != 0 );
				if ( ImGui::Checkbox( "denoise", &denoiseOn ) ) {
					ri.Cvar_Set( "r_pathtrace_denoise", denoiseOn ? "1" : "0" );
				}
			}
			if ( ImGui::SliderInt( "debug", &dbg, 0, 2 ) ) {
				ri.Cvar_SetValue( "r_pathtrace_debug", (float)dbg );
			}
			VkImgui_CvarSlider( "composite blend", "r_pathtrace_composite", composite, 0.0f, 1.0f );
			ImGui::TextDisabled( "Also needs r_rtx 1, r_rtxDemo 1 + vid_restart" );
		}
	}
#endif

	if (ImGui::CollapsingHeader("SSR")) {
		int ssrOn = ri.Cvar_VariableIntegerValue( "r_ssr" );
		bool ssrEnabled = ( ssrOn != 0 );
		if ( ImGui::Checkbox( "Enable SSR", &ssrEnabled ) ) {
			ri.Cvar_Set( "r_ssr", ssrEnabled ? "1" : "0" );
		}
		float maxDist = VkImgui_CvarFloat( "r_ssr_maxDistance" );
		float stepSz = VkImgui_CvarFloat( "r_ssr_stepSize" );
		float thick = VkImgui_CvarFloat( "r_ssr_thickness" );
		float ssrInt = VkImgui_CvarFloat( "r_ssr_intensity" );
		float maxGrad = VkImgui_CvarFloat( "r_ssr_maxDepthGradient" );
		VkImgui_CvarSlider( "Max Distance", "r_ssr_maxDistance", maxDist, 10.0f, 500.0f );
		VkImgui_CvarSlider( "Step Size", "r_ssr_stepSize", stepSz, 0.1f, 5.0f );
		VkImgui_CvarSlider( "Thickness", "r_ssr_thickness", thick, 0.1f, 5.0f );
		VkImgui_CvarSlider( "Intensity", "r_ssr_intensity", ssrInt, 0.0f, 1.0f );
		VkImgui_CvarSlider( "Max depth gradient", "r_ssr_maxDepthGradient", maxGrad, 0.01f, 0.5f, "%.3f" );
		if ( ImGui::IsItemHovered() )
			ImGui::SetTooltip( "Skip SSR at depth edges (object silhouettes, horizon) to reduce thin line artifacts. Lower = stricter." );
	}

	ImGui::EndChild();
	ImGui::End();
}

#endif /* USE_IMGUI */
