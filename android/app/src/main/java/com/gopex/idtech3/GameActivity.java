/*
 * Copyright (C) 2026 Gopex LLC. All rights reserved.
 *
 * Android launcher activity for id Tech 3 engine.
 * The actual engine runs via NativeActivity (android_main.c).
 * This class provides the JNI bridge for setting data paths.
 *
 * The engine can be launched either as:
 *   1. NativeActivity (declared in manifest, no Java UI)
 *   2. This GameActivity (Java UI + JNI bridge)
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
    }

    private void setupWindow() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

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
        }

        File internalDir = getFilesDir();
        if (internalDir != null) {
            nativeSetHomePath(internalDir.getAbsolutePath());
        }

        Log.i(TAG, "Data: " + (externalDir != null ? externalDir.getAbsolutePath() : "null"));
        Log.i(TAG, "Home: " + (internalDir != null ? internalDir.getAbsolutePath() : "null"));
    }

    @Override
    protected void onResume() {
        super.onResume();
        setupWindow();
    }
}
