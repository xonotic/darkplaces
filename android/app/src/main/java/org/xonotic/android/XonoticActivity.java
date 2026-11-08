package org.xonotic.android;

import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/** Runs the DarkPlaces engine (libmain.so) in Xonotic mode via SDL2. */
public class XonoticActivity extends SDLActivity {
    private static final String TAG = "XonoticPaths";
    private TouchControlsView touch;
    private File gameBase;

    private File resolveGameBase() {
        if (gameBase != null) return gameBase;
        File base = LauncherActivity.baseDir(this);
        try {
            base = base.getCanonicalFile();
        } catch (IOException e) {
            Log.w(TAG, "Cannot canonicalize game base; using absolute path", e);
            base = base.getAbsoluteFile();
        }
        File data = new File(base, "data");
        if (!data.isDirectory() && !data.mkdirs()) {
            throw new IllegalStateException("Cannot create game data directory: " + data);
        }
        if (!data.canRead()) {
            throw new IllegalStateException("Game data directory is not readable: " + data);
        }
        gameBase = base;
        Log.i(TAG, "Game base directory: " + base.getAbsolutePath());
        Log.i(TAG, "Game data directory: " + data.getAbsolutePath());
        File[] files = data.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.getName().endsWith(".pk3")) {
                    Log.i(TAG, "PK3: " + file.getName() + " bytes=" + file.length()
                            + " readable=" + file.canRead());
                }
            }
        }
        return gameBase;
    }

    @Override
    protected void onDestroy() {
        if (touch != null) touch.shutdown();
        super.onDestroy();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        resolveGameBase();
        installAndroidConfig();
        super.onCreate(savedInstanceState);
        if (mLayout != null) {
            touch = new TouchControlsView(this);
            mLayout.addView(touch, new android.view.ViewGroup.LayoutParams(
                    android.view.ViewGroup.LayoutParams.MATCH_PARENT,
                    android.view.ViewGroup.LayoutParams.MATCH_PARENT));
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected String[] getArguments() {
        String base = resolveGameBase().getAbsolutePath();
        return new String[] {
            "-xonotic",
            "-basedir", base,
            "-nohome",
            "+exec", "android.cfg",
        };
    }

    /** Copies assets/android.cfg (controller binds, mobile defaults) into data/. */
    private void installAndroidConfig() {
        File dst = new File(new File(resolveGameBase(), "data"), "android.cfg");
        try (InputStream in = getAssets().open("android.cfg");
             OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[8192];
            int r;
            while ((r = in.read(buf)) > 0) out.write(buf, 0, r);
        } catch (IOException e) {
            Log.e(TAG, "Cannot install Android config: " + dst, e);
        }
    }
}
