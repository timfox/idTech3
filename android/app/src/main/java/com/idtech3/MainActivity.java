package com.idtech3;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Build;
import android.widget.TextView;
import android.view.Choreographer;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("native-lib");
    }
    public native void engineInit();
    public native void engineLoadMod(String modPath);
    public native String stringFromJNI();
    public native void engineRender();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        engineInit();
        // Example: load a default mods directory (can be overridden at runtime)
        engineLoadMod("/sdcard/idtech3/mods");
        // Lightweight render loop placeholder (invokes native render hook)
        if (Build.VERSION.SDK_INT >= 16) {
            final Choreographer choreographer = Choreographer.getInstance();
            choreographer.postFrameCallback(new Choreographer.FrameCallback() {
                @Override
                public void doFrame(long frameTimeNanos) {
                    engineRender();
                    choreographer.postFrameCallback(this);
                }
            });
        } else {
            final Handler renderHandler = new Handler(Looper.getMainLooper());
        final TextView textView = new TextView(this);
        textView.setText(stringFromJNI());
        setContentView(textView);
            final Runnable renderLoop = new Runnable() {
                @Override
                public void run() {
                    engineRender();
                    renderHandler.postDelayed(this, 16);
                }
            };
            renderHandler.post(renderLoop);
        }
    }
}
