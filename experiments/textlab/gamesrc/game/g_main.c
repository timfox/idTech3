#include "q_shared.h"
#include "g_public.h"

// Shared syscall entry point
intptr_t (QDECL *game_syscall)(intptr_t arg, ...) = NULL;

static inline void G_Printf(const char *msg) {
	if (game_syscall && msg) {
		game_syscall(G_PRINT, msg);
	}
}

Q_EXPORT void dllEntry(intptr_t (QDECL *syscallptr)(intptr_t arg, ...)) {
	game_syscall = syscallptr;
	G_Printf("textlab game bound\n");
}

Q_EXPORT intptr_t vmMain(int command, int arg0, int arg1, int arg2,
                         int arg3, int arg4, int arg5, int arg6,
                         int arg7, int arg8, int arg9, int arg10,
                         int arg11) {
	(void)arg0; (void)arg1; (void)arg2; (void)arg3;
	(void)arg4; (void)arg5; (void)arg6; (void)arg7;
	(void)arg8; (void)arg9; (void)arg10; (void)arg11;

	switch (command) {
	case GAME_INIT:
		G_Printf("textlab: game init\n");
		return 0;
	case GAME_SHUTDOWN:
		G_Printf("textlab: game shutdown\n");
		return 0;
	default:
		return 0;
	}
}
