package org.xonotic.game.xonotic;

public class SDLActivity extends org.libsdl.app.SDLActivity {
    /**
     * This method is called by SDL before loading the native shared libraries.
     * It can be overridden to provide names of shared libraries to be loaded.
     * The default implementation returns the defaults. It never returns null.
     * An array returned by a new implementation must at least contain "SDL2".
     * Also keep in mind that the order the libraries are loaded may matter.
     * @return names of shared libraries to be loaded (e.g. "SDL2", "main").
     */
    protected String[] getLibraries() {
        return new String[] {
            "ogg",
            "vorbis",
            "SDL2",
            "main"
        };
    }

    /**
     * This method is called by SDL before starting the native application thread.
     * It can be overridden to provide the arguments after the application name.
     * The default implementation returns an empty array. It never returns null.
     * @return arguments for the native application.
     */
    protected String[] getArguments() {
        return new String[]{
            "+vid_touchscreen 1",
            "+vid_touchscreen_outlinealpha 1",
            "-basedir", "darkplaces/",
            "-userdir", "/sdcard/darkplaces/",
            //"-game", "xonotic",
            //"-basedir", "xonotic/",
            //"-userdir", "/sdcard/xonotic/",
        };
    }
}
