/*
===============================================================================

RGB9E5 HDR image loader for idtech3

This implements loading of RGB9E5 (.rgb9e5) image files for HDR support.

===============================================================================
*/

#include "../common/q_shared.h"
#include "../renderercommon/tr_public.h"

extern refimport_t ri;

/*
================================================================================

RGB9E5 format specification:

RGB9E5 is a shared exponent format that stores 3 RGB values and 1 shared
exponent in 32 bits total:

Bits:  31  30  29  28  27  26  25  24  23  22  21  20  19  18  17  16
       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
       |E4 |E3 |E2 |E1 |E0 |R8 |R7 |R6 |R5 |R4 |R3 |R2 |R1 |R0 |G8 |G7 |
       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

Bits:  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
       |G6 |G5 |G4 |G3 |G2 |G1 |G0 |B8 |B7 |B6 |B5 |B4 |B3 |B2 |B1 |B0 |
       +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

Where:
- R[8:0] = 9-bit red mantissa
- G[8:0] = 9-bit green mantissa
- B[8:0] = 9-bit blue mantissa
- E[4:0] = 5-bit shared exponent

The value is calculated as: (mantissa / 511.0) * (2^(exponent - 15))

================================================================================
*/

typedef struct {
    unsigned int data;  // 32-bit RGB9E5 value
} rgb9e5_pixel_t;

typedef struct {
    char magic[4];      // "RGB9"
    int width;
    int height;
    int flags;          // Reserved for future use
} rgb9e5_header_t;

// Convert RGB9E5 to float RGB
static void RGB9E5ToFloat3(unsigned int rgb9e5, float *r, float *g, float *b) {
    // Extract components
    int exponent = (rgb9e5 >> 27) & 0x1F;  // Bits 27-31
    int red_mantissa = (rgb9e5 >> 18) & 0x1FF;    // Bits 18-26
    int green_mantissa = (rgb9e5 >> 9) & 0x1FF;   // Bits 9-17
    int blue_mantissa = rgb9e5 & 0x1FF;           // Bits 0-8

    // Convert to float
    float exp_factor = (float)pow(2.0, exponent - 15.0);

    *r = (float)red_mantissa / 511.0f * exp_factor;
    *g = (float)green_mantissa / 511.0f * exp_factor;
    *b = (float)blue_mantissa / 511.0f * exp_factor;
}

// Convert float RGB to RGB9E5
static unsigned int Float3ToRGB9E5(float r, float g, float b) {
    // Find the maximum component
    float max_val = MAX(MAX(r, g), b);

    if (max_val == 0.0f) {
        return 0; // All zeros
    }

    // Calculate exponent
    int exponent = (int)ceilf(log2f(max_val)) + 15;
    exponent = Com_Clamp(0, 31, exponent);

    // Calculate mantissas
    float scale = (float)pow(2.0, 15 - exponent);
    int red_mantissa = (int)roundf(r * scale * 511.0f);
    int green_mantissa = (int)roundf(g * scale * 511.0f);
    int blue_mantissa = (int)roundf(b * scale * 511.0f);

    // Clamp mantissas
    red_mantissa = Com_Clamp(0, 511, red_mantissa);
    green_mantissa = Com_Clamp(0, 511, green_mantissa);
    blue_mantissa = Com_Clamp(0, 511, blue_mantissa);

    // Pack into RGB9E5 format
    unsigned int result = 0;
    result |= (exponent & 0x1F) << 27;
    result |= (red_mantissa & 0x1FF) << 18;
    result |= (green_mantissa & 0x1FF) << 9;
    result |= (blue_mantissa & 0x1FF);

    return result;
}

