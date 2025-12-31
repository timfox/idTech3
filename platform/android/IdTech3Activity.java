package com.idtech3.engine;

import android.app.NativeActivity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.DisplayMetrics;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.Toast;

/**
 * Android entry point that exposes a {@link SurfaceView} for the native Vulkan renderer.
 * The class still extends {@link NativeActivity} so that android_native_app_glue can drive
 * lifecycle events, but it now forwards Surface callbacks to JNI so the engine can create
 * or destroy the Vulkan swapchain as needed.
 */
public class IdTech3Activity extends NativeActivity implements SurfaceHolder.Callback {
    private SurfaceView renderSurface;
    private SurfaceHolder surfaceHolder;
    private Vibrator vibrator;
    private InputMethodManager inputMethodManager;

    static {
        System.loadLibrary("quake3e");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        inputMethodManager = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

        renderSurface = new SurfaceView(this);
        surfaceHolder = renderSurface.getHolder();
        surfaceHolder.addCallback(this);
        setContentView(renderSurface);

        Toast.makeText(this, "Starting idTech3 Vulkan renderer...", Toast.LENGTH_SHORT).show();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        surfaceHolder.removeCallback(this);
    }

    // Surface callbacks -> JNI bridge
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Surface surface = holder.getSurface();
        if (surface != null) {
            nativeSurfaceCreated(surface);
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Surface surface = holder.getSurface();
        if (surface != null) {
            nativeSurfaceChanged(surface, format, width, height);
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeSurfaceDestroyed();
    }

    // UI helpers accessible from native code
    public void showKeyboard() {
        runOnUiThread(() -> {
            if (inputMethodManager != null) {
                inputMethodManager.showSoftInput(renderSurface, 0);
            }
        });
    }

    public void hideKeyboard() {
        runOnUiThread(() -> {
            if (inputMethodManager != null) {
                inputMethodManager.hideSoftInputFromWindow(renderSurface.getWindowToken(), 0);
            }
        });
    }

    public void vibrate(int milliseconds) {
        if (vibrator != null && vibrator.hasVibrator()) {
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                vibrator.vibrate(VibrationEffect.createOneShot(milliseconds, VibrationEffect.DEFAULT_AMPLITUDE));
            } else {
                vibrator.vibrate(milliseconds);
            }
        }
    }

    public DisplayMetrics getDisplayMetrics() {
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
        return metrics;
    }

    // Native hooks for surface lifecycle
    private static native void nativeSurfaceCreated(Surface surface);
    private static native void nativeSurfaceChanged(Surface surface, int format, int width, int height);
    private static native void nativeSurfaceDestroyed();
}
