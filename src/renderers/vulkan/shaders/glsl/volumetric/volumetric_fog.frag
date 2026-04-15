#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D sceneColor;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler3D froxelScattering;
layout(binding = 3) uniform sampler3D froxelExtinction;
layout(binding = 5) uniform sampler2D motionTexture;
layout(binding = 6) uniform sampler2D localSpotShadowMap;
layout(binding = 7) uniform sampler2DArray localPointShadowMap;
layout(binding = 8) uniform usampler2D telemetryTexture;

const int MAX_VOLUMES = 24;
const int MAX_LIGHTS = 32;

layout(std140, binding = 4) uniform VolumetricParams {
    mat4 invProj;
    mat4 invView;
    mat4 proj;
    mat4 viewProj;
    mat4 prevView;
    mat4 prevViewProj;
    vec4 viewOrigin;
    vec4 sunDirection;
    vec4 fogColor;
    vec4 densityParams;
    vec4 worldMin;
    vec4 worldMax;
    vec4 gridDim;
    vec4 miscParams;
    vec4 sliceParams;
    vec4 phaseParams;
    vec4 scatterParams;
    vec4 noiseParams;
    vec4 noiseScroll;
    vec4 temporalParams;
    vec4 qualityParams;
    vec4 windParams;
    vec4 volumeCounts;
    vec4 passParams;
    vec4 volumeBoundsMin[MAX_VOLUMES];
    vec4 volumeBoundsMax[MAX_VOLUMES];
    vec4 volumeColorDensity[MAX_VOLUMES];
    vec4 volumeTypeParams[MAX_VOLUMES];
    vec4 lightPosRadius[MAX_LIGHTS];
    vec4 lightColorType[MAX_LIGHTS];
    vec4 lightDirAngle[MAX_LIGHTS];
    vec4 lightExtra[MAX_LIGHTS];
    mat4 sunShadowMatrix0;
    vec4 shadowParams0;
    vec4 shadowMapSize0;
    mat4 localSpotShadowMatrix[MAX_LIGHTS];
    mat4 localPointShadowMatrix[MAX_LIGHTS][6];
    vec4 localShadowAtlasUv[MAX_LIGHTS];
    vec4 localSpotShadowMapSize;
    vec4 localPointShadowMapSize;
    vec4 fluidParams0;
    vec4 fluidParams1;
    vec4 fluidParams2;
    vec4 fluidWorldMap;
    vec4 fluidEmitters[16];
    vec4 fluidEmitterData[16];
    vec4 fluidEmitterCount;
    vec4 telemetryParams0;
    vec4 telemetryParams1;
} params;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float halton2(int index) {
    float result = 0.0;
    float f = 0.5;
    int i = index;
    while (i > 0) {
        result += f * float(i & 1);
        i /= 2;
        f *= 0.5;
    }
    return result;
}

float saturate(float v) {
    return clamp(v, 0.0, 1.0);
}

bool isFinite1(float v) {
    return !(isnan(v) || isinf(v));
}

float telemetryNormalize(uint counterValue) {
    float c = float(counterValue);
    return clamp(log2(c + 1.0) / 12.0, 0.0, 1.0);
}

float decodeClipZ(float depthSample, int depthMode) {
    if (depthMode == 0) {
        return depthSample * 2.0 - 1.0;
    }
    if (depthMode == 1) {
        return (1.0 - depthSample) * 2.0 - 1.0;
    }
    return 0.0;
}

float getNearPlane() {
    return max(params.sliceParams.x, 0.001);
}

float getFarPlane(float nearPlane) {
    return max(params.sliceParams.y, nearPlane + 1.0);
}

float getSliceExponent() {
    return max(params.sliceParams.z, 1.0);
}

