#version 450

/*
 * Temporal Reconstruction AA (confidence-guided).
 * History weight = base × confidence (depth × velocity × luma × reactive).
 * YCoCg variance clipping replaces naïve RGB min/max clamp.
 */

layout(set = 0, binding = 0) uniform sampler2D currentColor;
layout(set = 1, binding = 0) uniform sampler2D depthTex;
layout(set = 2, binding = 0) uniform PostFXParams {
	mat4 invViewProj;
	mat4 prevViewProj;
	mat4 viewMatrix;
	vec4 motionBlur;
	vec4 depthOfField;
	vec4 frameInfo;
	vec4 depthParams;
	vec4 toneMapParams0;
	vec4 toneMapParams1;
	vec4 colorBalance;
	vec4 colorGrade;
	vec4 colorGrade2;
	vec4 shadowsLift;
	vec4 midsGamma;
	vec4 highlightsGain;
	vec4 splitShadow;
	vec4 splitHighlight;
	vec4 lensEffects0;
	vec4 lensEffects1;
	vec4 runtimeFlags;
	vec4 lutParams;
	vec4 autoExposureParams;
	vec4 localExposureParams;
	vec4 taaParams;
	vec4 temporalValidity;
	vec4 weaponTemporalParams;
	vec4 temporalDebugParams;
} postfx;
layout(set = 3, binding = 0) uniform sampler2D historyColor;
layout(set = 4, binding = 0) uniform sampler2D motionTex;
layout(set = 5, binding = 0) uniform sampler2D reactiveMaskTex;
layout(set = 6, binding = 0) uniform sampler2D temporalClassTex;
layout(set = 7, binding = 0) uniform sampler2D previousDepthTex;
/* Dynamic-object identity (R32_UINT): packed (reversedZ<<16 | stableId16). */
layout(set = 8, binding = 0) uniform usampler2D currentObjectId;
layout(set = 9, binding = 0) uniform usampler2D previousObjectId;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

const vec3 LUMA = vec3( 0.2126, 0.7152, 0.0722 );
/* Encoded as R8: WORLD=0, WEAPON≈1, SKY≈2/255 unused for now. */
const float CLASS_WEAPON_THRESH = 0.5;

vec2 texelSize( void ) {
	return postfx.frameInfo.yz;
}

vec3 sampleCurrent( vec2 uv ) {
	return textureLod( currentColor, clamp( uv, 0.0, 1.0 ), 0.0 ).rgb;
}

vec3 RGBToYCoCg( vec3 c ) {
	float Y = dot( c, vec3( 0.25, 0.5, 0.25 ) );
	float Co = dot( c, vec3( 0.5, 0.0, -0.5 ) );
	float Cg = dot( c, vec3( -0.25, 0.5, -0.25 ) );
	return vec3( Y, Co, Cg );
}

vec3 YCoCgToRGB( vec3 ycocg ) {
	float Y = ycocg.x;
	float Co = ycocg.y;
	float Cg = ycocg.z;
	return vec3( Y + Co - Cg, Y + Cg, Y - Co - Cg );
}

vec2 reprojectHistoryUV( vec2 uv, float depthNdc, out float previousDepthNdc ) {
	vec4 posClip = vec4( uv * 2.0 - 1.0, depthNdc, 1.0 );
	vec4 posWorld = postfx.invViewProj * posClip;
	posWorld /= max( posWorld.w, 1e-6 );
	vec4 prevClip = postfx.prevViewProj * posWorld;
	previousDepthNdc = prevClip.z / max( prevClip.w, 1e-6 );
	return prevClip.xy / max( prevClip.w, 1e-6 ) * 0.5 + 0.5;
}

/* Vulkan world projection is finite reversed-Z: near=1, far=0. */
float linearizeReversedDepth( float depthNdc ) {
	float zNear = max( postfx.depthParams.x, 1e-4 );
	float zFar = max( postfx.depthParams.y, zNear + 1e-3 );
	return ( zNear * zFar ) /
		max( zNear + clamp( depthNdc, 0.0, 1.0 ) * ( zFar - zNear ), 1e-6 );
}

void neighborhoodYCoCgStats( vec2 uv, out vec3 meanY, out vec3 sigmaY ) {
	vec2 texel = texelSize();
	vec3 acc = vec3( 0.0 );
	vec3 acc2 = vec3( 0.0 );
	for ( int y = -1; y <= 1; ++y ) {
		for ( int x = -1; x <= 1; ++x ) {
			vec3 ycc = RGBToYCoCg( sampleCurrent( uv + vec2( x, y ) * texel ) );
			acc += ycc;
			acc2 += ycc * ycc;
		}
	}
	meanY = acc * ( 1.0 / 9.0 );
	vec3 varY = max( acc2 * ( 1.0 / 9.0 ) - meanY * meanY, vec3( 1e-6 ) );
	sigmaY = sqrt( varY );
}

