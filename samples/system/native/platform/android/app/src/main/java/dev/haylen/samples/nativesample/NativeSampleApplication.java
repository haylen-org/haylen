package dev.haylen.samples.nativesample;

import android.app.Application;
import dev.haylen.HaylenBridge;

// Registers the handlers of the sample before the first activity starts: suspending Kotlin handlers and a Java handler that throws, which fails its call instead of crashing the app.
public final class NativeSampleApplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        NativeSampleHandlers.register();
        HaylenBridge.register("native_sample.explode", (params, reply) -> {
            throw new IllegalStateException("Java threw on purpose.");
        });
    }
}
