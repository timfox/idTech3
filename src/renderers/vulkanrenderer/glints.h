#ifndef TR_GLINTS_H
#define TR_GLINTS_H

#include <stddef.h>

#define GLINT_DICT_ENTRIES 192
#define GLINT_DICT_LEVELS 16
#define GLINT_DICT_SIZE 64

void R_Glints_InitDictionary(void);
void R_Glints_ShutdownDictionary(void);
float *R_Glints_GetPackedDictionary(size_t *outSize);

#endif // TR_GLINTS_H
