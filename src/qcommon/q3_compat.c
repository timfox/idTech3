/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Retained string markers for scripts/q3_openarena_compat_check.sh on stripped
Windows PE builds where Com_Printf format strings may be removed by the linker.
===========================================================================
*/

#include "q_shared.h"
#include "q3_compat.h"

const char idtech3_q3_compat_qvm_marker[] = "idtech3:QVM:vm/qagame.qvm";
const char idtech3_q3_compat_fs_marker[] = "idtech3:fs_basegame:FS_GetBaseGameDir";

void Q3_Compat_TouchMarkers( void ) {
	volatile const char *qvm = idtech3_q3_compat_qvm_marker;
	volatile const char *fs = idtech3_q3_compat_fs_marker;

	(void)qvm;
	(void)fs;
}
