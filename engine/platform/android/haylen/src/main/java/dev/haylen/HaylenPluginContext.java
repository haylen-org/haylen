package dev.haylen;

import android.app.Application;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import androidx.activity.result.contract.ActivityResultContract;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.json.JSONObject;

// What a plugin reaches the app through, one per plugin. Methods and events take the id of the plugin in front of their names, as the Lua handle of the plugin expects.
public final class HaylenPluginContext {
    private static final Handler mainThread = new Handler(Looper.getMainLooper());

    private final String id;
    private final Application application;
    private final JSONObject config;
    private final HaylenOverlay overlay;
    private final HaylenRequirements requirements;
    private final List<String> methods = new ArrayList<>();
    private final Map<String, HaylenPermission> permissions = new HashMap<>();
    private int covers;

    HaylenPluginContext(String id, Application application, JSONObject config) {
        this.id = id;
        this.application = application;
        this.config = config;
        overlay = new HaylenOverlay(id);
        requirements = new HaylenRequirements(application, "The plugin \"" + id + "\"");
    }

    public String id() {
        return id;
    }

    public Application application() {
        return application;
    }

    // The running activity, an `AppCompatActivity` and so a `ComponentActivity`, or `null` while there is none.
    public HaylenActivity activity() {
        return HaylenBridge.activity();
    }

    // The parameter values that `app.json` gives the plugin, over the defaults of its `plugin.json`.
    public JSONObject config() {
        return config;
    }

    // Answers `<id>.<method>` on the main thread.
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

    // Opens the screen `<id>.<name>` through an Activity Result contract, whose launcher every activity registers under the key `haylen.<id>.<name>` before it starts, so a result that arrives after the end of the process reaches the next app as `screenRestored`. The input turns the parameters of the app into the input of the contract, and the output turns the output of the contract into the result of the screen, while a `null` output, as the contracts of AndroidX give when the person backs out, ends the screen with the code `cancelled`.
    public <I, O> void registerScreen(String name, ActivityResultContract<I, O> contract, HaylenScreen.Input<I> input, HaylenScreen.Output<O> output) {
        HaylenScreens.register(id, name, new HaylenScreens.ContractScreen<>(contract, input, output));
    }

    // Opens the screen `<id>.<name>` with the opener, which shows the screen itself, such as through the launcher of an SDK, and ends it through the `HaylenScreen` it receives.
    public void registerScreen(String name, HaylenScreen.Opener opener) {
        HaylenScreens.register(id, name, new HaylenScreens.OpenerScreen(opener));
    }

    // The screen of the plugin that showed when the process ended, which the plugin ends once its answer arrives, such as through the launcher of an SDK that the plugin registered itself, so the end reaches the next app as `screenRestored`. It is `null` without one, and the screens of contracts end on their own.
    public HaylenScreen restoredScreen() {
        return HaylenScreens.restoredScreen(id);
    }

    // Opens the video stream `name` of the plugin, which the app draws through `handle:videoStream(name)`, with a size, 0 by 0 until the first frame, and a format, or returns the stream that is open already. Throws an `IllegalArgumentException` for an empty name, a negative size or a stream that is open with another format.
    public HaylenVideoStream openVideoStream(String name, int width, int height, HaylenVideoStream.Format format) {
        return HaylenVideoStream.open(id, name, width, height, format);
    }

    // Opens the audio stream `name` of the plugin, which the app plays through `handle:audioStream(name)`, with its sample rate, its channels, its format and room for `capacity` frames, or returns the stream that is open already. Throws an `IllegalArgumentException` for an empty name, a rate, channel count or capacity below 1, or a stream that is open with another rate, channel count or format.
    public HaylenAudioStream openAudioStream(String name, int sampleRate, int channels, HaylenAudioStream.Format format, int capacity) {
        return HaylenAudioStream.open(id, name, sampleRate, channels, format, capacity);
    }

    // Sends `<id>.<event>` to the app.
    public void emit(String event, Object payload) {
        HaylenBridge.emit(id + "." + event, payload, false);
    }

    // Sends `<id>.<event>` retained, so it waits for the first listener of its name, such as the deep link that opened the app.
    public void emitRetained(String event, Object payload) {
        HaylenBridge.emit(id + "." + event, payload, true);
    }

    // Sends `<id>.<event>`, retained or not, and batched, so the events of the name that arrive in one frame reach the app as one list in order, such as the readings of a sensor.
    public void emit(String event, Object payload, boolean retain, boolean batched) {
        HaylenBridge.emit(id + "." + event, payload, retain, batched);
    }

