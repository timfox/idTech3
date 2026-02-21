#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D sceneColor;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler3D froxelVolume;

const int MAX_VOLUMES = 24;
const int MAX_LIGHTS = 32;

layout(std140, binding = 3) uniform VolumetricParams {
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
} params;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float saturate(float v) {
    return clamp(v, 0.0, 1.0);
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

void main() {
    float depthSample = texture(depthTexture, v_UV).r;
    int depthMode = int(clamp(floor(params.miscParams.y + 0.5), 0.0, 2.0));
    int fogDebug = int(clamp(floor(params.gridDim.w + 0.5), 0.0, 6.0));
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
        float sigma = texture(froxelVolume, vec3(v_UV, 0.5)).a;
        fragColor = vec4(sigma, sigma, sigma, 1.0);
        return;
    }
    if (fogDebug == 3) {
        vec3 scatter = texture(froxelVolume, vec3(v_UV, 0.5)).rgb;
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
            vec4 media = texture(froxelVolume, uvw);
            float sigmaT = max(media.a, 0.0);
            vec3 scatterSource = max(media.rgb, vec3(0.0));

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
        vec3 scatter = texture(froxelVolume, vec3(v_UV, 0.5)).rgb;
        fragColor = vec4(scatter, 1.0);
        return;
    }

    vec3 outRgb = scene * transmittance + fogRadiance;
    fragColor = vec4(outRgb, 1.0);
}
