package com.classic.doom3;

/**
 * Thin JNI bridge into dhewm3 input queues.
 * Key codes match framework/KeyInput.h (dhewm3).
 */
public final class Doom3Native {
    private Doom3Native() {}

    public static final int K_ENTER      = 13;
    public static final int K_ESCAPE     = 27;
    public static final int K_SPACE      = 32;
    public static final int K_MOUSE1     = 187;
    public static final int K_MOUSE2     = 188;
    public static final int K_MWHEELDOWN = 195;
    public static final int K_MWHEELUP   = 196;

    public static native void touchKey(int key, boolean down);
    public static native void touchMouseDelta(int dx, int dy);
    public static native void touchMouseButton(int button /*1=LMB 2=RMB*/, boolean down);
}
