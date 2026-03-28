#ifndef VM_NATIVE_LOADER_H
#define VM_NATIVE_LOADER_H

#include "q_shared.h"
#include "q_platform.h"

#define VM_NATIVE_MODULE_CANDIDATE_COUNT 3

/*
=================
VM_BuildNativeModuleCandidate

Build one native module filename candidate for VM loading.
candidateIndex order is intentionally stable:
  0 -> module.so
  1 -> module.arch.so
  2 -> modulearch.so
Returns qtrue on success, qfalse when candidateIndex is out of range or
input/output buffers are invalid.
=================
*/
qboolean VM_BuildNativeModuleCandidate( const char *moduleName, int candidateIndex, char *filename, int filenameSize );

#endif
