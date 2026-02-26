/*
 * Copyright (C) 2026 Gopex LLC. All rights reserved.
 *
 * Android launcher activity for id Tech 3 engine.
 * Extends NativeActivity for direct native code execution.
 * Provides JNI bridge, lifecycle management, and immersive mode.
 */

package com.gopex.idtech3;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import java.io.File;

public class GameActivity extends NativeActivity {
    private static final String TAG = "idTech3";

    static {
        System.loadLibrary("idtech3");
    }

    public static native void nativeSetDataPath(String path);
    public static native void nativeSetHomePath(String path);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setupWindow();
        setupPaths();

        Log.i(TAG, "id Tech 3 engine starting");
        Log.i(TAG, "Device: " + Build.MANUFACTURER + " " + Build.MODEL);
        Log.i(TAG, "Android API: " + Build.VERSION.SDK_INT);
        Log.i(TAG, "ABI: " + Build.SUPPORTED_ABIS[0]);
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
