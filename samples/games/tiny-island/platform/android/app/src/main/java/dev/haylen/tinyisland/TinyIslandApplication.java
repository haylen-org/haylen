package dev.haylen.tinyisland;

import android.app.Application;
import dev.haylen.app.R;

// Registers the game's own platform plugins before the first activity starts.
public final class TinyIslandApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        GoogleSignInPlugin.register(getString(R.string.google_server_client_id));
    }
}
