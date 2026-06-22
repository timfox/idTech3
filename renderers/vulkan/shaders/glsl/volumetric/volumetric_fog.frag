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
layout(binding = 9) uniform sampler2D sunShadowMap;
layout(binding = 10) uniform sampler3D vdbFogDensity;
layout(binding = 11) uniform sampler3D vdbFogMajorant;

const float PI = 3.14159265359;
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
    vec4 vdbParams;
    vec4 vdbWorldMin;
    vec4 vdbWorldMax;
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

float sliceNormFromDepth(float depth, float nearPlane, float farPlane) {
    float mode = floor(params.qualityParams.y + 0.5);
    if (mode <= 0.5) {
        float r = log(max(depth / nearPlane, 1e-4)) / max(log(farPlane / nearPlane), 1e-5);
        return pow(saturate(r), 1.0 / getSliceExponent());
    }
    if (mode <= 1.5) {
        return saturate((depth - nearPlane) / max(farPlane - nearPlane, 1e-5));
    }
    float r = log(max(depth / nearPlane, 1e-4)) / max(log(farPlane / nearPlane), 1e-5);
    return pow(saturate(r), getSliceExponent());
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

float phaseHG(float cosTheta, float g) {
    float gg = g * g;
    float denom = max(1.0 + gg - 2.0 * g * cosTheta, 1e-5);
    return (1.0 - gg) / (4.0 * PI * pow(denom, 1.5));
}

float evaluateHeightDensity(vec3 worldPos) {
    float heightFalloff = max(params.densityParams.y, 0.0);
    float heightDelta = worldPos.z - params.viewOrigin.w;
    return exp(-heightFalloff * max(0.0, heightDelta));
}

float proceduralFogNoise(vec3 worldPos) {
    float noiseScale = max(params.noiseParams.x, 0.0);
    float noiseThreshold = saturate(params.noiseParams.y);
    float noiseStrength = saturate(params.noiseParams.z);

    if (noiseScale <= 0.0 || noiseStrength <= 0.0) {
        return 1.0;
    }

    vec3 wind = params.noiseScroll.xyz * max(params.windParams.z, 0.0);
    vec3 noiseUV = worldPos * noiseScale + wind * params.windParams.x;
    float n0 = hash12(noiseUV.xy + noiseUV.z * 17.31);
    float n1 = hash12(noiseUV.yz + noiseUV.x * 13.71);
    float noiseValue = mix(n0, n1, 0.5);
    float noiseMask = clamp((noiseValue - noiseThreshold) / max(1.0 - noiseThreshold, 1e-4), 0.0, 1.0);
    return mix(1.0, noiseMask, noiseStrength);
}

float evaluateSigmaTAt(vec3 worldPos) {
    float base = max(params.densityParams.x, 0.0) * max(params.scatterParams.y, 0.05);
    return base * evaluateHeightDensity(worldPos) * proceduralFogNoise(worldPos);
}

bool vdbWorldUvw(vec3 worldPos, out vec3 uvw) {
    if (params.vdbParams.y < 0.5) {
        return false;
    }
    vec3 extent = max(params.vdbWorldMax.xyz - params.vdbWorldMin.xyz, vec3(1e-4));
    uvw = (worldPos - params.vdbWorldMin.xyz) / extent;
    return all(greaterThanEqual(uvw, vec3(0.0))) && all(lessThanEqual(uvw, vec3(1.0)));
}

float sampleVdbSigmaT(vec3 worldPos) {
    vec3 uvw;
    if (!vdbWorldUvw(worldPos, uvw)) {
        return 0.0;
    }
    float blend = params.vdbParams.x;
    float scale = max(params.scatterParams.y, 0.05);
    return texture(vdbFogDensity, uvw).r * blend * scale;
}

float sampleVdbMajorant(vec3 worldPos) {
    vec3 uvw;
    if (!vdbWorldUvw(worldPos, uvw)) {
        return 0.0;
    }
    float blend = params.vdbParams.x;
    float scale = max(params.scatterParams.y, 0.05);
    if (params.vdbParams.z > 0.5) {
        return max(texture(vdbFogMajorant, uvw).r * blend * scale, 1e-5);
    }
    return max(sampleVdbSigmaT(worldPos), 1e-5);
}

float sampleSunShadowVisibility(vec3 worldPos);

/* Woodcock / delta tracking with OpenVDB majorant macrocells (arXiv:2211.09997 Algorithm 1). */
vec3 integrateVdbWoodcockFog(vec3 scene, vec3 viewDir, float rayLength, vec3 camPos, float blueNoise) {
    vec3 V = normalize(viewDir);
    vec3 sunDir = normalize(params.sunDirection.xyz);
    vec3 I = params.fogColor.rgb * max(params.phaseParams.y, 0.0);
    float g = clamp(params.phaseParams.x, -0.999, 0.999);
    float sigmaS = clamp(params.scatterParams.x, 0.0, 1.0);
    vec3 fogAmbient = params.fogColor.rgb * max(params.phaseParams.z, 0.0);
    int maxSteps = int(clamp(floor(params.miscParams.x + 0.5), 8.0, 96.0));
    float t = blueNoise * (rayLength / float(maxSteps));
    float T = 1.0;
    vec3 inScatter = vec3(0.0);
    float transCutoff = max(params.qualityParams.w, 0.0001);
    int iter = 0;

    while (t < rayLength && T > transCutoff && iter < maxSteps) {
        vec3 samplePos = camPos + V * t;
        float muBar = sampleVdbMajorant(samplePos);

        if (muBar < 1e-5) {
            t += rayLength / float(maxSteps);
            iter++;
            continue;
        }

        float xi = fract(hash12(samplePos.xy + samplePos.z * 17.0) + float(iter) * 0.618 + blueNoise);
        float dt = -log(max(1e-4, 1.0 - xi)) / muBar;
        float tHit = t + dt;
        if (tHit >= rayLength) {
            T *= exp(-muBar * (rayLength - t));
            break;
        }

        vec3 hitPos = camPos + V * tHit;
        float mu = sampleVdbSigmaT(hitPos) + evaluateSigmaTAt(hitPos) * 0.15;
        float reject = fract(hash12(hitPos.yz + float(iter) * 0.37));
        if (reject * muBar < mu) {
            float heightAtt = evaluateHeightDensity(hitPos);
            float noiseAtt = proceduralFogNoise(hitPos);
            if (params.volumeCounts.z > 0.5) {
                float sunVis = sampleSunShadowVisibility(hitPos);
                float cosTheta = dot(normalize(hitPos - camPos), sunDir);
                float p = phaseHG(cosTheta, g);
                inScatter += T * p * mu * sigmaS * sunVis * I * dt;
            } else {
                inScatter += T * mu * sigmaS * fogAmbient * dt * heightAtt * noiseAtt;
            }
        }

        T *= exp(-muBar * dt);
        t = tHit;
        iter++;
    }

    return scene * T + inScatter + (1.0 - T) * fogAmbient;
}

float sampleSunShadowVisibility(vec3 worldPos) {
    if (params.volumeCounts.z <= 0.5) {
        return 1.0;
    }
    if (params.shadowMapSize0.z <= 0.0 || params.shadowMapSize0.w <= 0.0) {
        return 1.0;
    }

    vec4 lightClip = params.sunShadowMatrix0 * vec4(worldPos, 1.0);
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
    vec2 texelStep = params.shadowMapSize0.zw * max(params.shadowParams0.y, 0.0);

    if (texelStep.x <= 0.0 || texelStep.y <= 0.0) {
        float sampleDepth = texture(sunShadowMap, uv).r;
        return (compareDepth <= sampleDepth) ? 1.0 : 0.0;
    }

    float lit = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 pcfUV = uv + vec2(x, y) * texelStep;
            float sampleDepth = texture(sunShadowMap, pcfUV).r;
            lit += (compareDepth <= sampleDepth) ? 1.0 : 0.0;
        }
    }
    return lit * (1.0 / 9.0);
}