float depthFromSliceNorm(float sliceNorm, float nearPlane, float farPlane) {
    float s = saturate(sliceNorm);
    float mode = floor(params.qualityParams.y + 0.5);
    if (mode <= 0.5) {
        float expNorm = pow(s, getSliceExponent());
        return nearPlane * pow(farPlane / nearPlane, expNorm);
    }
    if (mode <= 1.5) {
        return mix(nearPlane, farPlane, s);
    }
    float invExp = 1.0 / max(getSliceExponent(), 1.0);
    float expNorm = pow(s, invExp);
    return nearPlane * pow(farPlane / nearPlane, expNorm);
}

vec3 reconstructViewRay(vec2 uv) {
    vec4 clipFar = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 viewFar = params.invProj * clipFar;
    return viewFar.xyz / max(abs(viewFar.w), 1e-6);
}

void reconstructViewRayAndDepth(
    vec2 uv,
    float depthSample,
    int depthMode,
    float nearPlane,
    float farPlane,
    out vec3 viewDir,
    out float sceneDepth)
{
    vec3 viewRay = reconstructViewRay(uv);
    viewDir = normalize(viewRay);

    if (depthMode == 2) {
        float linearDepth = depthSample;
        if (linearDepth <= 1.0) {
            linearDepth = mix(nearPlane, farPlane, saturate(linearDepth));
        }
        sceneDepth = clamp(linearDepth, nearPlane, farPlane);
        return;
    }

    vec4 clip = vec4(uv * 2.0 - 1.0, decodeClipZ(depthSample, depthMode), 1.0);
    vec4 view = params.invProj * clip;
    vec3 viewPos = view.xyz / max(abs(view.w), 1e-6);
    viewDir = normalize(viewPos);
    sceneDepth = clamp(max(-viewPos.z, nearPlane), nearPlane, farPlane);
}

vec3 reconstructWorldPos(vec3 viewDir, float sceneDepth) {
    float viewDistance = sceneDepth / max(-viewDir.z, 1e-4);
    vec3 viewPos = viewDir * viewDistance;
    return (params.invView * vec4(viewPos, 1.0)).xyz;
}

int selectPointShadowFace(vec3 dirFromLight) {
    vec3 a = abs(dirFromLight);
    if (a.x >= a.y && a.x >= a.z) {
        return (dirFromLight.x >= 0.0) ? 0 : 1;
    }
    if (a.y >= a.x && a.y >= a.z) {
        return (dirFromLight.y >= 0.0) ? 2 : 3;
    }
    return (dirFromLight.z >= 0.0) ? 4 : 5;
}

float sampleSpotShadowDebug(vec3 worldPos, int lightIndex) {
    if (lightIndex < 0 || lightIndex >= MAX_LIGHTS) {
        return 1.0;
    }
    if (params.localSpotShadowMapSize.z <= 0.0 || params.localSpotShadowMapSize.w <= 0.0) {
        return 1.0;
    }

    vec4 lightClip = params.localSpotShadowMatrix[lightIndex] * vec4(worldPos, 1.0);
    if (abs(lightClip.w) <= 1e-6) {
        return 1.0;
    }

    vec4 atlasRect = params.localShadowAtlasUv[lightIndex];
    vec3 ndc = lightClip.xyz / lightClip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    vec2 atlasUV = uv * atlasRect.xy + atlasRect.zw;
    float depth = ndc.z * 0.5 + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || depth <= 0.0 || depth >= 1.0) {
        return 1.0;
    }
    if (any(lessThan(atlasUV, atlasRect.zw)) || any(greaterThan(atlasUV, atlasRect.zw + atlasRect.xy))) {
        return 1.0;
    }

    float compareDepth = depth - max(params.shadowParams0.x, 0.0);
    vec2 texelStep = params.localSpotShadowMapSize.zw * max(params.shadowParams0.y, 0.0);
    if (texelStep.x <= 0.0 || texelStep.y <= 0.0) {
        float sampleDepth = texture(localSpotShadowMap, atlasUV).r;
        return (compareDepth <= sampleDepth) ? 1.0 : 0.0;
    }

    float lit = 0.0;
    vec2 atlasMin = atlasRect.zw + texelStep * 0.5;
    vec2 atlasMax = atlasRect.zw + atlasRect.xy - texelStep * 0.5;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 pcfUV = clamp(atlasUV + vec2(x, y) * texelStep, atlasMin, atlasMax);
            float sampleDepth = texture(localSpotShadowMap, pcfUV).r;
            lit += (compareDepth <= sampleDepth) ? 1.0 : 0.0;
        }
    }
    return lit * (1.0 / 9.0);
}

