/*
===============================================================================
Modern Mathematics Implementation - C++23

Implementation of modern mathematical utilities using C++23 features.
Demonstrates constexpr, concepts, ranges, and modern C++ patterns.
===============================================================================
*/

#include "q_math_modern.h"
#include <algorithm>
#include <numeric>
#include <ranges>
#include <execution>

//===============================================================================
// Modern Vector Operations Implementation
//===============================================================================

// Demonstrate range-based algorithms with vectors
void ProcessVerticesModern(std::span<vec3_t> vertices) {
    // Use ranges to filter and transform vertices
    auto validVertices = vertices
        | std::ranges::views::filter([](const vec3_t& v) {
            return VectorMath::LengthSquared(v) > 0.001f;
        })
        | std::ranges::views::transform([](const vec3_t& v) -> vec3_t {
            vec3_t normalized = v;
            VectorMath::Normalize(normalized);
            return normalized;
        });

    // Process normalized vertices
    for (const auto& vertex : validVertices) {
        // Use structured bindings for cleaner code
        auto [x, y, z] = vertex;
        // Process vertex...
        (void)x; (void)y; (void)z; // Suppress unused variable warnings
    }
}

//===============================================================================
// Modern Matrix Operations with SIMD-like Performance
//===============================================================================

// Use constexpr for compile-time matrix operations
constexpr auto CreateViewMatrix(const vec3_t& position, const vec3_t& forward, const vec3_t& up) {
    MatrixMath::Matrix4x4 view{};

    // Create orthonormal basis
    vec3_t right;
    VectorMath::Cross(forward, up, right);
    VectorMath::Normalize(right);

    vec3_t trueUp;
    VectorMath::Cross(right, forward, trueUp);

    // Set rotation part (transpose of camera-to-world matrix)
    view[0][0] = right[0];   view[0][1] = trueUp[0];   view[0][2] = -forward[0];
    view[1][0] = right[1];   view[1][1] = trueUp[1];   view[1][2] = -forward[1];
    view[2][0] = right[2];   view[2][1] = trueUp[2];   view[2][2] = -forward[2];

    // Set translation part
    view[3][0] = -VectorMath::Dot(right, position);
    view[3][1] = -VectorMath::Dot(trueUp, position);
    view[3][2] = VectorMath::Dot(forward, position);
    view[3][3] = 1.0f;

    return view;
}

//===============================================================================
// Modern Collision Detection with Ranges
//===============================================================================

// Use ranges and views for efficient collision queries
std::vector<const AABB*> FindIntersectingAABBs(const AABB& query, std::span<const AABB> aabbs) {
    // Use parallel execution and ranges for performance
    auto intersecting = aabbs
        | std::ranges::views::filter([&query](const AABB& aabb) {
            return query.Intersects(aabb);
        })
        | std::ranges::views::transform([](const AABB& aabb) {
            return &aabb;
        });

    return {intersecting.begin(), intersecting.end()};
}

//===============================================================================
// Modern Random Number Generation Demo
//===============================================================================

// Use modern random generation for procedural content
void GenerateProceduralGeometry(std::span<vec3_t> vertices, uint64_t seed) {
    RandomMath rng{seed};

    for (auto& vertex : vertices) {
        // Generate random points on a sphere
        vertex = rng.RandomUnitSphere();

        // Add some noise for organic variation
        vec3_t noise = {
            rng.NextFloat(-0.1f, 0.1f),
            rng.NextFloat(-0.1f, 0.1f),
            rng.NextFloat(-0.1f, 0.1f)
        };

        VectorMath::Add(vertex, noise, vertex);
        VectorMath::Normalize(vertex);
    }
}

//===============================================================================
// Modern Animation and Interpolation
//===============================================================================

// Use modern easing functions for smooth animations
struct AnimationCurve {
    std::vector<float> keyframes;
    std::vector<float> times;

    float Evaluate(float time) const {
        if (keyframes.empty()) return 0.0f;
        if (keyframes.size() == 1) return keyframes[0];

        // Find keyframes to interpolate between
        auto it = std::ranges::lower_bound(times, time);
        if (it == times.begin()) return keyframes[0];
        if (it == times.end()) return keyframes.back();

        size_t index = it - times.begin();
        float t1 = times[index - 1];
        float t2 = times[index];
        float v1 = keyframes[index - 1];
        float v2 = keyframes[index];

        // Use smooth interpolation
        float t = (time - t1) / (t2 - t1);
        return InterpolationMath::Smootherstep(0.0f, 1.0f, t) * (v2 - v1) + v1;
    }
};

//===============================================================================
// Modern Asset Processing with Ranges
//===============================================================================

