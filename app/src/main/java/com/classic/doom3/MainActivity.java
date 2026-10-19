package com.classic.doom3;

import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] {
                "SDL2",
                "doom3"
        };
    }

    @Override
    protected String getMainFunction() {
        return "SDL_main";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Overlay on top of SDL surface — pure Java, no NDK rebuild for UI tweaks
        final TouchControlsView touch = new TouchControlsView(this);
        final FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        );
        // post so SDL has already set its content view
        getWindow().getDecorView().post(() -> addContentView(touch, lp));
    }
}
