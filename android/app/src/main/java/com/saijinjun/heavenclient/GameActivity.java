package com.saijinjun.heavenclient;

import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.view.Window;

import org.libsdl.app.SDLActivity;

/** Starts SDL, which loads libmain.so and runs the game's main(). */
public class GameActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // The game surface fills the whole screen, but with gesture
        // navigation the window still paints an opaque bar background over
        // the gesture strip at the bottom, even while the bars are hidden.
        // That strip covered the status bar (HP, MP, EXP and its buttons).
        // Make the system bar backgrounds see-through so the game shows.
        Window window = getWindow();
        window.setNavigationBarColor(Color.TRANSPARENT);
        window.setStatusBarColor(Color.TRANSPARENT);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.setNavigationBarContrastEnforced(false);
            window.setStatusBarContrastEnforced(false);
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
