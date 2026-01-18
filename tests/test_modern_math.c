/*
===============================================================================
Modern Mathematics Test Program

Tests the modern C++23 mathematics library features.
===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Include modern math header for testing
#include "../src/common/q_math_modern.h"

// Simple test implementations for functions that might not be available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test constants
#define TEST_VEC3_TOLERANCE 1e-6f
#define TEST_FLOAT_TOLERANCE 1e-6f

//===============================================================================
// Test Helper Functions
//===============================================================================

static int test_count = 0;
static int test_passed = 0;

#define TEST_BEGIN(name) printf("Testing %s...\n", name); test_count++
#define TEST_PASS() printf("  PASSED\n"); test_passed++
#define TEST_FAIL(msg) printf("  FAILED: %s\n", msg)

// Vector comparison
static int Vec3Equal(const vec3_t a, const vec3_t b, float tolerance) {
    return fabsf(a[0] - b[0]) < tolerance &&
           fabsf(a[1] - b[1]) < tolerance &&
           fabsf(a[2] - b[2]) < tolerance;
}

static int FloatEqual(float a, float b, float tolerance) {
    return fabsf(a - b) < tolerance;
}

//===============================================================================
// Test Functions
//===============================================================================

static void TestVectorMath() {
    TEST_BEGIN("Vector Mathematics");

    vec3_t a = {1.0f, 2.0f, 3.0f};
    vec3_t b = {4.0f, 5.0f, 6.0f};
    vec3_t result;
    vec3_t expected;

    // Test addition
    VectorMath::Add(a, b, result);
    expected[0] = 5.0f; expected[1] = 7.0f; expected[2] = 9.0f;
    if (!Vec3Equal(result, expected, TEST_VEC3_TOLERANCE)) {
        TEST_FAIL("Vector addition failed");
        return;
    }

    // Test scaling
    VectorMath::Scale(result, 0.5f);
    expected[0] = 2.5f; expected[1] = 3.5f; expected[2] = 4.5f;
    if (!Vec3Equal(result, expected, TEST_VEC3_TOLERANCE)) {
        TEST_FAIL("Vector scaling failed");
        return;
    }

    // Test dot product
    float dot = VectorMath::Dot(a, b);
    if (!FloatEqual(dot, 32.0f, TEST_FLOAT_TOLERANCE)) {
        TEST_FAIL("Dot product failed");
        return;
    }

    // Test cross product
    VectorMath::Cross(a, b, result);
    expected[0] = -3.0f; expected[1] = 6.0f; expected[2] = -3.0f;
    if (!Vec3Equal(result, expected, TEST_VEC3_TOLERANCE)) {
        TEST_FAIL("Cross product failed");
        return;
    }

    // Test length
    vec3_t unit = {1.0f, 0.0f, 0.0f};
    float length = VectorMath::Length(unit);
    if (!FloatEqual(length, 1.0f, TEST_FLOAT_TOLERANCE)) {
        TEST_FAIL("Vector length failed");
        return;
    }

    // Test normalization
    vec3_t toNormalize = {3.0f, 4.0f, 0.0f};
    VectorMath::Normalize(toNormalize);
    float normalizedLength = VectorMath::Length(toNormalize);
    if (!FloatEqual(normalizedLength, 1.0f, TEST_FLOAT_TOLERANCE)) {
        TEST_FAIL("Vector normalization failed");
        return;
    }

    TEST_PASS();
}

static void TestMatrixMath() {
    TEST_BEGIN("Matrix Mathematics");

    MatrixMath::Matrix4x4 identity;
    MatrixMath::Identity(identity);

    // Check identity matrix
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            if (!FloatEqual(identity[i][j], expected, TEST_FLOAT_TOLERANCE)) {
                TEST_FAIL("Identity matrix construction failed");
                return;
            }
        }
    }

    // Test perspective matrix creation
    auto perspective = MatrixMath::Perspective(45.0f * M_PI / 180.0f, 16.0f/9.0f, 0.1f, 1000.0f);

    // Check some known values (simplified check)
    if (perspective[0][0] <= 0.0f || perspective[1][1] <= 0.0f) {
        TEST_FAIL("Perspective matrix construction failed");
        return;
    }

    TEST_PASS();
}

static void TestAABB() {
    TEST_BEGIN("Axis-Aligned Bounding Box");

    // Test AABB creation and operations
    vec3_t min = {-1.0f, -1.0f, -1.0f};
    vec3_t max = {1.0f, 1.0f, 1.0f};
    AABB box{min, max};

    // Test center calculation
    vec3_t center = box.Center();
    vec3_t expectedCenter = {0.0f, 0.0f, 0.0f};
    if (!Vec3Equal(center, expectedCenter, TEST_VEC3_TOLERANCE)) {
        TEST_FAIL("AABB center calculation failed");
        return;
    }

    // Test size calculation
    vec3_t size = box.Size();
    vec3_t expectedSize = {2.0f, 2.0f, 2.0f};
    if (!Vec3Equal(size, expectedSize, TEST_VEC3_TOLERANCE)) {
        TEST_FAIL("AABB size calculation failed");
        return;
    }

    // Test point containment
    vec3_t inside = {0.0f, 0.0f, 0.0f};
    vec3_t outside = {2.0f, 2.0f, 2.0f};

    if (!box.ContainsPoint(inside)) {
        TEST_FAIL("Point containment test failed (inside point)");
        return;
    }

    if (box.ContainsPoint(outside)) {
        TEST_FAIL("Point containment test failed (outside point)");
        return;
    }

    // Test intersection
    AABB intersecting{min, max};
    AABB nonIntersecting = {{2.0f, 2.0f, 2.0f}, {3.0f, 3.0f, 3.0f}};

    if (!box.Intersects(intersecting)) {
        TEST_FAIL("AABB intersection test failed (should intersect)");
        return;
    }

    if (box.Intersects(nonIntersecting)) {
        TEST_FAIL("AABB intersection test failed (should not intersect)");
        return;
    }

    TEST_PASS();
}

static void TestColorMath() {
    TEST_BEGIN("Color Mathematics");

    // Test sRGB conversion (simplified test)
    float linear = 0.5f;
    float srgb = ColorMath::LinearToSRGB(linear);
    float backToLinear = ColorMath::SRGBToLinear(srgb);

    if (!FloatEqual(linear, backToLinear, 0.01f)) { // Allow some precision loss
        TEST_FAIL("sRGB conversion roundtrip failed");
        return;
    }

    // Test color conversion
    color4ub_t colorBytes = {128, 64, 192, 255};
    auto floatColor = ColorMath::Color4ubToFloat(colorBytes);
    auto backToBytes = ColorMath::FloatToColor4ub(floatColor);

    if (abs((int)backToBytes[0] - (int)colorBytes[0]) > 2 ||
        abs((int)backToBytes[1] - (int)colorBytes[1]) > 2 ||
        abs((int)backToBytes[2] - (int)colorBytes[2]) > 2) {
        TEST_FAIL("Color conversion roundtrip failed");
        return;
    }

    TEST_PASS();
}

static void TestRandomMath() {
    TEST_BEGIN("Random Number Generation");

    RandomMath rng{12345};

    // Test basic random generation
    float val1 = rng.NextFloat();
    float val2 = rng.NextFloat();

    if (val1 < 0.0f || val1 >= 1.0f || val2 < 0.0f || val2 >= 1.0f) {
        TEST_FAIL("Random float generation out of range");
        return;
    }

    if (FloatEqual(val1, val2, TEST_FLOAT_TOLERANCE)) {
        TEST_FAIL("Random values should be different");
        return;
    }

    // Test ranged random
    float ranged = rng.NextFloat(5.0f, 10.0f);
    if (ranged < 5.0f || ranged >= 10.0f) {
        TEST_FAIL("Ranged random generation failed");
        return;
    }

    // Test unit sphere generation
    vec3_t spherePoint = rng.RandomUnitSphere();
    float sphereLength = VectorMath::Length(spherePoint);

    if (!FloatEqual(sphereLength, 1.0f, 0.01f)) {
        TEST_FAIL("Unit sphere point generation failed");
        return;
    }

    TEST_PASS();
}

static void TestInterpolationMath() {
    TEST_BEGIN("Interpolation and Easing");

    // Test smoothstep
    float smooth = InterpolationMath::Smoothstep(0.0f, 1.0f, 0.5f);
    if (!FloatEqual(smooth, 0.5f, TEST_FLOAT_TOLERANCE)) {
        TEST_FAIL("Smoothstep interpolation failed");
        return;
    }

    // Test smootherstep
    float smoother = InterpolationMath::Smootherstep(0.0f, 1.0f, 0.5f);
    if (smoother <= 0.0f || smoother >= 1.0f) {
        TEST_FAIL("Smootherstep interpolation failed");
        return;
    }

    // Test easing functions
    float easeIn = InterpolationMath::EaseInQuad(0.5f);
    float easeOut = InterpolationMath::EaseOutQuad(0.5f);
    float easeInOut = InterpolationMath::EaseInOutQuad(0.5f);

    if (easeIn < 0.0f || easeIn > 1.0f ||
        easeOut < 0.0f || easeOut > 1.0f ||
        easeInOut < 0.0f || easeInOut > 1.0f) {
        TEST_FAIL("Easing functions out of range");
        return;
    }

    TEST_PASS();
}

static void TestProceduralTextures() {
    TEST_BEGIN("Procedural Texture Generation");

    proceduralParams_t params = {
        .type = PROC_NOISE_PERLIN,
        .octaves = 2,
        .frequency = 4.0f,
        .amplitude = 1.0f,
        .persistence = 0.5f,
        .lacunarity = 2.0f,
        .seed = 12345
    };

    const int width = 16, height = 16, channels = 1;
    float* textureData = (float*)malloc(width * height * channels * sizeof(float));

    if (!textureData) {
        TEST_FAIL("Could not allocate texture memory");
        return;
    }

    // Generate texture
    if (!Procedural_GenerateTexture(width, height, channels, &params, textureData)) {
        free(textureData);
        TEST_FAIL("Procedural texture generation failed");
        return;
    }

    // Check that all values are in valid range
    qboolean validRange = qtrue;
    for (int i = 0; i < width * height * channels; ++i) {
        if (textureData[i] < 0.0f || textureData[i] > 1.0f) {
            validRange = qfalse;
            break;
        }
    }

    free(textureData);

    if (!validRange) {
        TEST_FAIL("Procedural texture values out of range");
        return;
    }

    TEST_PASS();
}

//===============================================================================
// Main Test Function
//===============================================================================

int main(int argc, char* argv[]) {
    printf("Modern Mathematics Library Test Suite\n");
    printf("=====================================\n\n");

    // Run all tests
    TestVectorMath();
    TestMatrixMath();
    TestAABB();
    TestColorMath();
    TestRandomMath();
    TestInterpolationMath();
    TestProceduralTextures();

    printf("\n=====================================\n");
    printf("Test Results: %d/%d passed\n", test_passed, test_count);

    if (test_passed == test_count) {
        printf("All tests PASSED! 🎉\n");
        return 0;
    } else {
        printf("Some tests FAILED! ❌\n");
        return 1;
    }
}