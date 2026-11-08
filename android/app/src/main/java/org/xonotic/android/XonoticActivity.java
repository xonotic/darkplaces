package org.xonotic.android;

import android.os.Bundle;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/** Runs the DarkPlaces engine (libmain.so) in Xonotic mode via SDL2. */
public class XonoticActivity extends SDLActivity {
    private TouchControlsView touch;

    @Override
    protected void onDestroy() {
        if (touch != null) touch.shutdown();
        super.onDestroy();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
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
        String base = LauncherActivity.baseDir(this).getAbsolutePath();
        return new String[] {
            "-xonotic",
            "-basedir", base,
            "-nohome",
            "+exec", "android.cfg",
        };
    }

    /** Copies assets/android.cfg (controller binds, mobile defaults) into data/. */
    private void installAndroidConfig() {
        File dst = new File(LauncherActivity.dataDir(this), "android.cfg");
        try (InputStream in = getAssets().open("android.cfg");
             OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[8192];
            int r;
            while ((r = in.read(buf)) > 0) out.write(buf, 0, r);
        } catch (Exception ignored) {
        }
    }
}
