package dev.haylen;

import android.os.Handler;
import android.os.Looper;
import java.util.ArrayList;
import java.util.List;
import org.json.JSONException;

// A screen of a plugin that shows over the app until it ends with one result, such as a paywall, a sign-in flow or the activity of an SDK, which the app opens with `handle:openScreen(name, params, options)`. The engine covers the app before the screen shows and until it ends. The first end counts, from any thread, and later ends are dropped.
public final class HaylenScreen {
    // Opens a screen that the plugin shows itself, such as through the launcher of an SDK, on the main thread. The plugin ends it through the screen, and a thrown exception fails it like `fail(Throwable)`.
    public interface Opener {
        void open(Object params, HaylenScreen screen) throws Exception;
    }

    // Turns the parameters of the app into the input of the Activity Result contract of a screen, on the main thread.
    public interface Input<I> {
        I create(Object params) throws Exception;
    }

    // Turns the output of the contract into the result of the screen, any value that `HaylenBridge.Reply.success` takes. A thrown `HaylenBridge.Failure` fails the screen with its code and data.
    public interface Output<O> {
        Object convert(O output) throws Exception;
    }

    private static final Handler mainThread = new Handler(Looper.getMainLooper());

    private final long id;
    private final String plugin;
    private final String name;
    private final String state;
    private final boolean opaque;
    private final boolean restored;
    private final List<Runnable> cancelListeners = new ArrayList<>();
    private boolean ended;
    private boolean cancelled;

    HaylenScreen(long id, String plugin, String name, String state, boolean opaque, boolean restored) {
        this.id = id;
        this.plugin = plugin;
        this.name = name;
        this.state = state;
        this.opaque = opaque;
        this.restored = restored;
    }

    public String name() {
        return name;
    }

    // Whether the app asked for a screen that hides it completely.
    public boolean isOpaque() {
        return opaque;
    }

    // Whether the process ended while the screen showed, so its end reaches the next app as the retained event `screenRestored` with the state that the earlier app gave.
    public boolean isRestored() {
        return restored;
    }

    // Ends the screen with its result, converted like the value of `HaylenBridge.Reply.success`, with `byte[]` and `ByteBuffer` values as bytes.
    public void finish(Object result) {
        HaylenPayload payload;
        try {
            payload = HaylenPayload.encode(result);
        } catch (JSONException error) {
            end(false, HaylenBridge.encodeFailure("The screen \"" + name + "\" ended with a value that is not JSON: " + error.getMessage(), null, null), HaylenPayload.NO_BUFFERS);
            return;
        }
        end(true, payload.json, payload.buffers);
    }

    // Ends the screen with a failure, such as the code `cancelled` when the person closed it, whose code and data the call keeps.
    public void fail(String message, String code, Object data) {
        end(false, HaylenBridge.encodeFailure(message, code, data), HaylenPayload.NO_BUFFERS);
    }

    // Ends the screen with a thrown error: a `HaylenBridge.Failure` keeps its code and data, and any other error fails with the code `exception`.
    public void fail(Throwable error) {
        end(false, HaylenBridge.encodeFailure(error), HaylenPayload.NO_BUFFERS);
    }

    // Whether the app gave the screen up, with a cancel or a timeout.
    public synchronized boolean isCancelled() {
        return cancelled;
    }

    // Runs the listener on the main thread when the app gives the screen up, at once when that already happened, so the plugin closes the UI that it shows itself and ends the screen.
    public void onCancel(Runnable listener) {
        synchronized (this) {
            if (!cancelled) {
                cancelListeners.add(listener);
                return;
            }
        }
        mainThread.post(listener);
    }

    long id() {
        return id;
    }

    String plugin() {
        return plugin;
    }

    String state() {
        return state;
    }

    // The key of the plugin and the screen, under which the activity registers the launcher of the screen.
    String key() {
        return HaylenScreens.key(plugin, name);
    }

    synchronized boolean hasCancelListeners() {
        return !cancelListeners.isEmpty();
    }

    void cancel() {
        List<Runnable> listeners;
        synchronized (this) {
            if (cancelled || ended) {
                return;
            }
            cancelled = true;
            listeners = new ArrayList<>(cancelListeners);
            cancelListeners.clear();
        }
        for (Runnable listener : listeners) {
            listener.run();
        }
    }

    // A screen that the process ended under reaches the next app through the restore of the relay, with its kept state.
    private void end(boolean ok, byte[] json, Object[] buffers) {
        synchronized (this) {
            if (ended) {
                return;
            }
            ended = true;
        }
        HaylenScreens.ended(this);
        if (restored) {
            nativeRestore(HaylenBridge.utf8(plugin), HaylenBridge.utf8(name), HaylenBridge.utf8(state), ok, json, buffers);
        } else {
            nativeFinish(id, ok, json, buffers);
        }
    }

    private static native void nativeFinish(long id, boolean ok, byte[] json, Object[] buffers);

    private static native void nativeRestore(byte[] plugin, byte[] name, byte[] state, boolean ok, byte[] json, Object[] buffers);
}
