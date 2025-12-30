// UNIT_TEST: Wayland toggle test (very small stub to satisfy CI path)
// This ensures the CI path compiles when requested with UNIT_TEST.
#ifdef UNIT_TEST
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <stdlib.h>
#define UNSET_WAYLAND_FORCE _putenv_s("WAYLAND_FORCE", "")
#define SET_WAYLAND_FORCE(x) _putenv_s("WAYLAND_FORCE", x)
#else
#define UNSET_WAYLAND_FORCE unsetenv("WAYLAND_FORCE")
#define SET_WAYLAND_FORCE(x) setenv("WAYLAND_FORCE", x, 1)
#endif

extern "C" int Wayland_Toggle_IsWaylandForced();
int main() {
  // ensure not forced
  #if defined(_WIN32)
  UNSET_WAYLAND_FORCE;
  #else
  UNSET_WAYLAND_FORCE;
  #endif
  int before = Wayland_Toggle_IsWaylandForced();
  (void)before;
  // force Wayland
  #if defined(_WIN32)
  SET_WAYLAND_FORCE("1");
  #else
  SET_WAYLAND_FORCE("1");
  #endif
  int after = Wayland_Toggle_IsWaylandForced();
  if (after != 1) {
    return 1;
  }
  // reset
  #if defined(_WIN32)
  SET_WAYLAND_FORCE("0");
  #else
  SET_WAYLAND_FORCE("0");
  #endif
  return 0;
}
#else
int main() { return 0; }
#endif

