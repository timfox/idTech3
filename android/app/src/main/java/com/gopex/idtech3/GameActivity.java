/*
 * Copyright (C) 2026 Gopex LLC. All rights reserved.
 *
 * Android launcher activity for id Tech 3 engine.
 * Extends NativeActivity for direct native code execution.
 * Provides JNI bridge, lifecycle management, immersive mode, and touch HUD overlay.
 */

package com.gopex.idtech3;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.Toast;

import java.io.File;

public class GameActivity extends NativeActivity {
    private static final String TAG = "idTech3";

    static {
        System.loadLibrary("idtech3");
        nativeRegisterTouchOverlayJni();
    }

    private TouchHudView touchHud;
    private FrameLayout overlayRoot;

    private static native void nativeSetActivity(GameActivity activity);

    public static native void nativeSetDataPath(String path);
    public static native void nativeSetHomePath(String path);

    /** Register TouchOverlayBridge natives (FindClass from activity JNI context). */
    private static native void nativeRegisterTouchOverlayJni();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        nativeSetActivity(this);
        setupWindow();
        setupPaths();
        setupTouchOverlay();

        Log.i(TAG, "id Tech 3 engine starting");
        Log.i(TAG, "Device: " + Build.MANUFACTURER + " " + Build.MODEL);
        Log.i(TAG, "Android API: " + Build.VERSION.SDK_INT);
        Log.i(TAG, "ABI: " + Build.SUPPORTED_ABIS[0]);
    }

    private void setupTouchOverlay() {
        ViewGroup decor = (ViewGroup) getWindow().getDecorView();
        overlayRoot = new FrameLayout(this);
        overlayRoot.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        overlayRoot.setClickable(false);
        overlayRoot.setFocusable(false);

        touchHud = new TouchHudView(this);
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        touchHud.setLayoutParams(lp);
        /* Let touches fall through except where HUD handles them */
        touchHud.setClickable(false);

        overlayRoot.addView(touchHud);
        decor.addView(overlayRoot);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (touchHud != null && touchHud.onOverlayTouch(ev)) {
            return true;
        }
        return super.dispatchTouchEvent(ev);
    }

    @Override
    protected void onResume() {
        super.onResume();
        setupWindow();
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "Activity paused");
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            setupWindow();
        }
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
        Log.w(TAG, "Low memory warning");
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (level >= TRIM_MEMORY_RUNNING_LOW) {
            Log.w(TAG, "Trim memory level: " + level);
        }
    }

    private void setupWindow() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            | View.SYSTEM_UI_FLAG_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );
    }

    /**
     * Called from native when FS finds no pk3 (game thread). Shows a short on-screen hint
     * with the writable base path; process may exit shortly after.
     */
    public void showNoGameDataToast(final String basePath) {
        final String path = (basePath != null && !basePath.isEmpty()) ? basePath : "(unknown)";
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String msg = "No game data. Copy .pk3 files to:\n" + path + "/base\n"
                    + "(or add assets/apkassets/ in the APK - see apkassets/README.txt)";
                Toast.makeText(GameActivity.this, msg, Toast.LENGTH_LONG).show();
            }
        });
    }

    private void setupPaths() {
        File externalDir = getExternalFilesDir(null);
        if (externalDir != null) {
            externalDir.mkdirs();
            nativeSetDataPath(externalDir.getAbsolutePath());

            File baseDir = new File(externalDir, "base");
            baseDir.mkdirs();

            Log.i(TAG, "Data path: " + externalDir.getAbsolutePath());
        }

        File internalDir = getFilesDir();
        if (internalDir != null) {
            nativeSetHomePath(internalDir.getAbsolutePath());
            Log.i(TAG, "Home path: " + internalDir.getAbsolutePath());
        }
    }
}
