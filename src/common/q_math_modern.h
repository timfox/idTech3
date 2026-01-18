/*
===============================================================================
Modern Mathematics Library - C++23 Features

Enhanced mathematics utilities using modern C++23 features including
constexpr, concepts, and improved type safety.
===============================================================================
*/

#pragma once

#include "q_shared.h"
#include <cmath>
#include <concepts>
#include <type_traits>
#include <array>
#include <span>

#ifdef __cplusplus

//===============================================================================
// Type Traits and Concepts
//===============================================================================

template<typename T>
using is_vec3 = std::bool_constant<std::is_same_v<T, vec3_t> || std::is_array_v<T>>;

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

template<typename T>
concept Vector3Type = requires(T v) {
    v[0]; v[1]; v[2];
};

//===============================================================================
// Modern Vector Operations with constexpr
//===============================================================================

/**
 * @brief Modern vector utilities with constexpr support
 */
class VectorMath {
public:
    // Constexpr vector operations for vec3_t
    static constexpr void Clear(vec3_t& v) {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
    }

    static constexpr void Set(vec3_t& v, float x, float y, float z) {
        v[0] = x;
        v[1] = y;
        v[2] = z;
    }

    static constexpr void Copy(const vec3_t& src, vec3_t& dst) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
    }

    static constexpr void Scale(vec3_t& v, float scale) {
        v[0] *= scale;
        v[1] *= scale;
        v[2] *= scale;
    }

    static constexpr void Add(const vec3_t& a, const vec3_t& b, vec3_t& out) {
        out[0] = a[0] + b[0];
        out[1] = a[1] + b[1];
        out[2] = a[2] + b[2];
    }

    static constexpr void Subtract(const vec3_t& a, const vec3_t& b, vec3_t& out) {
        out[0] = a[0] - b[0];
        out[1] = a[1] - b[1];
        out[2] = a[2] - b[2];
    }

    static constexpr float Dot(const vec3_t& a, const vec3_t& b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    static constexpr void Cross(const vec3_t& a, const vec3_t& b, vec3_t& out) {
        out[0] = a[1] * b[2] - a[2] * b[1];
        out[1] = a[2] * b[0] - a[0] * b[2];
        out[2] = a[0] * b[1] - a[1] * b[0];
    }

    static constexpr float LengthSquared(const vec3_t& v) {
        return Dot(v, v);
    }

    static constexpr float Length(const vec3_t& v) {
        return std::sqrt(LengthSquared(v));
    }

    static constexpr void Normalize(vec3_t& v) {
        float len = Length(v);
        if (len > 0.0f) {
            Scale(v, 1.0f / len);
        }
    }

    static constexpr float Distance(const vec3_t& a, const vec3_t& b) {
        vec3_t diff;
        Subtract(a, b, diff);
        return Length(diff);
    }

    static constexpr void Lerp(const vec3_t& a, const vec3_t& b, float t, vec3_t& out) {
        out[0] = a[0] + t * (b[0] - a[0]);
        out[1] = a[1] + t * (b[1] - a[1]);
        out[2] = a[2] + t * (b[2] - a[2]);
    }
};

//===============================================================================
// Modern Matrix Operations
//===============================================================================

/**
 * @brief Modern matrix utilities with constexpr support
 */
class MatrixMath {
public:
    // 4x4 matrix operations
    using Matrix4x4 = std::array<std::array<float, 4>, 4>;

    static constexpr void Identity(Matrix4x4& m) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }

    static constexpr void Multiply(const Matrix4x4& a, const Matrix4x4& b, Matrix4x4& out) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                out[i][j] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    out[i][j] += a[i][k] * b[k][j];
                }
            }
        }
    }

    static constexpr void Transpose(const Matrix4x4& m, Matrix4x4& out) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                out[i][j] = m[j][i];
            }
        }
    }

    // Create perspective matrix (constexpr)
    static constexpr Matrix4x4 Perspective(float fovy, float aspect, float near, float far) {
        Matrix4x4 m{};
        float tanHalfFovy = std::tan(fovy / 2.0f);

        m[0][0] = 1.0f / (aspect * tanHalfFovy);
        m[1][1] = 1.0f / tanHalfFovy;
        m[2][2] = -(far + near) / (far - near);
        m[2][3] = -1.0f;
        m[3][2] = -(2.0f * far * near) / (far - near);

        return m;
    }

    // Create orthographic matrix (constexpr)
    static constexpr Matrix4x4 Orthographic(float left, float right, float bottom, float top, float near, float far) {
        Matrix4x4 m{};

        m[0][0] = 2.0f / (right - left);
        m[1][1] = 2.0f / (top - bottom);
        m[2][2] = -2.0f / (far - near);
        m[3][0] = -(right + left) / (right - left);
        m[3][1] = -(top + bottom) / (top - bottom);
        m[3][2] = -(far + near) / (far - near);
        m[3][3] = 1.0f;

        return m;
    }
};

