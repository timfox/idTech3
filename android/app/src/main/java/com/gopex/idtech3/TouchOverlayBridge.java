/*
 * Copyright (C) 2026 Gopex LLC. All rights reserved.
 *
 * JNI bridge: Java touch HUD -> native Sys_QueEvent / Cbuf (game thread pumps queue).
 */

package com.gopex.idtech3;

public final class TouchOverlayBridge {
    /* libidtech3 is loaded by GameActivity before overlay is shown */

    private TouchOverlayBridge() {}

    public static native void nativeKey(int key, boolean down);

    public static native void nativeMouseDelta(int dx, int dy);

    /** One line of console / bind command, e.g. "+attack" or "weapnext" */
    public static native void nativeCommand(String cmd);
}
