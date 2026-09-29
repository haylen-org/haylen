package dev.haylen.samples.platform;

import android.app.Application;

// Registers the bridge methods of the sample before the first activity starts.
public final class PlatformSampleApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        SamplePlugin.register(this);
    }
}
