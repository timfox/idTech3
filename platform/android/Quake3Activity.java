package com.idtech3.engine;

import android.app.NativeActivity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.DisplayMetrics;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.Toast;

public class Quake3Activity extends NativeActivity {
    private Vibrator vibrator;
    private InputMethodManager inputMethodManager;

    static {
        // Load native libraries
        System.loadLibrary("quake3e");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Force landscape orientation
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        // Keep screen on
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Initialize system services
        vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        inputMethodManager = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

        // Show initialization message
        Toast.makeText(this, "Starting Quake 3 Engine...", Toast.LENGTH_SHORT).show();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    // Native method declarations
    public native void showKeyboard();
    public native void hideKeyboard();
    public native void vibrate(int milliseconds);
    public native DisplayMetrics getDisplayMetrics();

    // Java wrapper for native methods
    public void showKeyboard() {
        runOnUiThread(() -> {
            if (inputMethodManager != null) {
                inputMethodManager.showSoftInput(getWindow().getDecorView(), 0);
            }
        });
    }

    public void hideKeyboard() {
        runOnUiThread(() -> {
            if (inputMethodManager != null) {
                inputMethodManager.hideSoftInputFromWindow(getWindow().getDecorView().getWindowToken(), 0);
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
}
