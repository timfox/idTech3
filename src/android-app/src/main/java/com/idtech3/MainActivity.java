package com.idtech3;

import android.os.Bundle;
import android.opengl.GLSurfaceView;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("native-lib");
    }
    private native void engineInit();
    private native void engineRender();
    private native void engineLoadMod(String modPath);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Initialize engine and set up a GL surface for rendering
        engineInit();
        engineLoadMod("/sdcard/idtech3/mods");
        GLSurfaceView glView = new GLSurfaceView(this);
        glView.setEGLContextClientVersion(2);
        glView.setRenderer(new com.idtech3.EngineSurfaceRenderer());
        setContentView(glView);
    }
}

