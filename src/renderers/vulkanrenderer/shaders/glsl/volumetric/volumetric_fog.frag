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
} params;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float saturate(float v) {
    return clamp(v, 0.0, 1.0);
}

bool isFinite1(float v) {
    return !(isnan(v) || isinf(v));
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
    float expNorm = pow(saturate(sliceNorm), getSliceExponent());
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
    float depthSample = texture(depthTexture, v_UV).r;
    int depthMode = int(clamp(floor(params.miscParams.y + 0.5), 0.0, 2.0));
    int fogDebug = int(clamp(floor(params.gridDim.w + 0.5), 0.0, 10.0));
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

    vec3 scene = texture(sceneColor, v_UV).rgb;
    float jitter = (hash12(gl_FragCoord.xy + frameIndex) - 0.5) * jitterStrength;

    if (fogDebug == 1) {
        float sliceNorm = clamp(log(max(sceneDepth / nearPlane, 1e-4)) / max(log(farPlane / nearPlane), 1e-5), 0.0, 1.0);
        fragColor = vec4(vec3(v_UV, sliceNorm), 1.0);
        return;
    }
    if (fogDebug == 2) {
        float sigma = texture(froxelExtinction, vec3(v_UV, 0.5)).r;
        fragColor = vec4(sigma, sigma, sigma, 1.0);
        return;
    }
    if (fogDebug == 3) {
        vec3 scatter = texture(froxelScattering, vec3(v_UV, 0.5)).rgb;
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
            float sigmaT = texture(froxelExtinction, uvw).r;
            if (!isFinite1(sigmaT)) {
                sigmaT = 0.0;
            }
            sigmaT = clamp(sigmaT, 0.0, 6.0);
            vec3 scatterSource = max(texture(froxelScattering, uvw).rgb, vec3(0.0));

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
        vec3 scatter = texture(froxelScattering, vec3(v_UV, 0.5)).rgb;
        fragColor = vec4(scatter, 1.0);
        return;
    }
    if (fogDebug == 7) {
        vec2 motion = texture(motionTexture, v_UV).rg;
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

    vec3 outRgb = scene * transmittance + fogRadiance;
    fragColor = vec4(outRgb, 1.0);
}
