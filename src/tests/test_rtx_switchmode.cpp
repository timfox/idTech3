// Unit tests for RTX_SwitchMode and resource cleanup flow
// This tests the mode switch path and ensures test hooks reflect state.
#include <assert.h>
#include <stdio.h>

#ifdef UNIT_TEST
// Test hooks exposed by vk_rtx_main.cpp
extern "C" int RTX_GetModeForTest();
extern "C" int RTX_IsInitializedForTest();
extern "C" void RTX_TestForceResourceCleanupForTest();
extern "C" void RTX_SwitchMode(int newMode);

int main() {
    // Ensure we can switch modes and observe state
    RTX_SwitchMode(1);
    int mode1 = RTX_GetModeForTest();
    int init1 = RTX_IsInitializedForTest();
    printf("RTX switch to 1 -> mode=%d, initialized=%d\\n", mode1, init1);
    assert(mode1 == 1);
    // After switch, resources should have been cleaned and initialization deferred
    assert(init1 == 0);

    RTX_SwitchMode(0);
    int mode0 = RTX_GetModeForTest();
    int init0 = RTX_IsInitializedForTest();
    printf("RTX switch to 0 -> mode=%d, initialized=%d\\n", mode0, init0);
    assert(mode0 == 0);
    assert(init0 == 0);

    // Force a cleanup path explicitly (idempotent)
    RTX_TestForceResourceCleanupForTest();
    int modeAfterCleanup = RTX_GetModeForTest();
    printf("RTX cleanup forced; mode=%d\\n", modeAfterCleanup);
    // State should remain valid
    assert(modeAfterCleanup == mode0);
    return 0;
}
#else
int main() { return 0; }
#endif

