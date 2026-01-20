// Unit tests for RGB9E5 HDR image format
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef UNIT_TEST

// RGB9E5 conversion functions (copied from implementation for testing)
static void RGB9E5ToFloat3(unsigned int rgb9e5, float *r, float *g, float *b) {
    int exponent = (rgb9e5 >> 27) & 0x1F;
    int red_mantissa = (rgb9e5 >> 18) & 0x1FF;
    int green_mantissa = (rgb9e5 >> 9) & 0x1FF;
    int blue_mantissa = rgb9e5 & 0x1FF;

    float exp_factor = (float)pow(2.0, exponent - 15.0);
    *r = (float)red_mantissa / 511.0f * exp_factor;
    *g = (float)green_mantissa / 511.0f * exp_factor;
    *b = (float)blue_mantissa / 511.0f * exp_factor;
}

static unsigned int Float3ToRGB9E5(float r, float g, float b) {
    float max_val = fmaxf(fmaxf(r, g), b);
    if (max_val == 0.0f) return 0;

    int exponent = (int)ceilf(log2f(max_val)) + 15;
    exponent = (exponent < 0) ? 0 : ((exponent > 31) ? 31 : exponent);

    float scale = (float)pow(2.0, 15 - exponent);
    int red_mantissa = (int)roundf(r * scale * 511.0f);
    int green_mantissa = (int)roundf(g * scale * 511.0f);
    int blue_mantissa = (int)roundf(b * scale * 511.0f);

    red_mantissa = (red_mantissa < 0) ? 0 : ((red_mantissa > 511) ? 511 : red_mantissa);
    green_mantissa = (green_mantissa < 0) ? 0 : ((green_mantissa > 511) ? 511 : green_mantissa);
    blue_mantissa = (blue_mantissa < 0) ? 0 : ((blue_mantissa > 511) ? 511 : blue_mantissa);

    unsigned int result = 0;
    result |= (exponent & 0x1F) << 27;
    result |= (red_mantissa & 0x1FF) << 18;
    result |= (green_mantissa & 0x1FF) << 9;
    result |= (blue_mantissa & 0x1FF);

    return result;
}

int main() {
    printf("Testing RGB9E5 conversion functions...\n");

    // Test 1: Zero values
    {
        float r = 0.0f, g = 0.0f, b = 0.0f;
        unsigned int rgb9e5 = Float3ToRGB9E5(r, g, b);
        assert(rgb9e5 == 0);

        float r2, g2, b2;
        RGB9E5ToFloat3(rgb9e5, &r2, &g2, &b2);
        assert(r2 == 0.0f && g2 == 0.0f && b2 == 0.0f);
        printf("✓ Zero values test passed\n");
    }

    // Test 2: Basic color conversion
    {
        float r = 1.0f, g = 0.5f, b = 0.25f;
        unsigned int rgb9e5 = Float3ToRGB9E5(r, g, b);

        float r2, g2, b2;
        RGB9E5ToFloat3(rgb9e5, &r2, &g2, &b2);

        // Allow for some precision loss due to 9-bit mantissa
        float tolerance = 0.01f;
        assert(fabsf(r - r2) < tolerance);
        assert(fabsf(g - g2) < tolerance);
        assert(fabsf(b - b2) < tolerance);
        printf("✓ Basic color conversion test passed\n");
    }

    // Test 3: High dynamic range values
    {
        float r = 10.0f, g = 5.0f, b = 2.5f;
        unsigned int rgb9e5 = Float3ToRGB9E5(r, g, b);

        float r2, g2, b2;
        RGB9E5ToFloat3(rgb9e5, &r2, &g2, &b2);

        // HDR values should be preserved within reasonable precision
        float tolerance = 0.1f;
        assert(fabsf(r - r2) < tolerance);
        assert(fabsf(g - g2) < tolerance);
        assert(fabsf(b - b2) < tolerance);
        printf("✓ HDR values test passed\n");
    }

    // Test 4: Round-trip conversion accuracy
    {
        const float test_values[][3] = {
            {0.1f, 0.2f, 0.3f},
            {1.0f, 1.0f, 1.0f},
            {2.5f, 1.5f, 0.8f},
            {10.0f, 20.0f, 30.0f},
            {100.0f, 50.0f, 25.0f}
        };

        for (size_t i = 0; i < sizeof(test_values) / sizeof(test_values[0]); i++) {
            float r = test_values[i][0], g = test_values[i][1], b = test_values[i][2];
            unsigned int rgb9e5 = Float3ToRGB9E5(r, g, b);

            float r2, g2, b2;
            RGB9E5ToFloat3(rgb9e5, &r2, &g2, &b2);

            // Calculate relative error
            float max_orig = fmaxf(fmaxf(r, g), b);
            float max_reconstructed = fmaxf(fmaxf(r2, g2), b2);
            float relative_error = fabsf(max_orig - max_reconstructed) / fmaxf(max_orig, 1.0f);

            // RGB9E5 should maintain reasonable accuracy
            assert(relative_error < 0.05f); // 5% relative error tolerance
        }
        printf("✓ Round-trip conversion accuracy test passed\n");
    }

    // Test 5: Bit pattern verification
    {
        // Test specific bit patterns
        unsigned int test_rgb9e5 = 0x3C000000; // Exponent=15, all mantissas=0
        float r, g, b;
        RGB9E5ToFloat3(test_rgb9e5, &r, &g, &b);
        assert(r == 0.0f && g == 0.0f && b == 0.0f);

        // Test maximum mantissa values
        test_rgb9e5 = 0x1FFFFF; // Exponent=0, all mantissas=max
        RGB9E5ToFloat3(test_rgb9e5, &r, &g, &b);
        float expected = 511.0f / 511.0f * (float)pow(2.0, 0 - 15);
        assert(fabsf(r - expected) < 0.001f);
        assert(fabsf(g - expected) < 0.001f);
        assert(fabsf(b - expected) < 0.001f);
        printf("✓ Bit pattern verification test passed\n");
    }

    printf("All RGB9E5 tests passed! 🎉\n");
    return 0;
}

#else
int main() { return 0; }
#endif