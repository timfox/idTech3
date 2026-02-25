/*
 * Copyright (C) 2026 Gopex LLC. All rights reserved.
 *
 * This file is original work by Gopex LLC and is not derived from
 * existing id Tech 3 / ioquake3 code.
 * The engine framework is based on id Tech 3 (GPLv2).
 *
 * Main Android activity for the id Tech 3 engine.
 * Manages SDL2 lifecycle, Vulkan surface creation, touch input,
 * and game data directory setup.
 */

package com.gopex.idtech3;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import java.io.File;

public class GameActivity extends Activity {
    private static final String TAG = "idTech3";

    static {
        System.loadLibrary("SDL2");
        System.loadLibrary("idtech3");
    }

    private static native int nativeInit(String[] args);
    private static native void nativeSetDataPath(String path);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setupWindow();
        setupGameData();

        Log.i(TAG, "id Tech 3 engine starting on Android");
        Log.i(TAG, "Device: " + Build.MANUFACTURER + " " + Build.MODEL);
        Log.i(TAG, "Android API: " + Build.VERSION.SDK_INT);

        new Thread(() -> {
            String dataPath = getGameDataPath();
            nativeSetDataPath(dataPath);
            nativeInit(new String[]{ "+set", "r_renderer", "vulkan" });
        }).start();
    }

    private void setupWindow() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);

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

    private void setupGameData() {
        File gameDir = new File(getGameDataPath());
        if (!gameDir.exists()) {
            boolean created = gameDir.mkdirs();
            Log.i(TAG, "Game data directory " + (created ? "created" : "exists") + ": " + gameDir.getAbsolutePath());
        }

        File baseDir = new File(gameDir, "base");
        if (!baseDir.exists()) {
            baseDir.mkdirs();
        }
    }

    private String getGameDataPath() {
        File externalDir = getExternalFilesDir(null);
        if (externalDir != null) {
            return externalDir.getAbsolutePath();
        }
        return Environment.getExternalStorageDirectory().getAbsolutePath() + "/idtech3";
    }

    @Override
    protected void onResume() {
        super.onResume();
        setupWindow();
    }
}
