/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
Image loader prototypes for all formats (built-in and custom).
===========================================================================
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

/* Built-in formats */
void R_LoadBMP(const char *filename, byte **pic, int *width, int *height);
void R_LoadJPG(const char *filename, byte **pic, int *width, int *height);
void R_LoadPCX(const char *filename, byte **pic, int *width, int *height);
void R_LoadPNG(const char *filename, byte **pic, int *width, int *height);
void R_LoadTGA(const char *filename, byte **pic, int *width, int *height);

/* Custom formats (Gopex) */
void R_LoadEXR(const char *filename, byte **pic, int *width, int *height);
void R_LoadEXR_HDR(const char *filename, float **pic, int *width, int *height);
qboolean R_SaveEXR(const char *filename, const float *rgba, int width, int height);
void R_LoadHDR(const char *filename, byte **pic, int *width, int *height);
void R_LoadHDR_Float(const char *filename, float **pic, int *width, int *height);
void R_LoadQOI(const char *filename, byte **pic, int *width, int *height);
void R_LoadDDS(const char *filename, byte **pic, int *width, int *height);
void R_LoadSVG(const char *filename, byte **pic, int *width, int *height);

/* Compressed loaders: return raw BC7/block data. format = VkFormat, size = byte count. */
qboolean R_LoadDDS_Compressed(const char *filename, byte **data, int *width, int *height, int *format, int *size);
qboolean R_LoadKTX2(const char *filename, byte **data, int *width, int *height, int *format, int *size);

#ifdef __cplusplus
}
#endif