/* Hoffman/Preetham-style single-sample approximate (reference: ApproximateFog). */
vec3 integrateApproximateFog(vec3 scene, vec3 viewDir, float rayLength, vec3 worldPos) {
    float g = clamp(params.phaseParams.x, -0.999, 0.999);
    vec3 sunDir = normalize(params.sunDirection.xyz);
    vec3 V = normalize(viewDir);
    float cosTheta = dot(V, sunDir);
    float p = phaseHG(cosTheta, g);

    float heightAtt = evaluateHeightDensity(worldPos);
    float sigmaT = evaluateSigmaTAt(worldPos);
    float sigmaS = sigmaT * clamp(params.scatterParams.x, 0.0, 1.0);
    float betaS = max(sigmaS, 1e-5);

    vec3 E_sun = params.fogColor.rgb * max(params.phaseParams.y, 0.0);
    float sunVis = sampleSunShadowVisibility(worldPos);

    vec3 inScatter = (E_sun * p) / betaS * (1.0 - exp(-betaS * rayLength)) * heightAtt * sunVis;
    float T = exp(-betaS * heightAtt * rayLength);
    vec3 fogAmbient = params.fogColor.rgb * max(params.phaseParams.z, 0.0);

    return scene * T + inScatter + (1.0 - T) * fogAmbient;
}

