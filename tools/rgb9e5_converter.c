/*
===============================================================================

RGB9E5 Converter Tool

Command-line utility for converting HDR images to RGB9E5 format and vice versa.

Usage:
  rgb9e5_converter input.exr output.rgb9e5    # Convert EXR to RGB9E5
  rgb9e5_converter input.rgb9e5 output.exr   # Convert RGB9E5 to EXR

===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

// Minimal OpenEXR support for reading/writing EXR files
// This is a simplified implementation - for production use,
// consider using the full OpenEXR library

typedef struct {
    char magic[4];      // "RGB9"
    int32_t width;
    int32_t height;
    int32_t flags;      // Reserved
} rgb9e5_header_t;

typedef struct {
    uint32_t data;      // RGB9E5 packed data
} rgb9e5_pixel_t;

// RGB9E5 conversion functions (from engine implementation)
static void RGB9E5ToFloat3(uint32_t rgb9e5, float *r, float *g, float *b) {
    int exponent = (rgb9e5 >> 27) & 0x1F;
    int red_mantissa = (rgb9e5 >> 18) & 0x1FF;
    int green_mantissa = (rgb9e5 >> 9) & 0x1FF;
    int blue_mantissa = rgb9e5 & 0x1FF;

    float exp_factor = (float)pow(2.0, exponent - 15.0);
    *r = (float)red_mantissa / 511.0f * exp_factor;
    *g = (float)green_mantissa / 511.0f * exp_factor;
    *b = (float)blue_mantissa / 511.0f * exp_factor;
}

static uint32_t Float3ToRGB9E5(float r, float g, float b) {
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

    uint32_t result = 0;
    result |= (exponent & 0x1F) << 27;
    result |= (red_mantissa & 0x1FF) << 18;
    result |= (green_mantissa & 0x1FF) << 9;
    result |= (blue_mantissa & 0x1FF);

    return result;
}

// Simplified EXR reading (only supports basic RGB EXR files)
// This is a minimal implementation - production code should use OpenEXR library
static int LoadEXR(const char *filename, float **data, int *width, int *height) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return 0;
    }

    // Read EXR header (simplified - real EXR has complex header structure)
    // For this demo, we'll assume a simple format
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // This is a placeholder - real EXR parsing is complex
    // For demonstration purposes, we'll create dummy HDR data
    *width = 512;
    *height = 512;
    *data = (float *)malloc((*width) * (*height) * 3 * sizeof(float));

    if (!*data) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(f);
        return 0;
    }

    // Generate simple HDR test pattern
    float *ptr = *data;
    for (int y = 0; y < *height; y++) {
        for (int x = 0; x < *width; x++) {
            // Create a simple HDR gradient
            float u = (float)x / (*width - 1);
            float v = (float)y / (*height - 1);

            *ptr++ = u * 2.0f;        // R: 0-2 range
            *ptr++ = v * 2.0f;        // G: 0-2 range
            *ptr++ = (u + v) * 1.0f;  // B: 0-2 range
        }
    }

    fclose(f);
    printf("Note: Using generated test HDR data (real EXR parsing not implemented)\n");
    return 1;
}

// Simplified EXR writing (placeholder)
static int SaveEXR(const char *filename, const float *data, int width, int height) {
    printf("EXR saving not implemented in this demo tool\n");
    printf("Use OpenEXR library for proper EXR support\n");
    return 0;
}

static int LoadRGB9E5(const char *filename, float **data, int *width, int *height) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return 0;
    }

    rgb9e5_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fprintf(stderr, "Error: Failed to read RGB9E5 header\n");
        fclose(f);
        return 0;
    }

    // Validate magic
    if (memcmp(header.magic, "RGB9", 4) != 0) {
        fprintf(stderr, "Error: Invalid RGB9E5 magic number\n");
        fclose(f);
        return 0;
    }

    *width = header.width;
    *height = header.height;

    // Allocate output buffer
    *data = (float *)malloc((*width) * (*height) * 3 * sizeof(float));
    if (!*data) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(f);
        return 0;
    }

    // Read RGB9E5 data
    rgb9e5_pixel_t *rgb9e5_data = (rgb9e5_pixel_t *)malloc((*width) * (*height) * sizeof(rgb9e5_pixel_t));
    if (!rgb9e5_data) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(*data);
        fclose(f);
        return 0;
    }

    if (fread(rgb9e5_data, sizeof(rgb9e5_pixel_t), (*width) * (*height), f) != (size_t)((*width) * (*height))) {
        fprintf(stderr, "Error: Failed to read RGB9E5 pixel data\n");
        free(rgb9e5_data);
        free(*data);
        fclose(f);
        return 0;
    }

    // Convert to float RGB
    float *dst = *data;
    for (int i = 0; i < (*width) * (*height); i++) {
        float r, g, b;
        RGB9E5ToFloat3(rgb9e5_data[i].data, &r, &g, &b);
        *dst++ = r;
        *dst++ = g;
        *dst++ = b;
    }

    free(rgb9e5_data);
    fclose(f);
    return 1;
}

static int SaveRGB9E5(const char *filename, const float *data, int width, int height) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Error: Cannot create file '%s'\n", filename);
        return 0;
    }

    rgb9e5_header_t header;
    memcpy(header.magic, "RGB9", 4);
    header.width = width;
    header.height = height;
    header.flags = 0;

    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        fprintf(stderr, "Error: Failed to write RGB9E5 header\n");
        fclose(f);
        return 0;
    }

    // Convert float RGB to RGB9E5
    rgb9e5_pixel_t *rgb9e5_data = (rgb9e5_pixel_t *)malloc(width * height * sizeof(rgb9e5_pixel_t));
    if (!rgb9e5_data) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(f);
        return 0;
    }

    const float *src = data;
    for (int i = 0; i < width * height; i++) {
        float r = *src++;
        float g = *src++;
        float b = *src++;
        rgb9e5_data[i].data = Float3ToRGB9E5(r, g, b);
    }

    if (fwrite(rgb9e5_data, sizeof(rgb9e5_pixel_t), width * height, f) != (size_t)(width * height)) {
        fprintf(stderr, "Error: Failed to write RGB9E5 pixel data\n");
        free(rgb9e5_data);
        fclose(f);
        return 0;
    }

    free(rgb9e5_data);
    fclose(f);
    return 1;
}

static void PrintUsage(const char *program_name) {
    fprintf(stderr, "Usage: %s <input_file> <output_file>\n\n", program_name);
    fprintf(stderr, "Supported formats:\n");
    fprintf(stderr, "  Input:  .exr (OpenEXR), .rgb9e5 (RGB9E5 HDR)\n");
    fprintf(stderr, "  Output: .rgb9e5 (RGB9E5 HDR), .exr (OpenEXR)\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s input.exr output.rgb9e5     # Convert EXR to RGB9E5\n", program_name);
    fprintf(stderr, "  %s input.rgb9e5 output.exr    # Convert RGB9E5 to EXR\n", program_name);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        PrintUsage(argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];

    // Determine conversion direction based on file extensions
    const char *input_ext = strrchr(input_file, '.');
    const char *output_ext = strrchr(output_file, '.');

    if (!input_ext || !output_ext) {
        fprintf(stderr, "Error: Could not determine file formats from extensions\n");
        return 1;
    }

    float *data = NULL;
    int width, height;
    int success = 0;

    // Load input file
    if (strcmp(input_ext, ".rgb9e5") == 0) {
        success = LoadRGB9E5(input_file, &data, &width, &height);
    } else if (strcmp(input_ext, ".exr") == 0) {
        success = LoadEXR(input_file, &data, &width, &height);
    } else {
        fprintf(stderr, "Error: Unsupported input format '%s'\n", input_ext);
        return 1;
    }

    if (!success) {
        return 1;
    }

    printf("Loaded %s: %dx%d pixels\n", input_file, width, height);

    // Save output file
    if (strcmp(output_ext, ".rgb9e5") == 0) {
        success = SaveRGB9E5(output_file, data, width, height);
    } else if (strcmp(output_ext, ".exr") == 0) {
        success = SaveEXR(output_file, data, width, height);
    } else {
        fprintf(stderr, "Error: Unsupported output format '%s'\n", output_ext);
        free(data);
        return 1;
    }

    free(data);

    if (success) {
        printf("Successfully converted to %s\n", output_file);
        return 0;
    } else {
        fprintf(stderr, "Error: Conversion failed\n");
        return 1;
    }
}