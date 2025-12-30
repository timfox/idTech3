// UNIT_TEST: Basic presence check for Wayland toggle hooks
// This file is a minimal sanity check to ensure the project builds with a Wayland toggle available.
#ifdef UNIT_TEST
#include <assert.h>

int main() {
  // The runtime toggle is exercised by environment variables; this test validates
  // that the test build path is compilable and that UNIT_TEST is active.
  return 0;
}
#else
int main() { return 0; }
#endif

