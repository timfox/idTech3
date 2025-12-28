package com.idtech3;

import android.os.Bundle;
import android.opengl.GLSurfaceView;
import androidx.appcompat.app.AppCompatActivity;
import android.content.res.AssetManager;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("native-lib");
    }
    private native void engineInit();
    private native void engineRender();
    private native void engineLoadMod(String modPath);
    private native void engineLoadModFromBytes(byte[] data, String modName);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Initialize engine and set up a GL surface for rendering
        engineInit();
        engineLoadMod("/sdcard/idtech3/mods");
        // Load a test mod from APK assets (mods/testmod.txt)
        try {
            AssetManager am = getAssets();
            InputStream is = am.open("mods/testmod.txt");
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int read;
            while ((read = is.read(buf)) != -1) {
                bos.write(buf, 0, read);
            }
            is.close();
            byte[] modBytes = bos.toByteArray();
            engineLoadModFromBytes(modBytes, "testmod.txt");
        } catch (IOException e) {
            // Ignore if asset not present
        }
        // Optional: load all mods from APK assets
        loadAllModsFromAssets();
        GLSurfaceView glView = new GLSurfaceView(this);
        glView.setEGLContextClientVersion(2);
        glView.setRenderer(new com.idtech3.EngineSurfaceRenderer());
        setContentView(glView);
    }

    // Optional: enumerate mods in assets/mods
    private String[] listModsInAssets() {
        try {
            AssetManager am = getAssets();
            return am.list("mods");
        } catch (IOException e) {
            return new String[0];
        }
    }

    // Optional: load all mods from APK assets (mods/ directory)
    private void loadAllModsFromAssets() {
        String[] modNames = listModsInAssets();
        if (modNames == null) return;
        try {
            AssetManager am = getAssets();
            for (String modName : modNames) {
                String assetPath = "mods/" + modName;
                InputStream is = am.open(assetPath);
                ByteArrayOutputStream bos = new ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                int read;
                while ((read = is.read(buf)) != -1) {
                    bos.write(buf, 0, read);
                }
                is.close();
                byte[] modBytes = bos.toByteArray();
                engineLoadModFromBytes(modBytes, modName);
            }
        } catch (IOException ignore) {
            // If assets/mods is missing or unreadable, ignore
        }
    }
}

