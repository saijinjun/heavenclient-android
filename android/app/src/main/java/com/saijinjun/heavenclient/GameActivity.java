package com.saijinjun.heavenclient;

import org.libsdl.app.SDLActivity;

/** Starts SDL, which loads libmain.so and runs the game's main(). */
public class GameActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