    // Places native views over the app. It works on the main thread while an activity exists.
    public HaylenOverlay overlay() {
        return overlay;
    }

    // What the project of the app holds, which the plugin checks before it calls a system API that needs it, from any thread.
    public HaylenRequirements requirements() {
        return requirements;
    }

    // Registers a permission that the plugin declares in the `permissions` of its `plugin.json`, so the engine answers `<id>.permission` and `<id>.requestPermission`, which `system.permission` and `system.requestPermission` call, for it. Throws an `IllegalArgumentException` for a name that the plugin registered before.
    public void registerPermission(HaylenPermission permission) {
        boolean first;
        synchronized (permissions) {
            if (permissions.containsKey(permission.getName())) {
                throw new IllegalArgumentException("The plugin \"" + id + "\" registered the permission \"" + permission.getName() + "\" twice.");
            }
            first = permissions.isEmpty();
            permissions.put(permission.getName(), permission);
        }
        if (first) {
            register("permission", (params, reply) -> answerPermission(params, reply, false));
            register("requestPermission", (params, reply) -> answerPermission(params, reply, true));
        }
    }

    // Runs `granted` on the main thread once the app holds the permission `name` that the plugin registered, such as `camera`, after it asked the person where the app does not hold it yet, as `system.requestPermission` does. A permission that the person refused fails the call with the code `denied` and the sentence that names it and how to ask again, a manifest that lacks it fails the call with the code `unsupported` and the missing lines in `data.missing`, and a `granted` that throws, a `HaylenBridge.Failure` among them, fails the call with its error. A platform that tells nothing until the app tries, which Android does only while no activity exists, runs `granted` too. Throws an `IllegalArgumentException` for a name that the plugin did not register.
    public void requirePermission(String name, HaylenBridge.Reply reply, Runnable granted) {
        HaylenPermission permission = findPermission(name);
        if (permission == null) {
            throw new IllegalArgumentException("The plugin \"" + id + "\" requires the permission \"" + name + "\", which it did not register with \"registerPermission\".");
        }
        runOnMainThread(() -> HaylenPermissions.ask(permission, application, requirements, (state, message, missing) -> {
            if (state.equals("granted") || state.equals("unknown")) {
                try {
                    granted.run();
                } catch (Exception error) {
                    reply.failure(error);
                }
            } else if (state.equals("unsupported")) {
                reply.failure(message, "unsupported", missing.isEmpty() ? null : Collections.singletonMap("missing", missing));
            } else {
                reply.failure(HaylenPermissions.refusal(permission), "denied", null);
            }
        }));
    }

    // Tells the app that native UI of the plugin covers it, such as a full screen ad, which halts and mutes the app until the matching `uncoverApp`. Covers nest, and the ones still open when the activity is destroyed end with it.
    public synchronized void coverApp() {
        if (HaylenBridge.activity() == null) {
            Log.w("haylen", "The plugin \"" + id + "\" covered the app while no activity exists, which covers nothing.");
            return;
        }
        ++covers;
        nativeCoverApp();
    }

    public synchronized void uncoverApp() {
        if (covers == 0) {
            Log.e("haylen", "The plugin \"" + id + "\" uncovered the app without covering it first.");
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

    // A name that the plugin did not register on this platform answers `unsupported`, as a platform without the capability does.
    private void answerPermission(Object params, HaylenBridge.Reply reply, boolean asking) {
        String name = params instanceof JSONObject object ? object.optString("name") : "";
        HaylenPermission permission = findPermission(name);
        if (permission == null) {
            reply.success(HaylenPermissions.toJson("unsupported", "The plugin \"" + id + "\" registers no permission \"" + name + "\" on Android.", Collections.emptyList()));
            return;
        }
        HaylenPermissions.Answer answer = (state, message, missing) -> reply.success(HaylenPermissions.toJson(state, message, missing));
        if (asking) {
            HaylenPermissions.ask(permission, application, requirements, answer);
        } else {
            HaylenPermissions.check(permission, application, requirements, answer);
        }
    }

    private HaylenPermission findPermission(String name) {
        synchronized (permissions) {
            return permissions.get(name);
        }
    }

    synchronized void endCovers() {
        for (; covers > 0; --covers) {
            nativeUncoverApp();
        }
    }

    // A plugin that failed to load answers nothing, so the methods and the screens it registered before failing go away with it.
    void unregisterAll() {
        HaylenScreens.unregister(id);
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
