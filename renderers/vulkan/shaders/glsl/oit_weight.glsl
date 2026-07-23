/* Color Pipeline Phase 2.5 — bounded WBOIT weight evaluation.
 * Must match vk_oit_weight_contract frozen coeffs (BOUNDED_PRODUCTION).
 * See docs/WBOIT_WEIGHT_CONTRACT.md.
 */
#ifndef OIT_WEIGHT_GLSL
#define OIT_WEIGHT_GLSL

#include "depth_view.glsl"

/* Frozen production coefficients (hash-locked on CPU). */
const float OIT_W_MIN = 1e-2;
const float OIT_W_MAX = 3e3;
const float OIT_W_ALPHA_EXP = 3.0;
const float OIT_W_DEPTH_EXP = 3.0;
const float OIT_W_DEPTH_SCALE = 0.9;
const float OIT_W_MIN_OPACITY = 0.01;
const float OIT_W_ALPHA_GAIN = 10.0;
const float OIT_W_LUMA_SCALE = 1e3;

/*
 * BOUNDED_PRODUCTION weight.
 * viewDepth = certified positive view-depth (−viewSpace.z).
 * zNear/zFar = cluster / contract clamps.
 */
float OitWeight_BoundedProduction( float opacity, float viewDepth, float zNear, float zFar )
{
	float a = clamp( opacity, 0.0, 1.0 );
	float zTrad = Depth_ViewDepthToTraditional01( viewDepth, zNear, zFar );
	float aFactor = pow( min( 1.0, a * OIT_W_ALPHA_GAIN ) + OIT_W_MIN_OPACITY, OIT_W_ALPHA_EXP );
	float zFactor = pow( 1.0 - zTrad * OIT_W_DEPTH_SCALE, OIT_W_DEPTH_EXP );
	return clamp( aFactor * OIT_W_LUMA_SCALE * zFactor, OIT_W_MIN, OIT_W_MAX );
}

/* Laboratory: coverage-proportional weight (no depth term). */
float OitWeight_AlphaReference( float opacity )
{
	return clamp( opacity, OIT_W_MIN, OIT_W_MAX );
}

#endif