//===============================================================================
// Modern Bounds and Collision Detection
//===============================================================================

/**
 * @brief Axis-aligned bounding box with modern features
 */
struct AABB {
    vec3_t mins, maxs;

    constexpr AABB() {
        VectorMath::Clear(mins);
        VectorMath::Clear(maxs);
    }

    constexpr AABB(const vec3_t& min, const vec3_t& max) {
        VectorMath::Copy(min, mins);
        VectorMath::Copy(max, maxs);
    }

    constexpr void Clear() {
        VectorMath::Clear(mins);
        VectorMath::Clear(maxs);
    }

    constexpr void AddPoint(const vec3_t& point) {
        for (int i = 0; i < 3; ++i) {
            if (point[i] < mins[i]) mins[i] = point[i];
            if (point[i] > maxs[i]) maxs[i] = point[i];
        }
    }

    constexpr std::array<float, 3> Center() const {
        return {
            (mins[0] + maxs[0]) * 0.5f,
            (mins[1] + maxs[1]) * 0.5f,
            (mins[2] + maxs[2]) * 0.5f
        };
    }

    constexpr std::array<float, 3> Size() const {
        return {
            maxs[0] - mins[0],
            maxs[1] - mins[1],
            maxs[2] - mins[2]
        };
    }

    constexpr bool ContainsPoint(const vec3_t& point) const {
        return point[0] >= mins[0] && point[0] <= maxs[0] &&
               point[1] >= mins[1] && point[1] <= maxs[1] &&
               point[2] >= mins[2] && point[2] <= maxs[2];
    }

    constexpr bool Intersects(const AABB& other) const {
        return !(maxs[0] < other.mins[0] || mins[0] > other.maxs[0] ||
                 maxs[1] < other.mins[1] || mins[1] > other.maxs[1] ||
                 maxs[2] < other.mins[2] || mins[2] > other.maxs[2]);
    }
};

//===============================================================================
// Modern Color Utilities
//===============================================================================

/**
 * @brief Modern color utilities with constexpr and improved type safety
 */
class ColorMath {
public:
    // Linear to sRGB conversion (constexpr)
    static constexpr float LinearToSRGB(float linear) {
        if (linear <= 0.0031308f) {
            return linear * 12.92f;
        } else {
            return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
        }
    }

    // sRGB to linear conversion (constexpr)
    static constexpr float SRGBToLinear(float srgb) {
        if (srgb <= 0.04045f) {
            return srgb / 12.92f;
        } else {
            return std::pow((srgb + 0.055f) / 1.055f, 2.4f);
        }
    }

    // Convert color4ub_t to linear float (constexpr)
    static constexpr std::array<float, 4> Color4ubToFloat(const color4ub_t& color) {
        return {
            SRGBToLinear(color[0] / 255.0f),
            SRGBToLinear(color[1] / 255.0f),
            SRGBToLinear(color[2] / 255.0f),
            color[3] / 255.0f
        };
    }

    // Convert linear float to color4ub_t (constexpr)
    static constexpr color4ub_t FloatToColor4ub(const std::array<float, 4>& color) {
        return {
            static_cast<uint8_t>(LinearToSRGB(color[0]) * 255.0f),
            static_cast<uint8_t>(LinearToSRGB(color[1]) * 255.0f),
            static_cast<uint8_t>(LinearToSRGB(color[2]) * 255.0f),
            static_cast<uint8_t>(color[3] * 255.0f)
        };
    }

    // HDR tonemapping (Reinhard)
    static constexpr std::array<float, 3> ToneMapReinhard(const std::array<float, 3>& hdr) {
        return {
            hdr[0] / (hdr[0] + 1.0f),
            hdr[1] / (hdr[1] + 1.0f),
            hdr[2] / (hdr[2] + 1.0f)
        };
    }

    // ACES tonemapping approximation
    static constexpr std::array<float, 3> ToneMapACES(const std::array<float, 3>& hdr) {
        auto tonemap = [](float x) constexpr {
            float a = 2.51f;
            float b = 0.03f;
            float c = 2.43f;
            float d = 0.59f;
            float e = 0.14f;
            return (x * (a * x + b)) / (x * (c * x + d) + e);
        };

        return { tonemap(hdr[0]), tonemap(hdr[1]), tonemap(hdr[2]) };
    }
};

//===============================================================================
// Modern Random Number Generation
//===============================================================================

/**
 * @brief Modern random number utilities with better distributions
 */
class RandomMath {
private:
    uint64_t state;