vec3 applyResolveSharpen( vec2 uv, vec3 resolved ) {
	float sharpen = clamp( postfx.taaParams.w, 0.0, 1.0 );
	if ( sharpen <= 0.0 ) {
		return resolved;
	}
	vec2 texel = texelSize();
	vec3 blur = (
		sampleCurrent( uv + vec2( texel.x, 0.0 ) ) +
		sampleCurrent( uv - vec2( texel.x, 0.0 ) ) +
		sampleCurrent( uv + vec2( 0.0, texel.y ) ) +
		sampleCurrent( uv - vec2( 0.0, texel.y ) ) ) * 0.25;
	return max( resolved + ( resolved - blur ) * sharpen * 0.35, vec3( 0.0 ) );
}

/*
 * Edge-aware current-frame neighborhood (SMAA-class spatial fallback).
 * Used when history is rejected under Present-Time Adaptive Reconstruction —
 * not a full-frame SMAA pass over the resolve.
 */
vec3 spatialCurrentFallback( vec2 uv ) {
	vec2 texel = texelSize();
	vec3 c0 = sampleCurrent( uv );
	vec3 cL = sampleCurrent( uv + vec2( -texel.x, 0.0 ) );
	vec3 cR = sampleCurrent( uv + vec2( texel.x, 0.0 ) );
	vec3 cU = sampleCurrent( uv + vec2( 0.0, -texel.y ) );
	vec3 cD = sampleCurrent( uv + vec2( 0.0, texel.y ) );
	float l0 = max( dot( c0, LUMA ), 1e-4 );
	float wL = 1.0 / ( 1.0 + abs( max( dot( cL, LUMA ), 0.0 ) - l0 ) * 8.0 );
	float wR = 1.0 / ( 1.0 + abs( max( dot( cR, LUMA ), 0.0 ) - l0 ) * 8.0 );
	float wU = 1.0 / ( 1.0 + abs( max( dot( cU, LUMA ), 0.0 ) - l0 ) * 8.0 );
	float wD = 1.0 / ( 1.0 + abs( max( dot( cD, LUMA ), 0.0 ) - l0 ) * 8.0 );
	float wSum = 1.0 + wL + wR + wU + wD;
	return ( c0 + cL * wL + cR * wR + cU * wU + cD * wD ) / wSum;
}

