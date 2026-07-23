/* Color Pipeline Phase 2.3.2 — positive view-depth reconstruction.
 * Certified metric: meters along camera forward (−viewSpace.z).
 * Forbidden for fog/weight: raw device depth without linearization.
 * See docs/DEPTH_CONTRACT.md.
 */
#ifndef DEPTH_VIEW_GLSL
#define DEPTH_VIEW_GLSL

/*
 * Vulkan finite reversed-Z: device depth near→1, far→0, clear=0.
 * Matches taa.frag / postfx depthParams linearization.
 */
float Depth_LinearizeReversedZ( float deviceDepth, float zNear, float zFar )
{
	float zn = max( zNear, 1e-4 );
	float zf = max( zFar, zn + 1e-3 );
	float d = clamp( deviceDepth, 0.0, 1.0 );
	return ( zn * zf ) / max( zn + d * ( zf - zn ), 1e-6 );
}

/* Preferred: world position + camera forward (Q3 axis[0]). */
float Depth_PositiveViewFromWorld( vec3 worldPos, vec3 viewOrg, vec3 viewForward )
{
	return max( dot( worldPos - viewOrg, normalize( viewForward ) ), 0.0 );
}

/* Legacy (pre-2.3.2): Euclidean camera distance — not the certified metric. */
float Depth_CameraDistance( vec3 worldPos, vec3 viewOrg )
{
	return length( worldPos - viewOrg );
}

/*
 * Reconstruct certified positive view-depth for a fragment.
 * Primary: linearize the fragment's projected device depth (equals −viewZ).
 * Fallback z-range: cluster SSBO or push constants.
 */
float Depth_ReconstructPositiveViewDepth( float deviceDepth, float zNear, float zFar )
{
	return Depth_LinearizeReversedZ( deviceDepth, zNear, zFar );
}

/* Normalize view-depth to [0,1] for weight curves (near→0, far→1). */
float Depth_ViewDepthToTraditional01( float viewDepth, float zNear, float zFar )
{
	float zn = max( zNear, 1e-4 );
	float zf = max( zFar, zn + 1e-3 );
	return clamp( ( viewDepth - zn ) / ( zf - zn ), 0.0, 1.0 );
}

#endif
