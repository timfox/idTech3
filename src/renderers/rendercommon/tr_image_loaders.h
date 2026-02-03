#pragma once

#include "../../qcommon/q_shared.h" // for byte

void R_LoadBMP(const char *name, byte **pic, int *width, int *height);
void R_LoadEXR(const char *name, byte **pic, int *width, int *height);
void R_LoadJPG(const char *filename, byte **pic, int *width, int *height);
void R_LoadPCX(const char *filename, byte **pic, int *width, int *height);
void R_LoadPNG(const char *name, byte **pic, int *width, int *height);
void R_LoadTGA(const char *name, byte **pic, int *width, int *height);