void main() {
	vec2 uv = frag_tex_coord;
	vec2 texel = texelSize();
	vec2 sampleUV = uv - postfx.lutParams.zw * texel;
	vec3 current = sampleCurrent( sampleUV );
	float depthNdc = textureLod( depthTex, sampleUV, 0.0 ).r;

	/* colorGrade2.yzw: Temporal Reconstruction flags (gamma uses .x = hue only).
	 * shadowsLift.a: debug mode (gamma uses .rgb only).
	 * highlightsGain.a: adaptive recon pack (>0.5 on). */
	float useVarClip = postfx.colorGrade2.y;
	float useDisocc = postfx.colorGrade2.z;
	float useReactive = postfx.colorGrade2.w;
	float debugMode = postfx.shadowsLift.a;
	float adaptivePack = postfx.highlightsGain.a;
	bool adaptive = adaptivePack >= 1.0;
	bool adaptSpatial = adaptivePack >= 2.0;
	float adaptBudget = adaptive ? clamp( adaptivePack - ( adaptSpatial ? 2.0 : 1.0 ), 0.0, 1.0 ) : 0.0;

	if ( postfx.taaParams.x < 0.5 || postfx.frameInfo.w < 0.5 || depthNdc <= 0.0 || depthNdc >= 1.0 ) {
		/* Phase 9: age / resolve-count views remain useful even with no history. */
		if ( debugMode > 32.5 && debugMode < 33.5 ) {
			float age = postfx.temporalDebugParams.y;
			out_color = age < 0.5 ? vec4( 1.0, 1.0, 0.0, 1.0 ) :
				( age < 1.5 ? vec4( 0.0, 1.0, 0.0, 1.0 ) : vec4( 1.0, 0.0, 0.0, 1.0 ) );
			return;
		}
		if ( debugMode > 33.5 && debugMode < 34.5 ) {
			float resolves = postfx.temporalDebugParams.z;
			out_color = resolves < 0.5 ? vec4( 0.2, 0.2, 0.2, 1.0 ) :
				( resolves < 1.5 ? vec4( 0.0, 1.0, 0.0, 1.0 ) : vec4( 1.0, 0.0, 0.0, 1.0 ) );
			return;
		}
		if ( debugMode > 1.5 ) {
			out_color = vec4( 1.0, 1.0, 1.0, 1.0 ); /* white = cut/reset */
			return;
		}
		out_color = vec4( adaptive && adaptSpatial ? spatialCurrentFallback( sampleUV ) : current, 1.0 );
		return;
	}

	vec2 historyUV;
	vec2 motion = vec2( 0.0 );
	bool mvValid = false;
	float predictedPreviousDepthNdc = depthNdc;
	vec2 matrixHistoryUV = reprojectHistoryUV( sampleUV, depthNdc, predictedPreviousDepthNdc );
	if ( postfx.depthParams.z > 0.5 ) {
		motion = textureLod( motionTex, sampleUV, 0.0 ).rg;
		historyUV = sampleUV - motion;
		mvValid = !( any( isnan( motion ) ) || any( isinf( motion ) ) );
		if ( !mvValid ) {
			historyUV = matrixHistoryUV;
		}
	} else {
		historyUV = matrixHistoryUV;
	}

	if ( any( lessThan( historyUV, vec2( 0.0 ) ) ) || any( greaterThan( historyUV, vec2( 1.0 ) ) ) ) {
		if ( debugMode > 1.5 ) {
			out_color = vec4( 1.0, 0.0, 1.0, 1.0 ); /* magenta = invalid MV / OOB */
			return;
		}
		out_color = vec4( current, 1.0 );
		return;
	}

	if ( debugMode > 0.5 && debugMode < 1.5 ) {
		/* Motion vector debug: encode velocity as color */
		vec2 texSize = vec2( textureSize( currentColor, 0 ) );
		vec2 vel = ( sampleUV - historyUV ) * texSize * 0.05;
		out_color = vec4( abs( vel.x ), abs( vel.y ), 0.15, 1.0 );
		return;
	}

	/* Extended r_temporalDebug views that need depth before full resolve. */
	{
		if ( debugMode > 12.5 && debugMode < 13.5 ) {
			/* 13 = NaN/Inf detection on current / motion */
			bool bad = any( isnan( current ) ) || any( isinf( current ) ) ||
				isnan( depthNdc ) || isinf( depthNdc ) ||
				any( isnan( motion ) ) || any( isinf( motion ) );
			out_color = bad ? vec4( 1.0, 0.0, 1.0, 1.0 ) : vec4( current, 1.0 );
			return;
		}
		if ( debugMode > 13.5 && debugMode < 14.5 ) {
			/* 14 = pre-weapon merged velocity (weapon is not rendered yet). */
			vec2 texSize = vec2( textureSize( currentColor, 0 ) );
			vec2 vel = ( sampleUV - historyUV ) * texSize * 0.05;
			out_color = vec4( abs( vel.x ), abs( vel.y ), 0.15, 1.0 );
			return;
		}
		if ( debugMode > 14.5 && debugMode < 15.5 ) {
			/* 15 = prior-class-gated velocity; 21 is actual post-draw weapon MVP. */
			vec2 texSize = vec2( textureSize( currentColor, 0 ) );
			float priorWeapon = textureLod( temporalClassTex, historyUV, 0.0 ).r > CLASS_WEAPON_THRESH ? 1.0 : 0.0;
			vec2 vel = ( sampleUV - historyUV ) * texSize * 0.05 * priorWeapon;
			out_color = vec4( abs( vel.x ), abs( vel.y ), priorWeapon, 1.0 );
			return;
		}
		/* Modes 16–27 are owned by the post-weapon diagnostic resolve (weapon_taa.frag).
		 * Modes 28–35: world reprojection debugger (Phase 9). */
		if ( debugMode > 27.5 && debugMode < 35.5 ) {
			vec2 texSize = vec2( textureSize( currentColor, 0 ) );
			vec2 velUV = motion; /* VK_VELOCITY_SPACE_UV: currentUV - previousUV */
			vec2 velPx = velUV * texSize;
			float age = postfx.temporalDebugParams.y; /* prevMatricesAge */
			float resolves = postfx.temporalDebugParams.z; /* worldResolvesLastFrame */
			float spaceId = postfx.temporalDebugParams.w; /* 0=UV canonical */

			if ( debugMode < 28.5 ) {
				/* 28 = raw stored velocity (as signed RG, magenta = invalid) */
				out_color = !mvValid ? vec4( 1.0, 0.0, 1.0, 1.0 ) :
					vec4( 0.5 + velUV.x * 10.0, 0.5 + velUV.y * 10.0, 0.15, 1.0 );
				return;
			}
			if ( debugMode < 29.5 ) {
				/* 29 = velocity converted to UV (same as stored — confirms space) */
				out_color = !mvValid ? vec4( 1.0, 0.0, 1.0, 1.0 ) :
					vec4( abs( velUV ) * 20.0, spaceId * 0.25, 1.0 );
				return;
			}
			if ( debugMode < 30.5 ) {
				/* 30 = velocity converted to pixels (abs / 64 so 64px → 1.0) */
				out_color = !mvValid ? vec4( 1.0, 0.0, 1.0, 1.0 ) :
					vec4( abs( velPx ) / 64.0, 0.15, 1.0 );
				return;
			}
			if ( debugMode < 31.5 ) {
				/* 31 = history UV displacement magnitude (green = small, red = large) */
				float disp = length( sampleUV - historyUV ) * max( texSize.x, texSize.y );
				float t = clamp( disp / 64.0, 0.0, 1.0 );
				out_color = vec4( t, 1.0 - t, 0.1, 1.0 );
				return;
			}
			if ( debugMode < 32.5 ) {
				/* 32 = velocity error ratio vs matrix reprojection.
				 * Green ≈ 1.0; yellow ≈ 2x; red ≈ 4x; cyan ≈ 0.5x. */
				vec2 matrixDisp = ( sampleUV - matrixHistoryUV ) * texSize;
				vec2 mvDisp = ( sampleUV - historyUV ) * texSize;
				float mLen = length( matrixDisp );
				float vLen = length( mvDisp );
				float ratio = ( mLen > 0.25 ) ? ( vLen / mLen ) : 1.0;
				vec3 col;
				if ( !mvValid ) {
					col = vec3( 1.0, 0.0, 1.0 );
				} else if ( abs( ratio - 1.0 ) < 0.08 ) {
					col = vec3( 0.0, 1.0, 0.0 );
				} else if ( abs( ratio - 2.0 ) < 0.25 ) {
					col = vec3( 1.0, 1.0, 0.0 );
				} else if ( abs( ratio - 4.0 ) < 0.5 ) {
					col = vec3( 1.0, 0.0, 0.0 );
				} else if ( abs( ratio - 0.5 ) < 0.1 ) {
					col = vec3( 0.0, 1.0, 1.0 );
				} else if ( abs( ratio - 0.25 ) < 0.08 ) {
					col = vec3( 0.0, 0.4, 1.0 );
				} else {
					col = vec3( clamp( ratio * 0.25, 0.0, 1.0 ), 0.2, 0.2 );
				}
				out_color = vec4( col, 1.0 );
				return;
			}
			if ( debugMode < 33.5 ) {
				/* 33 = temporal frame age of previous matrices (1=green, >1=red, 0=yellow) */
				if ( age < 0.5 ) {
					out_color = vec4( 1.0, 1.0, 0.0, 1.0 );
				} else if ( age < 1.5 ) {
					out_color = vec4( 0.0, 1.0, 0.0, 1.0 );
				} else {
					float t = clamp( ( age - 1.0 ) / 3.0, 0.0, 1.0 );
					out_color = vec4( 1.0, 1.0 - t, 0.0, 1.0 );
				}
				return;
			}
			if ( debugMode < 34.5 ) {
				/* 34 = number of temporal resolves applied last frame (1=green, >1=red) */
				if ( resolves < 0.5 ) {
					out_color = vec4( 0.2, 0.2, 0.2, 1.0 );
				} else if ( resolves < 1.5 ) {
					out_color = vec4( 0.0, 1.0, 0.0, 1.0 );
				} else {
					out_color = vec4( 1.0, 0.0, 0.0, 1.0 );
				}
				return;
			}
			/* 35 = reprojection correspondence: current (cyan) + history lookup (magenta)
			 * linked by a soft trail along the motion vector. */
			{
				vec2 trail = historyUV - sampleUV;
				float along = length( trail ) > 1e-6 ?
					clamp( dot( uv - sampleUV, trail ) / dot( trail, trail ), 0.0, 1.0 ) : 0.0;
				vec2 closest = sampleUV + trail * along;
				float distPx = length( ( uv - closest ) * texSize );
				float onLine = 1.0 - smoothstep( 0.75, 1.75, distPx );
				float atCurr = 1.0 - smoothstep( 1.5, 3.0, length( ( uv - sampleUV ) * texSize ) );
				float atHist = 1.0 - smoothstep( 1.5, 3.0, length( ( uv - historyUV ) * texSize ) );
				vec3 base = current * 0.35;
				base = mix( base, vec3( 0.0, 1.0, 1.0 ), atCurr );
				base = mix( base, vec3( 1.0, 0.0, 1.0 ), atHist );
				base = mix( base, vec3( 1.0, 1.0, 0.2 ), onLine * ( 1.0 - max( atCurr, atHist ) ) );
				out_color = vec4( base, 1.0 );
				return;
			}
		}
	}

	vec2 texSize = vec2( textureSize( currentColor, 0 ) );
	/* Matrix reprojection displacement (camera-only). If stored MVs understate
	 * camera motion (silent zero / scale bug), prefer matrix history before
	 * sampling so dark residual shading cannot linger from the wrong UV. */
	{
		vec2 storedVel = ( sampleUV - historyUV ) * texSize;
		vec2 matrixVel = ( sampleUV - matrixHistoryUV ) * texSize;
		float storedLen = length( storedVel );
		float matrixLen = length( matrixVel );
		if ( mvValid && storedLen + 0.75 < matrixLen * 0.45 && matrixLen > 3.0 ) {
			historyUV = matrixHistoryUV;
			motion = sampleUV - historyUV;
		}
	}

	vec3 history = textureLod( historyColor, historyUV, 0.0 ).rgb;
	float histDepth = textureLod( previousDepthTex, historyUV, 0.0 ).r;

	vec2 velocity = ( sampleUV - historyUV ) * texSize;
	float motionLen = length( velocity );
	float motionFactor = smoothstep( 0.2, 8.0, motionLen );

	/* Object-debug / dynamic tuning from packed PostFX (DoF-off channels). */
	float dynHistMax = ( postfx.depthOfField.x < 0.5 ) ? clamp( postfx.depthOfField.y, 0.0, 0.85 ) : 0.48;
	float dynDepthThresh = ( postfx.depthOfField.x < 0.5 ) ? clamp( postfx.depthOfField.z, 0.001, 0.1 ) : 0.012;
	float dynDilation = ( postfx.depthOfField.x < 0.5 ) ? clamp( postfx.depthOfField.w, 0.0, 4.0 ) : 1.5;
	dynDilation = clamp( dynDilation + motionLen * 0.08, 1.0, 2.5 );
	/* Surf / high camera speed: hard-reject earlier than the old 48 px cap. */
	float velLimit = adaptive ? 28.0 : 36.0;
	float objectDebug = 0.0;
	if ( postfx.temporalDebugParams.x > 100.0 ) {
		objectDebug = postfx.temporalDebugParams.x - 100.0;
	}

	/*
	 * Class-aware history rejection (r_weaponTemporalMode via splitShadow.a):
	 *   0 = no weapon history (force current when prev class is WEAPON)
	 *   1 = classified shared history (reject WEAPON↔WORLD mismatch) [default]
	 *   2 = independent weapon history; world resolve still rejects weapon history here
	 * Prev class is dilated 1–2 px along motion so silhouette edges do not bleed.
	 */
	float weaponTemporalMode = postfx.splitShadow.a;
	float classActive = postfx.splitHighlight.a;
	float prevClass = 0.0;
	float currClassHint = 0.0;
	bool classReject = false;
	if ( classActive > 0.5 ) {
		vec2 classDilate = texel * clamp( motionLen * 0.15, 1.0, 2.0 );
		float c0 = textureLod( temporalClassTex, historyUV, 0.0 ).r;
		float c1 = textureLod( temporalClassTex, historyUV + vec2( classDilate.x, 0.0 ), 0.0 ).r;
		float c2 = textureLod( temporalClassTex, historyUV - vec2( classDilate.x, 0.0 ), 0.0 ).r;
		float c3 = textureLod( temporalClassTex, historyUV + vec2( 0.0, classDilate.y ), 0.0 ).r;
		float c4 = textureLod( temporalClassTex, historyUV - vec2( 0.0, classDilate.y ), 0.0 ).r;
		prevClass = max( c0, max( c1, max( c2, max( c3, c4 ) ) ) );
		/* Current frame is world-only during Architecture B TAA; weapon depth hints residual. */
		currClassHint = smoothstep( 0.58, 0.62, depthNdc );
		bool prevWeapon = prevClass > CLASS_WEAPON_THRESH;
		bool currWeapon = currClassHint > 0.5;
		if ( weaponTemporalMode < 0.5 ) {
			/* Mode 0: never reuse history that landed on last-frame weapon. */
			classReject = prevWeapon;
		} else {
			/* Mode 1/2: reject WEAPON↔WORLD cross-contamination. */
			classReject = prevWeapon != currWeapon && ( prevWeapon || currWeapon );
			/* Newly revealed world behind last-frame gun: force current. */
			if ( prevWeapon && !currWeapon ) {
				classReject = true;
			}
		}
	}

	/*
	 * True per-pixel object-identity rejection.
	 * currentObjectId is stamped this frame at the pixel; previousObjectId is last
	 * frame's buffer sampled at the reprojected history UV. Any mismatch means the
	 * pixel's owner changed between frames — object moved, disoccluded background,
	 * background→object, entity-slot reuse, or overlapping object — so the history
	 * cannot be trusted. World/static pixels read 0 in both buffers (also when the
	 * identity feature is off and the 1x1 stub is bound), so they are never rejected.
	 */
	uint currObjId = texture( currentObjectId, sampleUV ).r & 0xFFFFu;
	uint prevObjId = texture( previousObjectId, historyUV ).r & 0xFFFFu;
	bool objectIdReject = ( currObjId != prevObjId );

	/* Confidence factors */
	float depthConf = 1.0;
	float neighborhoodDepthReject = 0.0;
	if ( useDisocc > 0.5 ) {
		if ( postfx.temporalValidity.y < 0.5 ) {
			depthConf = 0.0;
		} else {
			float predictedPreviousLinear = linearizeReversedDepth( predictedPreviousDepthNdc );
			float sampledPreviousLinear = linearizeReversedDepth( histDepth );
			float relativeDepthError = abs( predictedPreviousLinear - sampledPreviousLinear ) /
				max( max( predictedPreviousLinear, sampledPreviousLinear ), 1e-3 );
			/* Stricter for small/fast movers (helmet silhouettes). */
			float dLo = adaptive ? 0.0015 : dynDepthThresh;
			float dHi = adaptive ? 0.018 : max( dynDepthThresh * 4.0, 0.03 );
			depthConf = 1.0 - smoothstep( dLo, dHi, relativeDepthError );

			/*
			 * Neighborhood depth test at silhouettes: if any nearby current depth
			 * disagrees strongly with history at the reprojected UV, reject.
			 * Dilate along the trailing edge (-velocity) where background is uncovered.
			 */
			vec2 trailDir = length( velocity ) > 1e-3 ? normalize( -velocity ) : vec2( 0.0 );
			float ndMin = 1e30;
			float ndMax = -1e30;
			for ( int oy = -1; oy <= 1; ++oy ) {
				for ( int ox = -1; ox <= 1; ++ox ) {
					vec2 o = vec2( float( ox ), float( oy ) );
					vec2 p = sampleUV + o * texel * dynDilation + trailDir * texel * ( dynDilation - 1.0 );
					float d = textureLod( depthTex, clamp( p, 0.0, 1.0 ), 0.0 ).r;
					float lin = linearizeReversedDepth( d );
					ndMin = min( ndMin, lin );
					ndMax = max( ndMax, lin );
				}
			}
			float histLin = sampledPreviousLinear;
			float layerGap = abs( histLin - clamp( histLin, ndMin, ndMax ) ) /
				max( max( histLin, ndMax ), 1e-3 );
			neighborhoodDepthReject = smoothstep( dLo, dHi, layerGap );
			depthConf *= 1.0 - neighborhoodDepthReject;
		}
	}

	float velocityConf = 1.0 - smoothstep( adaptive ? 2.5 : 4.0, adaptive ? 16.0 : 24.0, motionLen );
	if ( !mvValid && postfx.depthParams.z > 0.5 ) {
		/* Missing/invalid object motion: hard-reject, never blend stale history. */
		velocityConf = 0.0;
		depthConf = 0.0;
	}
	/* Cap velocity contribution for dynamic-scale motion. */
	if ( motionLen > velLimit ) {
		velocityConf = 0.0;
	}

	float currentLuma = max( dot( current, LUMA ), 0.0 );
	float historyLuma = max( dot( history, LUMA ), 0.0 );
	float lumaDiff = abs( currentLuma - historyLuma );
	float lumaConf = 1.0 - smoothstep( adaptive ? 0.018 : 0.028, adaptive ? 0.18 : 0.26, lumaDiff );

	/*
	 * Heuristic reactive always runs with Temporal Reconstruction.
	 * colorGrade2.w / midsGamma.a only gate the stamped OIT/transparent mask texture —
	 * not near-weapon, flash, or silhouette bleed (those caused the echoing glow when
	 * r_temporalReactiveMask was 0 or the mask was not allocated).
	 */
	float nearWeapon = smoothstep( 0.82, 0.995, depthNdc ); /* near in reversed-Z */
	float fastMotion = smoothstep( 2.5, 10.0, motionLen );
	float flash = smoothstep( 0.06, 0.28, lumaDiff );
	/* View-dependent / emissive peaks: current much brighter than history. */
	float highlightGhost = smoothstep( 0.10, 0.60, currentLuma - historyLuma ) *
		smoothstep( 0.15, 1.10, currentLuma );
	/* Dark geo over former bright history (skyline / AZ / HOST banner trails). */
	float historyBleed = smoothstep( 0.04, 0.35, historyLuma - currentLuma ) *
		smoothstep( 0.08, 0.70, historyLuma );
	/* Trailing-edge disocclusion: high motion + depth reject → reactive. */
	float trailDisocc = neighborhoodDepthReject * smoothstep( 1.0, 5.0, motionLen );
	float reactive = clamp( max( nearWeapon,
		max( fastMotion * 0.95,
		max( flash, max( highlightGhost * 0.95,
		max( historyBleed * 1.0, trailDisocc ) ) ) ) ), 0.0, 1.0 );
	if ( !mvValid && postfx.depthParams.z > 0.5 ) {
		reactive = max( reactive, 1.0 );
	}
	if ( useReactive > 0.5 && postfx.midsGamma.a > 0.5 ) {
		float stamped = textureLod( reactiveMaskTex, sampleUV, 0.0 ).r;
		/* Any meaningful transparent/OIT stamp fully prefers current. */
		if ( stamped > 0.02 ) {
			reactive = max( reactive, max( stamped, 0.95 ) );
		}
	}
	float reactiveConf = 1.0 - reactive;

	float confidence = clamp( depthConf * velocityConf * lumaConf * reactiveConf, 0.0, 1.0 );
	/* Hard reject when reactive is high — do not blend a fixed global weight. */
	float reactiveHard = adaptive ? 0.65 : 0.82;
	if ( reactive > reactiveHard ) {
		confidence = 0.0;
	}
	if ( classReject ) {
		confidence = 0.0;
		reactive = max( reactive, 0.98 );
	}
	/* Object-identity mismatch: hard reject history and prefer current color. */
	if ( objectIdReject ) {
		confidence = 0.0;
		reactive = max( reactive, 0.98 );
	}
	if ( adaptive && depthConf < 0.35 ) {
		confidence = 0.0; /* immediate disocclusion → current-frame spatial */
	}
	if ( !adaptive && depthConf < 0.20 ) {
		confidence = 0.0; /* hard reject: real previous-depth mismatch */
	}
	if ( any( isnan( history ) ) || any( isinf( history ) ) || any( lessThan( history, vec3( -1e-3 ) ) ) ) {
		confidence = 0.0;
		history = current;
	}

	/* Difficult-pixel mask for bounded extra current-frame samples. */
	float difficult = clamp( max( 1.0 - confidence,
		max( reactive, max( 1.0 - depthConf, motionFactor ) ) ), 0.0, 1.0 );
	bool wantExtraSamples = adaptive && difficult > ( 1.0 - adaptBudget * 0.85 );

	/* r_temporalObjectDebug 1–12 (encoded as temporalDebugParams.x = 100+mode). */
	if ( objectDebug > 0.5 ) {
		float od = objectDebug;
		if ( od < 1.5 ) {
			/* 1 object velocity */
			vec2 vel = velocity * 0.05;
			bool bad = any( isnan( motion ) ) || any( isinf( motion ) );
			float large = motionLen > 40.0 ? 1.0 : 0.0;
			out_color = bad ? vec4( 1.0, 0.0, 1.0, 1.0 ) :
				( large > 0.5 ? vec4( 1.0, 1.0, 0.0, 1.0 ) : vec4( abs( vel.x ), abs( vel.y ), 0.15, 1.0 ) );
			return;
		}
		if ( od < 2.5 ) {
			/* 2 previous transform validity */
			out_color = mvValid ? vec4( 0.0, 1.0, 0.0, 1.0 ) : vec4( 1.0, 0.0, 0.0, 1.0 );
			return;
		}
		if ( od < 5.5 ) {
			/* 3 = current object id (hashed to color), 4 = previous object id at history UV,
			 * 5 = identity-reject mask (magenta where current/prev ids disagree). */
			if ( od < 3.5 ) {
				float h = float( currObjId ) / 65535.0;
				out_color = ( currObjId == 0u ) ? vec4( 0.05, 0.05, 0.05, 1.0 )
					: vec4( fract( h * 97.0 ), fract( h * 31.0 ), fract( h * 13.0 ), 1.0 );
			} else if ( od < 4.5 ) {
				float h = float( prevObjId ) / 65535.0;
				out_color = ( prevObjId == 0u ) ? vec4( 0.05, 0.05, 0.05, 1.0 )
					: vec4( fract( h * 97.0 ), fract( h * 31.0 ), fract( h * 13.0 ), 1.0 );
			} else {
				out_color = objectIdReject ? vec4( 1.0, 0.0, 1.0, 1.0 ) : vec4( 0.0, 0.35, 0.0, 1.0 );
			}
			return;
		}
		if ( od < 6.5 ) { out_color = vec4( depthNdc, depthNdc, depthNdc, 1.0 ); return; }
		if ( od < 7.5 ) { out_color = vec4( histDepth, histDepth, histDepth, 1.0 ); return; }
		if ( od < 8.5 ) { out_color = vec4( 1.0 - depthConf, neighborhoodDepthReject, 0.1, 1.0 ); return; }
		if ( od < 9.5 ) { out_color = vec4( confidence, confidence, confidence, 1.0 ); return; }
		if ( od < 10.5 ) {
			float d = clamp( lumaDiff * 3.0, 0.0, 1.0 );
			out_color = vec4( d, 0.2, 1.0 - d, 1.0 );
			return;
		}
		if ( od < 11.5 ) { out_color = vec4( reactive, reactive * 0.35, 1.0 - reactive, 1.0 ); return; }
		if ( od < 12.5 ) { out_color = vec4( trailDisocc, neighborhoodDepthReject, 1.0 - depthConf, 1.0 ); return; }
	}

	if ( debugMode > 1.5 ) {
		/* Rejection reason / ownership viz (r_debugHistoryRejection / r_temporalDebugView). */
		float dbg = debugMode;
		if ( dbg > 8.5 && dbg < 9.5 ) {
			/* 9 = adaptive current-frame sample mask */
			float m = wantExtraSamples ? 1.0 : difficult;
			out_color = vec4( m, m * 0.4, 1.0 - m, 1.0 );
			return;
		}
		if ( dbg > 9.5 && dbg < 10.5 ) {
			/* 10 = current (R) vs history (G) contribution proxy */
			float histW = clamp( confidence * 0.5, 0.0, 1.0 );
			out_color = vec4( 1.0 - histW, histW, 0.15, 1.0 );
			return;
		}
		if ( dbg > 10.5 && dbg < 11.5 ) {
			/* 11 = neighborhood luminance variance */
			vec3 meanY, sigmaY;
			neighborhoodYCoCgStats( sampleUV, meanY, sigmaY );
			float v = clamp( sigmaY.x * 4.0, 0.0, 1.0 );
			out_color = vec4( v, v * 0.5, 0.1, 1.0 );
			return;
		}
		if ( dbg > 11.5 && dbg < 12.5 ) {
			/* 12 = unclipped history vs current delta */
			float d = clamp( lumaDiff * 3.0, 0.0, 1.0 );
			out_color = vec4( d, 0.2, 1.0 - d, 1.0 );
			return;
		}
		if ( dbg > 2.5 && dbg < 3.5 ) {
			/* 3 = reactive mask */
			out_color = vec4( reactive, reactive * 0.35, 1.0 - reactive, 1.0 );
			return;
		}
		if ( dbg > 3.5 && dbg < 4.5 ) {
			/* 4 = history weight / confidence */
			out_color = vec4( confidence, confidence, confidence, 1.0 );
			return;
		}
		if ( dbg > 4.5 && dbg < 5.5 ) {
			/* 5 = disocclusion (1 - depthConf) */
			out_color = vec4( 1.0 - depthConf, 0.2, 0.2, 1.0 );
			return;
		}
		if ( dbg > 5.5 && dbg < 6.5 ) {
			/* 6 = reprojected history UV */
			out_color = vec4( historyUV, 0.0, 1.0 );
			return;
		}
		if ( dbg > 6.5 && dbg < 7.5 ) {
			/* 7 = near-weapon heuristic mask */
			float nearWeaponDbg = smoothstep( 0.90, 0.998, depthNdc );
			out_color = vec4( nearWeaponDbg, 0.15, 0.15, 1.0 );
			return;
		}
		if ( dbg > 7.5 && dbg < 8.5 ) {
			/* 8 = world vs reactive ownership */
			out_color = vec4( reactive > 0.5 ? vec3( 1.0, 0.85, 0.1 ) : vec3( 0.15, 0.35, 1.0 ), 1.0 );
			return;
		}
		if ( reactive > 0.55 ) {
			out_color = vec4( 1.0, 1.0, 0.0, 1.0 ); /* yellow reactive */
			return;
		}
		if ( depthConf < 0.45 ) {
			out_color = vec4( 1.0, 0.0, 0.0, 1.0 ); /* red depth */
			return;
		}
		if ( !mvValid && postfx.depthParams.z > 0.5 ) {
			out_color = vec4( 1.0, 0.0, 1.0, 1.0 ); /* magenta MV */
			return;
		}
		if ( lumaConf < 0.45 ) {
			out_color = vec4( 0.0, 1.0, 1.0, 1.0 ); /* cyan luma */
			return;
		}
		if ( velocityConf < 0.45 ) {
			out_color = vec4( 0.2, 0.4, 1.0, 1.0 ); /* blue velocity proxy */
			return;
		}
		out_color = vec4( 0.0, 1.0, 0.0, 1.0 ); /* green accept */
		return;
	}

	float baseStationary = clamp( postfx.taaParams.y, 0.0, 0.95 );
	float baseMotion = clamp( postfx.taaParams.z, 0.0, 0.95 );
	float baseWeight = mix( baseStationary, baseMotion, motionFactor );
	/* Cap via depthParams.w carrying r_temporalHistoryWeight */
	float maxHist = clamp( postfx.depthParams.w, 0.0, 0.95 );
	baseWeight = min( baseWeight, maxHist );
	/* Dynamic objects (significant screen motion): cap below static-world maximum. */
	if ( motionLen > 1.25 ) {
		baseWeight = min( baseWeight, dynHistMax );
	}
	if ( adaptive ) {
		baseWeight = min( baseWeight, 0.42 );
	}

	float feedback = clamp( baseWeight * confidence, 0.0, adaptive ? 0.42 : 0.95 );

	vec3 historyClipped = history;
	if ( useVarClip > 0.5 ) {
		vec3 meanY, sigmaY;
		neighborhoodYCoCgStats( sampleUV, meanY, sigmaY );
		/* Tighten further on highlight / reactive / silhouette pixels so trails cannot stick. */
		float highlightTighten = smoothstep( 0.20, 1.10, currentLuma );
		float edgeTighten = max( neighborhoodDepthReject, reactive );
		float gamma = mix( 1.25, 0.55, max( motionFactor, reactive ) );
		gamma = mix( gamma, gamma * 0.72, highlightTighten );
		gamma = mix( gamma, gamma * 0.65, edgeTighten );
		if ( adaptive ) {
			gamma *= 0.78; /* tighter clip — current neighborhood owns bounds */
		}
		vec3 lo = meanY - gamma * sigmaY;
		vec3 hi = meanY + gamma * sigmaY;
		/* Tighter luminance (Y) than chroma */
		lo.x = meanY.x - gamma * sigmaY.x * 0.85;
		hi.x = meanY.x + gamma * sigmaY.x * 0.85;
		vec3 histY = RGBToYCoCg( historyClipped );
		histY = clamp( histY, lo, hi );
		historyClipped = max( YCoCgToRGB( histY ), vec3( 0.0 ) );
	} else {
		/* Legacy RGB neighborhood clamp fallback */
		vec3 mn = vec3( 1e30 );
		vec3 mx = vec3( -1e30 );
		for ( int y = -1; y <= 1; ++y ) {
			for ( int x = -1; x <= 1; ++x ) {
				vec3 c = sampleCurrent( sampleUV + vec2( x, y ) * texel );
				mn = min( mn, c );
				mx = max( mx, c );
			}
		}
		historyClipped = clamp( historyClipped, mn, mx );
	}

	vec3 currentSample = current;
	if ( wantExtraSamples && adaptSpatial ) {
		currentSample = spatialCurrentFallback( sampleUV );
	} else if ( confidence < 0.08 && adaptSpatial ) {
		currentSample = spatialCurrentFallback( sampleUV );
	}

	vec3 resolved = mix( currentSample, historyClipped, feedback );
	resolved = applyResolveSharpen( sampleUV, resolved );
	out_color = vec4( resolved, 1.0 );
}