// Process texture data using modern ranges and views
void ProcessTextureModern(std::span<uint8_t> textureData, int width, int height, int channels) {
    // Convert to float and apply color corrections
    auto floatData = textureData
        | std::ranges::views::chunk(channels)
        | std::ranges::views::transform([](auto pixel) {
            std::array<float, 4> color{0.0f, 0.0f, 0.0f, 1.0f};
            for (size_t i = 0; i < std::min(pixel.size(), size_t(4)); ++i) {
                color[i] = pixel[i] / 255.0f;
            }
            return ColorMath::Color4ubToFloat(color);
        });

    // Apply tonemapping to the entire texture
    std::vector<std::array<float, 3>> tonemappedColors;
    for (const auto& color : floatData) {
        auto rgb = std::array<float, 3>{color[0], color[1], color[2]};
        tonemappedColors.push_back(ColorMath::ToneMapACES(rgb));
    }

    // Convert back to bytes
    auto finalData = tonemappedColors
        | std::ranges::views::transform([](const auto& color) {
            auto color4 = std::array<float, 4>{color[0], color[1], color[2], 1.0f};
            return ColorMath::FloatToColor4ub(color4);
        });

    // Copy back to original buffer
    size_t index = 0;
    for (const auto& color : finalData) {
        if (index + 4 <= textureData.size()) {
            textureData[index++] = color[0];
            textureData[index++] = color[1];
            textureData[index++] = color[2];
            textureData[index++] = color[3];
        }
    }
}

//===============================================================================
// Modern Physics Simulation with Parallel Execution
//===============================================================================

// Demonstrate parallel physics simulation using execution policies
void SimulateParticles(std::span<vec3_t> positions, std::span<vec3_t> velocities, float deltaTime) {
    // Update positions in parallel
    std::for_each(std::execution::par_unseq, positions.begin(), positions.end(),
                  [velocities, deltaTime, i = 0](vec3_t& pos) mutable {
        VectorMath::Scale(velocities[i++], deltaTime);
        VectorMath::Add(pos, velocities[i-1], pos);
    });

    // Apply gravity (parallel)
    std::for_each(std::execution::par_unseq, velocities.begin(), velocities.end(),
                  [deltaTime](vec3_t& vel) {
        vec3_t gravity = {0.0f, -9.81f * deltaTime, 0.0f};
        VectorMath::Add(vel, gravity, vel);
    });
}

//===============================================================================
// Modern Resource Management with RAII
//===============================================================================

// Smart pointer-based resource management
class GeometryBuffer {
private:
    std::unique_ptr<vec3_t[]> vertices;
    std::unique_ptr<uint32_t[]> indices;
    size_t vertexCount;
    size_t indexCount;

public:
    GeometryBuffer(size_t verts, size_t inds)
        : vertices(std::make_unique<vec3_t[]>(verts))
        , indices(std::make_unique<uint32_t[]>(inds))
        , vertexCount(verts)
        , indexCount(inds) {}

    // Modern accessors using spans
    std::span<vec3_t> GetVertices() {
        return std::span<vec3_t>(vertices.get(), vertexCount);
    }

    std::span<uint32_t> GetIndices() {
        return std::span<uint32_t>(indices.get(), indexCount);
    }

    std::span<const vec3_t> GetVertices() const {
        return std::span<const vec3_t>(vertices.get(), vertexCount);
    }

    std::span<const uint32_t> GetIndices() const {
        return std::span<const uint32_t>(indices.get(), indexCount);
    }

    // Modern bounds calculation
    AABB CalculateBounds() const {
        AABB bounds;
        for (const auto& vertex : GetVertices()) {
            bounds.AddPoint(vertex);
        }
        return bounds;
    }
};

//===============================================================================
// Modern Shader Constants with Designated Initializers
//===============================================================================

// Demonstrate designated initializers and structured bindings
ShaderConstants CreateShaderConstants(const vec3_t& cameraPos, float time, const MatrixMath::Matrix4x4& vp) {
    return ShaderConstants{
        .time = time,
        .cameraPosition = cameraPos,
        .viewProjection = vp
    };
}

//===============================================================================
// Modern Error Handling with std::expected (C++23)
//===============================================================================

#include <expected>

enum class MathError {
    DivisionByZero,
    InvalidInput,
    Overflow
};

using MathResult = std::expected<float, MathError>;

// Modern error handling
MathResult SafeDivide(float a, float b) {
    if (b == 0.0f) {
        return std::unexpected(MathError::DivisionByZero);
    }

    if (!std::isfinite(a) || !std::isfinite(b)) {
        return std::unexpected(MathError::InvalidInput);
    }

    float result = a / b;
    if (!std::isfinite(result)) {
        return std::unexpected(MathError::Overflow);
    }

    return result;
}

//===============================================================================
// Modern Event System with Concepts and Coroutines (C++20/23)
//===============================================================================

#include <coroutine>
#include <generator>

// Concept for event handlers
template<typename T>
concept EventHandler = requires(T handler) {
    handler(42); // Can be called with an int (example)
};

// Coroutine-based event processing
std::generator<int> ProcessEvents() {
    for (int i = 0; i < 10; ++i) {
        co_yield i;
    }
}