float samplePointShadowDebug(vec3 worldPos, int lightIndex) {
    if (lightIndex < 0 || lightIndex >= MAX_LIGHTS) {
        return 1.0;
    }
    int pointSlot = int(floor(params.lightExtra[lightIndex].w + 0.5));
    if (pointSlot < 0 || params.localPointShadowMapSize.z <= 0.0) {
        return 1.0;
    }

    vec3 fromLight = worldPos - params.lightPosRadius[lightIndex].xyz;
    int face = selectPointShadowFace(fromLight);
    int layer = pointSlot * 6 + face;

    vec4 lightClip = params.localPointShadowMatrix[lightIndex][face] * vec4(worldPos, 1.0);
    if (abs(lightClip.w) <= 1e-6) {
        return 1.0;
    }

    vec3 ndc = lightClip.xyz / lightClip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float depth = ndc.z * 0.5 + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || depth <= 0.0 || depth >= 1.0) {
        return 1.0;
    }

    float compareDepth = depth - max(params.shadowParams0.x, 0.0);
    vec2 texelStep = vec2(params.localPointShadowMapSize.z) * max(params.shadowParams0.y, 0.0);
    if (texelStep.x <= 0.0) {
        float sampleDepth = texture(localPointShadowMap, vec3(uv, float(layer))).r;
        return (compareDepth <= sampleDepth) ? 1.0 : 0.0;
    }

    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 pcfUV = uv + vec2(x, y) * texelStep;
            float sampleDepth = texture(localPointShadowMap, vec3(pcfUV, float(layer))).r;
            lit += (compareDepth <= sampleDepth) ? 1.0 : 0.0;
        }
    }
    return lit * (1.0 / 9.0);
}

int findFirstShadowedLightIndex(bool wantSpot) {
    int lightCount = int(clamp(floor(params.volumeCounts.y + 0.5), 0.0, float(MAX_LIGHTS)));
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (i >= lightCount) {
            break;
        }
        bool isSpot = params.lightColorType[i].w > 0.5;
        bool hasShadow = params.lightExtra[i].z > 0.5 && params.lightExtra[i].w >= 0.0;
        if (isSpot == wantSpot && hasShadow) {
            return i;
        }
    }
    return -1;
}