/*
==============
R_LoadRGB9E5

Loads RGB9E5 HDR image files
==============
*/
void R_LoadRGB9E5(const char *name, unsigned char **pic, int *width, int *height) {
    rgb9e5_header_t header;
    rgb9e5_pixel_t *rgb9e5_data = NULL;
    float *float_data = NULL;
    int fileLen;
    byte *buf = NULL;

    *pic = NULL;

    // Load the file
    fileLen = ri.FS_ReadFile((char *)name, (void **)&buf);
    if (!buf || fileLen <= 0) {
        return;
    }

    if (fileLen < (int)sizeof(rgb9e5_header_t)) {
        ri.Printf(PRINT_WARNING, "R_LoadRGB9E5: File '%s' is too small to be a valid RGB9E5 file\n", name);
        ri.FS_FreeFile(buf);
        return;
    }

    // Read header
    memcpy(&header, buf, sizeof(rgb9e5_header_t));

    // Validate magic
    if (memcmp(header.magic, "RGB9", 4) != 0) {
        ri.Printf(PRINT_WARNING, "R_LoadRGB9E5: File '%s' is not a valid RGB9E5 file (bad magic)\n", name);
        ri.FS_FreeFile(buf);
        return;
    }

    // Validate dimensions
    if (header.width <= 0 || header.height <= 0 ||
        header.width > 4096 || header.height > 4096) {
        ri.Printf(PRINT_WARNING, "R_LoadRGB9E5: File '%s' has invalid dimensions %dx%d\n",
                 name, header.width, header.height);
        ri.FS_FreeFile(buf);
        return;
    }

    // Check file size
    int expected_size = sizeof(rgb9e5_header_t) + header.width * header.height * sizeof(rgb9e5_pixel_t);
    if (fileLen != expected_size) {
        ri.Printf(PRINT_WARNING, "R_LoadRGB9E5: File '%s' has incorrect size (expected %d, got %d)\n",
                 name, expected_size, fileLen);
        ri.FS_FreeFile(buf);
        return;
    }

    // Allocate output buffer (RGBA float format for the renderer)
    int pixel_count = header.width * header.height;
    *width = header.width;
    *height = header.height;

    // Allocate float buffer (4 floats per pixel: RGBA)
    float_data = (float *)ri.Malloc(pixel_count * 4 * sizeof(float));
    if (!float_data) {
        ri.Printf(PRINT_WARNING, "R_LoadRGB9E5: Failed to allocate memory for '%s'\n", name);
        ri.FS_FreeFile(buf);
        return;
    }

    // Get RGB9E5 data pointer
    rgb9e5_data = (rgb9e5_pixel_t *)(buf + sizeof(rgb9e5_header_t));

    // Convert RGB9E5 to float RGBA
    float *dst = float_data;
    for (int i = 0; i < pixel_count; ++i) {
        float r, g, b;
        RGB9E5ToFloat3(rgb9e5_data[i].data, &r, &g, &b);

        *dst++ = r;  // R
        *dst++ = g;  // G
        *dst++ = b;  // B
        *dst++ = 1.0f; // A (fully opaque for HDR)
    }

    // Return the float data
    *pic = (unsigned char *)float_data;

    ri.FS_FreeFile(buf);

    ri.Printf(PRINT_DEVELOPER, "R_LoadRGB9E5: Loaded HDR image '%s' (%dx%d)\n",
             name, header.width, header.height);
}

/*
==============
R_SaveRGB9E5

Saves an image as RGB9E5 HDR format
==============
*/
qboolean R_SaveRGB9E5(const char *name, const float *float_data, int width, int height) {
    if (!float_data || width <= 0 || height <= 0) {
        return qfalse;
    }

    rgb9e5_header_t header;
    rgb9e5_pixel_t *rgb9e5_data;
    int pixel_count = width * height;
    int file_size = sizeof(rgb9e5_header_t) + pixel_count * sizeof(rgb9e5_pixel_t);
    byte *buf = (byte *)ri.Malloc(file_size);

    if (!buf) {
        return qfalse;
    }

    // Write header
    memcpy(header.magic, "RGB9", 4);
    header.width = width;
    header.height = height;
    header.flags = 0; // Reserved

    memcpy(buf, &header, sizeof(rgb9e5_header_t));

    // Convert float RGBA to RGB9E5
    rgb9e5_data = (rgb9e5_pixel_t *)(buf + sizeof(rgb9e5_header_t));
    const float *src = float_data;

    for (int i = 0; i < pixel_count; ++i) {
        float r = *src++;
        float g = *src++;
        float b = *src++;
        // Skip alpha

        rgb9e5_data[i].data = Float3ToRGB9E5(r, g, b);
    }

    // Write file
    ri.FS_WriteFile(name, buf, file_size);
    ri.Free(buf);

    ri.Printf(PRINT_DEVELOPER, "R_SaveRGB9E5: Saved HDR image '%s' (%dx%d)\n",
             name, width, height);

    return qtrue;
}