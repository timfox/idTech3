#include "crash_verbose.h"
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

static struct sigaction g_old_sigsegv;

static void crash_signal_handler(int sig, siginfo_t* info, void* ucontext) {
    // Dump verbose crash context if available
    append_verbose_crash_context();
    // Re-raise the signal with the previous handler
    if (sig == SIGSEGV) {
        // Restore previous handler and exit with standard crash code
        sigaction(SIGSEGV, &g_old_sigsegv, NULL);
        _exit(128 + SIGSEGV);
    } else {
        _exit(1);
    }
}

static void install_crash_hook(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, &g_old_sigsegv) != 0) {
        // If we fail to install, fail silently
    }
}

__attribute__((constructor)) static void crash_hook_ctor() {
    install_crash_hook();
    // Also ensure verbose crash context is wired if available
    // Dump a verbose crash context at startup for easier debugging (best-effort)
    append_verbose_crash_context();
}
