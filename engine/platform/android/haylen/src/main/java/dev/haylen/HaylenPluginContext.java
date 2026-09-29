package dev.haylen;

import android.app.Activity;
import android.app.Application;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.util.ArrayList;
import java.util.List;
import org.json.JSONObject;

// What a plugin reaches the app through, one per plugin. Methods and events take the id of the plugin in front of their names, as the Lua handle of the plugin expects.
public final class HaylenPluginContext {
    private static final Handler mainThread = new Handler(Looper.getMainLooper());

    private final String id;
    private final Application application;
    private final JSONObject config;
    private final HaylenOverlay overlay;
    private final List<String> methods = new ArrayList<>();
    private int covers;

    HaylenPluginContext(String id, Application application, JSONObject config) {
        this.id = id;
        this.application = application;
        this.config = config;
        overlay = new HaylenOverlay(id);
    }

    public String id() {
        return id;
    }

    public Application application() {
        return application;
    }

    // The running activity, or null while there is none.
    public Activity activity() {
        return HaylenBridge.activity();
    }

    // The parameter values of the plugin in the app.json of the package, over the defaults of its plugin.json.
    public JSONObject config() {
        return config;
    }

    // Answers <id>.<method> on the main thread.
    public void register(String method, HaylenBridge.MethodHandler handler) {
        register(method, handler, HaylenBridge.Threading.MAIN);
    }

    public void register(String method, HaylenBridge.MethodHandler handler, HaylenBridge.Threading threading) {
        String name = id + "." + method;
        HaylenBridge.register(name, handler, threading);
        synchronized (methods) {
            methods.add(name);
        }
    }

    // Sends <id>.<event> to the app.
    public void emit(String event, Object payload) {
        HaylenBridge.emit(id + "." + event, payload, false);
    }

    // Sends <id>.<event> retained, so it waits for the first listener of its name, such as the deep link that opened the app.
    public void emitRetained(String event, Object payload) {
        HaylenBridge.emit(id + "." + event, payload, true);
    }

    // Places native views over the app. It works on the main thread while an activity exists.
    public HaylenOverlay overlay() {
        return overlay;
    }

    // Tells the app that native UI of the plugin covers it, such as a full screen ad, which halts and mutes the app until the matching uncoverApp. Covers nest, and the ones still open when the activity is destroyed end with it.
    public synchronized void coverApp() {
        if (HaylenBridge.activity() == null) {
            Log.w("haylen", "The plugin " + id + " covered the app while no activity exists, which covers nothing.");
            return;
        }
        ++covers;
        nativeCoverApp();
    }

    public synchronized void uncoverApp() {
        if (covers == 0) {
            Log.e("haylen", "The plugin " + id + " uncovered the app without covering it first.");
            return;
        }
        --covers;
        nativeUncoverApp();
    }

    // Runs the task at once on the main thread, and posts it there from any other thread.
    public void runOnMainThread(Runnable task) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            task.run();
        } else {
            mainThread.post(task);
        }
    }

    synchronized void endCovers() {
        for (; covers > 0; --covers) {
            nativeUncoverApp();
        }
    }

    // A plugin that failed to load answers nothing, so the methods it registered before failing go away with it.
    void unregisterAll() {
        synchronized (methods) {
            for (String name : methods) {
                HaylenBridge.unregister(name);
            }
            methods.clear();
        }
    }

    private static native void nativeCoverApp();

    private static native void nativeUncoverApp();
}