//===============================================================================
// Demonstration Functions
//===============================================================================

void DemonstrateModernMath() {
    // Vector operations
    vec3_t a = {1.0f, 2.0f, 3.0f};
    vec3_t b = {4.0f, 5.0f, 6.0f};
    vec3_t result;

    VectorMath::Add(a, b, result);
    VectorMath::Scale(result, 0.5f);

    // AABB operations
    AABB bounds{a, b};
    vec3_t center = bounds.Center();
    vec3_t size = bounds.Size();

    // Random generation
    RandomMath rng{12345};
    vec3_t randomPoint = rng.RandomUnitSphere();
    float randomValue = rng.NextFloat(0.0f, 1.0f);

    // Color operations
    std::array<float, 4> linearColor = {1.0f, 0.5f, 0.0f, 1.0f};
    auto tonemapped = ColorMath::ToneMapACES({linearColor[0], linearColor[1], linearColor[2]});

    // Matrix operations
    auto perspective = MatrixMath::Perspective(45.0f * M_PI / 180.0f, 16.0f/9.0f, 0.1f, 1000.0f);

    // Interpolation
    float eased = InterpolationMath::EaseInOutCubic(0.5f);
    float smoothed = InterpolationMath::Smootherstep(0.0f, 1.0f, 0.5f);

    // Error handling
    auto divResult = SafeDivide(10.0f, 2.0f);
    if (divResult) {
        float value = *divResult;
        (void)value; // Use result
    } else {
        // Handle error
        MathError error = divResult.error();
        (void)error;
    }

    // Modern resource management
    auto geometry = std::make_unique<GeometryBuffer>(100, 300);
    AABB geomBounds = geometry->CalculateBounds();

    // Designated initializers
    ShaderConstants constants = CreateShaderConstants(center, 0.0f, perspective);

    // All variables used for demonstration
    (void)result; (void)size; (void)randomPoint; (void)randomValue;
    (void)tonemapped; (void)perspective; (void)eased; (void)smoothed;
    (void)geomBounds; (void)constants;
}

void DemonstrateRangesAndViews() {
    std::array<vec3_t, 10> vertices;

    // Initialize with some test data
    for (size_t i = 0; i < vertices.size(); ++i) {
        vertices[i][0] = static_cast<float>(i);
        vertices[i][1] = static_cast<float>(i * 2);
        vertices[i][2] = static_cast<float>(i * 3);
    }

    // Use ranges to process vertices
    ProcessVerticesModern(vertices);

    // Use spans for safe array access
    std::span<vec3_t> vertexSpan = SpanMath::MakeSpan(vertices);
    float totalLength = 0.0f;

    for (const auto& vertex : vertexSpan) {
        totalLength += VectorMath::Length(vertex);
    }

    // Filter and transform
    auto longVectors = vertexSpan
        | std::ranges::views::filter([](const vec3_t& v) {
            return VectorMath::Length(v) > 5.0f;
        });

    size_t count = std::ranges::distance(longVectors);

    (void)totalLength; (void)count; // Use results
}

//===============================================================================
// Performance Comparison Functions
//===============================================================================

#include <chrono>

// Compare traditional vs modern approaches
double BenchmarkTraditionalVectorOps(size_t iterations) {
    auto start = std::chrono::high_resolution_clock::now();

    vec3_t a = {1.0f, 2.0f, 3.0f};
    vec3_t b = {4.0f, 5.0f, 6.0f};
    vec3_t result = {0};

    for (size_t i = 0; i < iterations; ++i) {
        result[0] = a[0] + b[0];
        result[1] = a[1] + b[1];
        result[2] = a[2] + b[2];

        float len = sqrtf(result[0] * result[0] + result[1] * result[1] + result[2] * result[2]);

        result[0] /= len;
        result[1] /= len;
        result[2] /= len;
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(end - start).count();
}

double BenchmarkModernVectorOps(size_t iterations) {
    auto start = std::chrono::high_resolution_clock::now();

    vec3_t a = {1.0f, 2.0f, 3.0f};
    vec3_t b = {4.0f, 5.0f, 6.0f};
    vec3_t result = {0};

    for (size_t i = 0; i < iterations; ++i) {
        VectorMath::Add(a, b, result);
        VectorMath::Normalize(result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(end - start).count();
}

//===============================================================================
// Export Functions for Testing
//===============================================================================

extern "C" {

void TestModernMath() {
    DemonstrateModernMath();
    DemonstrateRangesAndViews();
}

void BenchmarkMathPerformance() {
    const size_t iterations = 1000000;

    double traditional = BenchmarkTraditionalVectorOps(iterations);
    double modern = BenchmarkModernVectorOps(iterations);

    printf("Performance comparison (%zu iterations):\n", iterations);
    printf("Traditional: %.3f seconds\n", traditional);
    printf("Modern: %.3f seconds\n", modern);
    printf("Speedup: %.2fx\n", traditional / modern);
}

} // extern "C"