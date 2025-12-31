package com.idtech3;

import android.os.Bundle;
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
        // Replace content view with Android OpenGL surface-based renderer
        android.opengl.GLSurfaceView glView = new android.opengl.GLSurfaceView(this);
        glView.setEGLContextClientVersion(2);
        glView.setRenderer(new EngineSurfaceRenderer());
        setContentView(glView);
    }
}