void main() {
    float depthSample = textureLod(depthTexture, v_UV, 0.0).r;
    int depthMode = int(clamp(floor(params.miscParams.y + 0.5), 0.0, 2.0));
    int fogDebug = int(clamp(floor(params.gridDim.w + 0.5), 0.0, 13.0));
    float frameIndex = params.miscParams.z;
    float nearPlane = getNearPlane();
    float farPlane = getFarPlane(nearPlane);
    int zSlices = int(max(floor(params.gridDim.z + 0.5), 1.0));
    int stepBudget = int(max(floor(params.miscParams.x + 0.5), 1.0));
    int marchCount = min(zSlices, stepBudget);
    float jitterStrength = max(params.densityParams.z, 0.0);

    vec3 viewDir;
    float sceneDepth;
    reconstructViewRayAndDepth(v_UV, depthSample, depthMode, nearPlane, farPlane, viewDir, sceneDepth);

    float maxDistance = max(params.sliceParams.w, nearPlane + 1.0);
    sceneDepth = min(sceneDepth, maxDistance);

    vec3 scene = textureLod(sceneColor, v_UV, 0.0).rgb;
    int compositeMode = int(clamp(floor(params.volumeCounts.w + 0.5), 0.0, 2.0));
    int px = int(gl_FragCoord.x), py = int(gl_FragCoord.y);
    int haltonIdx = px + py * 4096 + int(frameIndex) * 256;
    float jitter = (halton2(haltonIdx % 256) - 0.5) * jitterStrength;

    if (fogDebug == 1) {
        float sliceNorm = clamp(log(max(sceneDepth / nearPlane, 1e-4)) / max(log(farPlane / nearPlane), 1e-5), 0.0, 1.0);
        fragColor = vec4(vec3(v_UV, sliceNorm), 1.0);
        return;
    }
    if (fogDebug == 2) {
        float sigma = textureLod(froxelExtinction, vec3(v_UV, 0.5), 0.0).r;
        fragColor = vec4(sigma, sigma, sigma, 1.0);
        return;
    }
    if (fogDebug == 3) {
        vec3 scatter = textureLod(froxelScattering, vec3(v_UV, 0.5), 0.0).rgb;
        fragColor = vec4(scatter, 1.0);
        return;
    }
    if (fogDebug == 4) {
        float temporalWeight = clamp(params.densityParams.w, 0.0, 1.0);
        float hasHistory = params.miscParams.w > 0.5 ? 1.0 : 0.0;
        fragColor = vec4(hasHistory, temporalWeight, params.temporalParams.w, 1.0);
        return;
    }

    float transmittance = 1.0;
    vec3 fogRadiance = vec3(0.0);
    float transCutoff = max(params.qualityParams.w, 0.0001);
    int step = 0;

    while (step < marchCount) {
        int advance = 1;
        float s0 = float(step) / float(marchCount);
        if (params.qualityParams.x <= 1.0 && s0 > params.qualityParams.z) {
            advance = 2;
        }
        int step1 = min(step + advance, marchCount);

        float s1 = float(step1) / float(marchCount);
        float z0 = depthFromSliceNorm(s0, nearPlane, farPlane);
        float z1 = depthFromSliceNorm(s1, nearPlane, farPlane);
        float t0 = max(z0, nearPlane);
        float t1 = min(z1, sceneDepth);
        if (t1 > t0) {
            float sCenter = (float(step) + 0.5 * float(advance) + jitter) / float(marchCount);
            vec3 uvw = vec3(v_UV, clamp(sCenter, 0.0, 1.0));
            float sigmaT = textureLod(froxelExtinction, uvw, 0.0).r;
            if (!isFinite1(sigmaT)) {
                sigmaT = 0.0;
            }
            sigmaT = clamp(sigmaT, 0.0, 6.0);
            vec3 scatterSource = max(textureLod(froxelScattering, uvw, 0.0).rgb, vec3(0.0));

            float dt = t1 - t0;
            float prevT = transmittance;
            transmittance *= exp(-sigmaT * dt);
            fogRadiance += prevT * scatterSource * dt;

            if (transmittance <= transCutoff) {
                break;
            }
        }

        step = step1;
    }

    if (fogDebug == 5) {
        fragColor = vec4(vec3(clamp(transmittance, 0.0, 1.0)), 1.0);
        return;
    }
    if (fogDebug == 6) {
        vec3 scatter = textureLod(froxelScattering, vec3(v_UV, 0.5), 0.0).rgb;
        fragColor = vec4(scatter, 1.0);
        return;
    }
    if (fogDebug == 7) {
        vec2 motion = textureLod(motionTexture, v_UV, 0.0).rg;
        float threshold = max(params.temporalParams.x, 1e-5);
        float motionMag = length(motion);
        float motionNorm = clamp(motionMag / threshold, 0.0, 1.0);
        float rejected = motionMag > threshold ? 1.0 : 0.0;
        fragColor = vec4(motionNorm, clamp(motionMag * 8.0, 0.0, 1.0), 1.0 - rejected, 1.0);
        return;
    }
    if (fogDebug == 8 || fogDebug == 9) {
        vec3 worldPos = reconstructWorldPos(viewDir, sceneDepth);
        int lightIndex = findFirstShadowedLightIndex(fogDebug == 8);
        if (lightIndex < 0) {
            fragColor = (fogDebug == 8) ? vec4(1.0, 0.0, 1.0, 1.0) : vec4(0.0, 1.0, 1.0, 1.0);
            return;
        }

        float shadowVis = (fogDebug == 8) ? sampleSpotShadowDebug(worldPos, lightIndex) : samplePointShadowDebug(worldPos, lightIndex);
        fragColor = vec4(vec3(clamp(shadowVis, 0.0, 1.0)), 1.0);
        return;
    }
    if (fogDebug == 10) {
        float hasHistory = params.miscParams.w > 0.5 ? 1.0 : 0.0;
        float cameraCut = params.temporalParams.w > 0.5 ? 1.0 : 0.0;
        float temporalWeight = clamp(params.densityParams.w, 0.0, 1.0);
        fragColor = vec4(hasHistory, cameraCut, temporalWeight, 1.0);
        return;
    }
    if (fogDebug == 11) {
        const vec3 palette[6] = vec3[6](
            vec3(1.0, 0.25, 0.25),
            vec3(1.0, 0.7, 0.15),
            vec3(0.2, 0.75, 1.0),
            vec3(0.3, 1.0, 0.45),
            vec3(0.95, 0.45, 1.0),
            vec3(1.0, 1.0, 0.35)
        );
        float lane = clamp(v_UV.x, 0.0, 0.9999) * 6.0;
        int idx = int(floor(lane));
        float counterNorm = telemetryNormalize(texelFetch(telemetryTexture, ivec2(idx, 0), 0).r);
        float filled = (v_UV.y <= counterNorm) ? 1.0 : 0.0;
        vec3 col = mix(vec3(0.02), palette[idx], filled);
        fragColor = vec4(col, 1.0);
        return;
    }
    if (fogDebug == 12) {
        float targetMs = max(params.telemetryParams0.w, 0.1);
        float totalMs = max(params.telemetryParams1.x, 0.0);
        float fluidMs = max(params.telemetryParams1.y, 0.0);
        float dynScale = clamp(params.telemetryParams1.z, 0.0, 1.0);
        float dynIters = clamp(params.telemetryParams1.w / 32.0, 0.0, 1.0);
        float totalNorm = clamp(totalMs / targetMs, 0.0, 2.0) * 0.5;
        float fluidNorm = clamp(fluidMs / targetMs, 0.0, 2.0) * 0.5;
        fragColor = vec4(totalNorm, fluidNorm, dynScale, dynIters);
        return;
    }

    vec3 outRgb;
    if (compositeMode == 1) {
        /* Depth-aware in-scatter weight: attenuate fogRadiance when transmittance is high (near camera / clear air). */
        float inScatterWeight = clamp(transmittance, 0.0, 1.0);
        outRgb = scene * transmittance + fogRadiance * inScatterWeight;
    } else {
        outRgb = scene * transmittance + fogRadiance;
    }
    if (compositeMode == 2) {
        float cap = max(params.temporalParams.z, 0.0);
        if (cap > 0.0) {
            outRgb = min(outRgb, vec3(cap));
        }
    }
    /* Guard against NaN/Inf from upstream (compute, bad params, etc.) */
    if ( isnan( outRgb.r ) || isnan( outRgb.g ) || isnan( outRgb.b ) ||
         isinf( outRgb.r ) || isinf( outRgb.g ) || isinf( outRgb.b ) ) {
        outRgb = scene;
    }
    if (fogDebug == 13) {
        vec3 fogDelta = outRgb - scene;
        float fogAmount = clamp(dot(abs(fogDelta), vec3(0.3333)) * 6.0, 0.0, 1.0);
        float extinction = clamp(1.0 - transmittance, 0.0, 1.0);
        vec3 heat = mix(vec3(0.0, 0.10, 0.65), vec3(1.0, 0.28, 0.0), fogAmount);
        fragColor = vec4(heat * fogAmount + vec3(0.0, extinction * 0.35, 0.0), 1.0);
        return;
    }
    fragColor = vec4(outRgb, 1.0);
}
