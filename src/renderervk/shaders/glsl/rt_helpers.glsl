// Ray tracing helper functions

// Camera and ray generation
vec3 getWorldRayDirection(vec2 uv, mat4 viewInverse, mat4 projInverse) {
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 viewSpace = projInverse * clipSpace;
    viewSpace /= viewSpace.w;
    vec4 worldSpace = viewInverse * vec4(viewSpace.xyz, 0.0);
    return normalize(worldSpace.xyz);
}

vec3 getWorldRayOrigin(mat4 viewInverse) {
    return (viewInverse * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
}

float linearizeDepth(float depth, float near, float far) {
    return (2.0 * near) / (far + near - depth * (far - near));
}

// Tonemapping operators
vec3 tonemapReinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

vec3 tonemapACES(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 tonemapUncharted2(vec3 x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemapFilmic(vec3 color) {
    vec3 curr = tonemapUncharted2(color);
    vec3 whiteScale = 1.0 / tonemapUncharted2(vec3(11.2));
    return curr * whiteScale;
}

// Random number generation (using blue noise or hash)
uint hash(uint x) {
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

float random(uint seed) {
    return float(hash(seed)) / 4294967296.0;
}

vec2 random2(uint seed) {
    return vec2(random(seed), random(hash(seed)));
}

vec3 random3(uint seed) {
    return vec3(random(seed), random(hash(seed)), random(hash(hash(seed))));
}

// PBR utility functions
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / max(denom, EPSILON);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / max(denom, EPSILON);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// PBR lighting calculation
vec3 calculatePBR(vec3 albedo, float metallic, float roughness, vec3 N, vec3 V, vec3 L, vec3 lightColor) {
    vec3 H = normalize(V + L);
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + EPSILON;
    vec3 specular = numerator / max(denominator, EPSILON);
    
    float NdotL = max(dot(N, L), 0.0);
    
    return (kD * albedo / PI + specular) * lightColor * NdotL;
}

// Normal mapping
vec3 perturbNormal(vec3 normal, vec3 tangent, vec3 bitangent, vec2 normalMap) {
    vec3 normalMapVec = normalMap * 2.0 - 1.0;
    mat3 TBN = mat3(tangent, bitangent, normal);
    return normalize(TBN * normalMapVec);
}

// Sky color calculation
vec3 getSkyColor(vec3 rayDir) {
    float t = clamp(rayDir.y, -1.0, 1.0);
    
    vec3 skyTop = vec3(0.5, 0.7, 1.0);
    vec3 skyHorizon = vec3(0.8, 0.8, 0.9);
    vec3 skyBottom = vec3(0.3, 0.3, 0.4);
    
    if (t > 0.0) {
        return mix(skyHorizon, skyTop, t);
    } else {
        return mix(skyHorizon, skyBottom, -t);
    }
}

// Color space conversions
vec3 linearToSRGB(vec3 linear) {
    return pow(linear, vec3(1.0 / 2.2));
}

vec3 sRGBToLinear(vec3 srgb) {
    return pow(srgb, vec3(2.2));
}

// Utility functions
vec3 reflectRay(vec3 I, vec3 N) {
    return I - 2.0 * dot(I, N) * N;
}

vec3 refractRay(vec3 I, vec3 N, float eta) {
    float cosI = -dot(N, I);
    float sinT2 = eta * eta * (1.0 - cosI * cosI);
    if (sinT2 > 1.0) {
        return reflectRay(I, N); // Total internal reflection
    }
    float cosT = sqrt(1.0 - sinT2);
    return eta * I + (eta * cosI - cosT) * N;
}

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// Ambient Occlusion helpers
vec3 getHemisphereSample(uint index, vec3 normal, uint seed) {
    // Generate uniform hemisphere samples using Hammersley sequence
    float u = float(index) / float(AO_NUM_SAMPLES);
    float v = 0.0;
    
    // Van der Corput sequence
    float b = 0.5;
    float t = u;
    v = 0.0;
    for (int i = 0; i < 32; i++) {
        if (t >= b) {
            v += 1.0 / (2.0 * b);
            t -= b;
        }
        b *= 0.5;
    }
    
    // Map to hemisphere
    float phi = 2.0 * PI * u;
    float cosTheta = 1.0 - v;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 sampleDir = vec3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );
    
    // Transform to normal space
    vec3 tangent = normalize(vec3(1.0, 0.0, 0.0) - normal * dot(vec3(1.0, 0.0, 0.0), normal));
    if (length(tangent) < 0.1) {
        tangent = normalize(vec3(0.0, 1.0, 0.0) - normal * dot(vec3(0.0, 1.0, 0.0), normal));
    }
    vec3 bitangent = cross(normal, tangent);
    
    return normalize(tangent * sampleDir.x + bitangent * sampleDir.y + normal * sampleDir.z);
}

vec3 getRandomHemisphereSample(vec3 normal, uint seed) {
    // Generate random direction in hemisphere
    vec3 randomVec = normalize(random3(seed));
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    
    float u = random(seed);
    float v = random(hash(seed));
    
    float phi = 2.0 * PI * u;
    float cosTheta = v;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 sampleDir = vec3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );
    
    return normalize(tangent * sampleDir.x + bitangent * sampleDir.y + normal * sampleDir.z);
}

// Calculate AO factor from hit distance
float calculateAO(float hitDistance, float maxDistance) {
    if (hitDistance >= maxDistance) {
        return 1.0; // No occlusion
    }
    
    float occlusion = 1.0 - (hitDistance / maxDistance);
    occlusion = pow(occlusion, AO_POWER);
    return 1.0 - occlusion;
}

// Multi-bounce AO helper
vec3 getMAOSample(vec3 position, vec3 normal, uint bounce, uint sampleIndex, uint seed) {
    vec3 sampleDir = getRandomHemisphereSample(normal, seed + bounce * 1000u + sampleIndex);
    return sampleDir;
}