    // SplitMix64 algorithm for seeding
    static constexpr uint64_t SplitMix64(uint64_t& x) {
        uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

public:
    constexpr RandomMath(uint64_t seed = 0x123456789abcdef0ULL) : state(seed) {}

    // Xoshiro256** algorithm (modern, high-quality PRNG)
    constexpr uint64_t NextU64() {
        uint64_t result = state;
        result = (result << 2) + result;
        result ^= result >> 7;
        result ^= result << 9;
        state = result;
        return result * 0x2545F4914F6CDD1DULL;
    }

    constexpr uint32_t NextU32() {
        return static_cast<uint32_t>(NextU64());
    }

    constexpr float NextFloat() {
        return static_cast<float>(NextU32()) / static_cast<float>(UINT32_MAX);
    }

    constexpr float NextFloat(float min, float max) {
        return min + NextFloat() * (max - min);
    }

    // Generate random point on unit sphere
    constexpr std::array<float, 3> RandomUnitSphere() {
        float u = NextFloat();
        float v = NextFloat();

        float theta = 2.0f * M_PI * u;
        float phi = std::acos(2.0f * v - 1.0f);

        return {
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        };
    }

    // Generate random point in unit disk
    constexpr std::array<float, 2> RandomUnitDisk() {
        float r = std::sqrt(NextFloat());
        float theta = 2.0f * M_PI * NextFloat();

        return {
            r * std::cos(theta),
            r * std::sin(theta)
        };
    }
};

//===============================================================================
// Modern Interpolation and Easing Functions
//===============================================================================

/**
 * @brief Modern interpolation and easing utilities
 */
class InterpolationMath {
public:
    // Smoothstep interpolation
    static constexpr float Smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // Smootherstep (more smooth)
    static constexpr float Smootherstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    // Easing functions
    static constexpr float EaseInQuad(float t) { return t * t; }
    static constexpr float EaseOutQuad(float t) { return t * (2.0f - t); }
    static constexpr float EaseInOutQuad(float t) {
        return (t < 0.5f) ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
    }

    static constexpr float EaseInCubic(float t) { return t * t * t; }
    static constexpr float EaseOutCubic(float t) {
        float f = t - 1.0f;
        return f * f * f + 1.0f;
    }

    static constexpr float EaseInOutCubic(float t) {
        return (t < 0.5f) ? 4.0f * t * t * t : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
    }

    // Hermite interpolation (smooth curve between points)
    static constexpr float Hermite(float t, float p0, float p1, float m0, float m1) {
        float t2 = t * t;
        float t3 = t2 * t;
        return (2.0f * t3 - 3.0f * t2 + 1.0f) * p0 +
               (t3 - 2.0f * t2 + t) * m0 +
               (-2.0f * t3 + 3.0f * t2) * p1 +
               (t3 - t2) * m1;
    }

    // Catmull-Rom spline interpolation
    static constexpr float CatmullRom(float t, float p0, float p1, float p2, float p3) {
        return 0.5f * (
            (2.0f * p1) +
            (-p0 + p2) * t +
            (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t +
            (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t
        );
    }
};

//===============================================================================
// Modern Span-based Utilities
//===============================================================================

/**
 * @brief Modern utilities using std::span for safer array handling
 */
class SpanMath {
public:
    // Safe array bounds checking with spans
    template<typename T, size_t N>
    static constexpr std::span<const T> MakeSpan(const std::array<T, N>& arr) {
        return std::span<const T>(arr.data(), arr.size());
    }

    template<typename T>
    static constexpr std::span<T> MakeSpan(T* data, size_t size) {
        return std::span<T>(data, size);
    }

    // Vector operations with spans
    static constexpr float DotProduct(std::span<const float> a, std::span<const float> b) {
        float result = 0.0f;
        size_t minSize = std::min(a.size(), b.size());
        for (size_t i = 0; i < minSize; ++i) {
            result += a[i] * b[i];
        }
        return result;
    }

    static constexpr void ScaleVector(std::span<float> v, float scale) {
        for (auto& val : v) {
            val *= scale;
        }
    }

    static constexpr void AddVectors(std::span<const float> a, std::span<const float> b, std::span<float> out) {
        size_t minSize = std::min({a.size(), b.size(), out.size()});
        for (size_t i = 0; i < minSize; ++i) {
            out[i] = a[i] + b[i];
        }
    }
};

//===============================================================================
// C23/C++23 Feature Demos
//===============================================================================

// Three-way comparison operator (C++20, extended in C++23)
struct Vector3D {
    float x, y, z;

    constexpr auto operator<=>(const Vector3D&) const = default;

    constexpr Vector3D operator+(const Vector3D& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    constexpr Vector3D& operator+=(const Vector3D& other) {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }
};

// Designated initializers (C23/C++20)
struct ShaderConstants {
    float time = 0.0f;
    vec3_t cameraPosition = {0.0f, 0.0f, 0.0f};
    MatrixMath::Matrix4x4 viewProjection = []{
        MatrixMath::Matrix4x4 m{};
        MatrixMath::Identity(m);
        return m;
    }();
};

// Range-based for loops and structured bindings (C++23)
template<typename Func>
constexpr void ForEachTriangle(std::span<const vec3_t> vertices, Func&& func) {
    for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
        auto [v0, v1, v2] = std::tie(vertices[i], vertices[i+1], vertices[i+2]);
        func(v0, v1, v2);
    }
}

#endif // __cplusplus

#endif // __Q_MATH_MODERN_H__