/* Screen-space ray march with height density + sun shadow (reference: RayMarchFog; no TLAS). */
vec3 integrateRayMarchFog(vec3 scene, vec3 viewDir, float rayLength, vec3 camPos, float blueNoiseJitter) {
    int numSteps = int(clamp(floor(params.miscParams.x + 0.5), 1.0, 256.0));
    float stepSize = rayLength / float(numSteps);

    vec3 sunDir = normalize(params.sunDirection.xyz);
    vec3 I = params.fogColor.rgb * max(params.phaseParams.y, 0.0);
    float g = clamp(params.phaseParams.x, -0.999, 0.999);
    float densityScale = max(params.densityParams.x, 0.0) * max(params.scatterParams.y, 0.05);
    float sigmaS = densityScale * clamp(params.scatterParams.x, 0.0, 1.0);
    vec3 fogAmbient = params.fogColor.rgb * max(params.phaseParams.z, 0.0);

    vec3 inScatter = vec3(0.0);
    float T = 1.0;

    for (int i = 0; i < numSteps; ++i) {
        float rayT = stepSize * (float(i) + blueNoiseJitter);
        if (rayT > rayLength) {
            break;
        }
        vec3 samplePos = camPos + normalize(viewDir) * rayT;
        float heightAtt = evaluateHeightDensity(samplePos);
        float noiseAtt = proceduralFogNoise(samplePos);
        float sigmaT = densityScale * heightAtt * noiseAtt;
        float perStepAttenuation = exp(-stepSize * sigmaT);

        if (params.volumeCounts.z > 0.5) {
            float sunVis = sampleSunShadowVisibility(samplePos);
            vec3 V = normalize(samplePos - camPos);
            float cosTheta = dot(V, sunDir);
            float p = phaseHG(cosTheta, g);
            inScatter += T * p * sigmaS * sunVis * I * stepSize;
        } else {
            inScatter += T * sigmaS * fogAmbient * stepSize * heightAtt * noiseAtt;
        }

        T *= perStepAttenuation;
        if (T <= max(params.qualityParams.w, 0.0001)) {
            break;
        }
    }

    return scene * T + inScatter + (1.0 - T) * fogAmbient;
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

    int integrationMode = int(clamp(floor(params.passParams.x + 0.5), 0.0, 3.0));
    if (integrationMode > 0) {
        vec3 worldPos = reconstructWorldPos(viewDir, sceneDepth);
        vec3 camPos = params.viewOrigin.xyz;
        float rayLength = length(worldPos - camPos);
        rayLength = min(rayLength, maxDistance - nearPlane);
        rayLength = max(rayLength, nearPlane);

        float blueNoise = halton2(haltonIdx % 256);
        vec3 outRgb;
        if (integrationMode == 1) {
            outRgb = integrateApproximateFog(scene, viewDir, rayLength, worldPos);
        } else if (integrationMode == 2) {
            outRgb = integrateRayMarchFog(scene, viewDir, rayLength, camPos, blueNoise);
        } else if (params.vdbParams.y > 0.5) {
            outRgb = integrateVdbWoodcockFog(scene, viewDir, rayLength, camPos, blueNoise);
        } else {
            outRgb = integrateRayMarchFog(scene, viewDir, rayLength, camPos, blueNoise);
        }

        if (compositeMode == 2) {
            float cap = max(params.temporalParams.z, 0.0);
            if (cap > 0.0) {
                outRgb = min(outRgb, vec3(cap));
            }
        }
        if (fogDebug == 13) {
            vec3 fogDelta = outRgb - scene;
            float fogAmount = clamp(dot(abs(fogDelta), vec3(0.3333)) * 6.0, 0.0, 1.0);
            fragColor = vec4(vec3(fogAmount), 1.0);
            return;
        }
        fragColor = vec4(outRgb, 1.0);
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
            float sliceA = sliceNormFromDepth(t0, nearPlane, farPlane);
            float sliceB = sliceNormFromDepth(t1, nearPlane, farPlane);
            float zBlend = (t1 > t0 + 1e-5) ? saturate((0.5 * (t0 + t1) - t0) / (t1 - t0)) : 0.5;
            sliceA = clamp(sliceA + jitter / float(marchCount), 0.0, 1.0);
            sliceB = clamp(sliceB + jitter / float(marchCount), 0.0, 1.0);

            vec3 uvwA = vec3(v_UV, sliceA);
            vec3 uvwB = vec3(v_UV, sliceB);
            float sigmaA = textureLod(froxelExtinction, uvwA, 0.0).r;
            float sigmaB = textureLod(froxelExtinction, uvwB, 0.0).r;
            vec3 scatterA = max(textureLod(froxelScattering, uvwA, 0.0).rgb, vec3(0.0));
            vec3 scatterB = max(textureLod(froxelScattering, uvwB, 0.0).rgb, vec3(0.0));
            float sigmaT = mix(sigmaA, sigmaB, zBlend);
            vec3 scatterSource = mix(scatterA, scatterB, zBlend);

            if (!isFinite1(sigmaT)) {
                sigmaT = 0.0;
            }
            sigmaT = clamp(sigmaT, 0.0, 6.0);

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
        /* Artistic near-camera halo reduction (not strictly physical). Mode 0 is the accurate path. */
        float inScatterWeight = clamp(1.0 - transmittance, 0.0, 1.0);
        outRgb = scene * transmittance + fogRadiance * inScatterWeight;
    } else {
        /* Physical single-scatter composite: C = C_scene * T + L_in-scatter */
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
