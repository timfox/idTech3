/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
Image loader prototypes for all custom formats.
===========================================================================
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../../qcommon/q_shared.h"

void R_LoadEXR(const char *filename, byte **pic, int *width, int *height);
void R_LoadHDR(const char *filename, byte **pic, int *width, int *height);
void R_LoadQOI(const char *filename, byte **pic, int *width, int *height);
void R_LoadDDS(const char *filename, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif
