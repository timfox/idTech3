package com.idtech3;

import android.opengl.GLSurfaceView;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class EngineSurfaceRenderer implements GLSurfaceView.Renderer {
    static {
        System.loadLibrary("native-lib");
    }
    private native void engineInit();
    private native void engineRender();
    private native void engineConfigureSurface(int width, int height);

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        engineInit();
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        engineConfigureSurface(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        engineRender();
    }
}

