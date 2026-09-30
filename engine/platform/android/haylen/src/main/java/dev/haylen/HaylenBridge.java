package dev.haylen;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONException;
import org.json.JSONObject;

// Native side of the platform bridge on Android. Handlers run on the main thread or on the shared background thread and may reply later from any thread. A handler that throws fails its call instead of crashing the app. Parameters, results and events carry bytes as `byte[]` and `ByteBuffer` values, which cross as byte buffers instead of text.
public final class HaylenBridge {
    // The thread a handler runs on. The `MAIN` handlers may touch the activity and views. The `BACKGROUND` handlers share one background thread, so work that does not touch the UI, such as disk or database access, never holds up the main thread.
    public enum Threading {
        MAIN,
        BACKGROUND
    }

    public interface Reply {
        // Answers with `null`, a string, a number, a boolean, a `JSONObject`, a `JSONArray`, a map, a collection or an array, with `byte[]` and `ByteBuffer` values anywhere inside.
        void success(Object value);

        void failure(String message);

        // Fails the call with a code that tells failures apart and data for the app, both optional.
        void failure(String message, String code, Object data);

        // Fails the call with a thrown error: a `Failure` keeps its code and data, and any other error fails with the code `exception`.
        void failure(Throwable error);

        boolean isCancelled();

        // Runs the listener on the main thread once the app cancels the call or its timeout passes, at once when that already happened.
        void onCancel(Runnable listener);
    }

    public interface MethodHandler {
        void handle(Object params, Reply reply) throws Exception;
    }

    // Thrown by a handler to fail its call with a code and data.
    public static final class Failure extends Exception {
        private final String code;
        private final transient Object data;

        public Failure(String message, String code, Object data) {
            super(message);
            this.code = code;
            this.data = data;
        }

        public String code() {
            return code;
        }

        public Object data() {
            return data;
        }
    }

    // Events that native code sends while no app runs wait here, up to this many of each name, until the next app starts.
    private static final int KEPT_EVENTS_PER_NAME = 32;

    private static final Map<String, Registration> handlers = new ConcurrentHashMap<>();
    private static final Map<Long, PendingReply> pending = new ConcurrentHashMap<>();
    private static final Handler mainThread = new Handler(Looper.getMainLooper());
    private static final ExecutorService background = Executors.newSingleThreadExecutor(task -> {
        Thread thread = new Thread(task, "haylen-bridge");
        thread.setDaemon(true);
        return thread;
    });
    private static final List<KeptEvent> keptEvents = new ArrayList<>();
    private static boolean appRunning;
    private static volatile HaylenActivity activity;

    private HaylenBridge() {}

    public static void register(String method, MethodHandler handler) {
        register(method, handler, Threading.MAIN);
    }

    public static void register(String method, MethodHandler handler, Threading threading) {
        handlers.put(method, new Registration(handler, threading));
    }

    public static void unregister(String method) {
        handlers.remove(method);
    }

    // Sends an event to the app, which receives it through `haylen.platform.on`.
    public static void emit(String event, Object payload) {
        emit(event, payload, false, false);
    }

    public static void emit(String event, Object payload, boolean retain) {
        emit(event, payload, retain, false);
    }

    // Sends an event to the app. A retained event that arrives while nothing listens waits for the first listener. The batched events of a name that arrive in one frame reach the app once, as one list in order, which suits sensors and progress. An event sent while no app runs, such as before the first activity loads the native library, waits here until an app starts. A payload that is not JSON is logged and dropped.
    public static void emit(String event, Object payload, boolean retain, boolean batched) {
        HaylenPayload encoded;
        try {
            encoded = HaylenPayload.encode(payload);
        } catch (JSONException error) {
            Log.e("haylen", "The event \"" + event + "\" carried a payload that is not JSON and was dropped: " + error.getMessage());
            return;
        }
        KeptEvent kept = new KeptEvent(event, utf8(event), encoded, retain, batched);
        synchronized (keptEvents) {
            if (appRunning) {
                kept.send();
                return;
            }
            // The oldest event of the name gives way once the name has its limit.
            int oldest = -1;
            int count = 0;
            for (int index = 0; index < keptEvents.size(); ++index) {
                if (!keptEvents.get(index).event.equals(event)) {
                    continue;
                }
                if (oldest < 0) {
                    oldest = index;
                }
                ++count;
            }
            if (count == KEPT_EVENTS_PER_NAME) {
                keptEvents.remove(oldest);
            }
            keptEvents.add(kept);
        }
    }

    // The running activity, or `null` while there is none.
    public static HaylenActivity activity() {
        return activity;
    }

    static void attach(HaylenActivity owner) {
        activity = owner;
    }

    // A newer activity may have attached itself before an older one goes away.
    static void detach(HaylenActivity owner) {
        if (activity == owner) {
            activity = null;
        }
    }

    // Called from the frame thread of the engine when an app starts and before it stops. The engine takes events only while an app runs, so the events kept meanwhile reach the app that starts, in order.
    static void setAppRunning(boolean running) {
        synchronized (keptEvents) {
            appRunning = running;
            if (!running) {
                return;
            }
            for (KeptEvent kept : keptEvents) {
                kept.send();
            }
            keptEvents.clear();
        }
    }

    // Called from the native frame thread for every call that no C++ handler answers, with the byte buffers of its parameters. The parameters are parsed on the thread of the handler, so the frame thread only hands the call over.
    static void dispatch(long call, byte[] methodBytes, byte[] paramsBytes, byte[][] buffers) {
        String method = new String(methodBytes, StandardCharsets.UTF_8);
        PendingReply reply = new PendingReply(call);
        Registration registration = handlers.get(method);
        if (registration == null) {
            reply.failure("No native handler is registered for \"" + method + "\".", "noHandler", null);
            return;
        }
        pending.put(call, reply);
        Runnable run = () -> handle(method, registration, paramsBytes, buffers, reply);
        if (registration.threading == Threading.BACKGROUND) {
            background.execute(run);
        } else {
            mainThread.post(run);
        }
    }

    // Called from the native frame thread when the app cancels a call or its timeout passes.
    static void cancel(long call) {
        PendingReply reply = pending.remove(call);
        if (reply != null) {
            reply.cancel();
        }
    }

    private static void handle(String method, Registration registration, byte[] paramsBytes, byte[][] buffers, PendingReply reply) {
        if (reply.isCancelled()) {
            return;
        }
        if (registration.threading == Threading.MAIN && activity == null) {
            reply.failure("The activity was destroyed before \"" + method + "\" ran.");
            return;
        }

        Object params;
        try {
            params = HaylenPayload.decode(paramsBytes, buffers);
        } catch (JSONException error) {
            reply.failure(error.getMessage());
            return;
        }
        try {
            registration.handler.handle(params, reply);
        } catch (Exception error) {
            reply.failure(error);
        }
    }

    // Failures carry JSON alone.
    private static void fail(long call, String json) {
        nativeResolve(call, false, utf8(json), HaylenPayload.NO_BUFFERS);
    }

    // Text crosses JNI as UTF-8 bytes, because the JNI string functions use a modified UTF-8 that breaks characters outside the Basic Multilingual Plane, such as emoji.
    private static byte[] utf8(String text) {
        return text.getBytes(StandardCharsets.UTF_8);
    }

    private static final class Registration {
        final MethodHandler handler;
        final Threading threading;

        Registration(MethodHandler handler, Threading threading) {
            this.handler = handler;
            this.threading = threading;
        }
    }

    private static final class KeptEvent {
        final String event;
        final byte[] name;
        final HaylenPayload payload;
        final boolean retain;
        final boolean batched;

        KeptEvent(String event, byte[] name, HaylenPayload payload, boolean retain, boolean batched) {
            this.event = event;
            this.name = name;
            this.payload = payload;
            this.retain = retain;
            this.batched = batched;
        }

        void send() {
            nativeEmit(name, payload.json, payload.buffers, retain, batched);
        }
    }

    // A call answers once: later answers, and answers after a cancel, are dropped.
    private static final class PendingReply implements Reply {
        private final long call;
        private final List<Runnable> cancelListeners = new ArrayList<>();
        private boolean settled;
        private boolean cancelled;

        PendingReply(long call) {
            this.call = call;
        }

        @Override
        public void success(Object value) {
            if (!settle()) {
                return;
            }
            try {
                HaylenPayload payload = HaylenPayload.encode(value);
                nativeResolve(call, true, payload.json, payload.buffers);
            } catch (JSONException error) {
                fail(call, JSONObject.quote("The native handler returned a value that is not JSON: " + error.getMessage()));
            }
        }

        @Override
        public void failure(String message) {
            failure(message, null, null);
        }

        @Override
        public void failure(String message, String code, Object data) {
            if (!settle()) {
                return;
            }
            try {
                JSONObject failure = new JSONObject();
                failure.put("message", message == null ? "The native call failed." : message);
                failure.putOpt("code", code);
                if (data != null) {
                    failure.put("data", JSONObject.wrap(data));
                }
                fail(call, failure.toString());
            } catch (JSONException error) {
                fail(call, JSONObject.quote(message == null ? "The native call failed." : message));
            }
        }

        @Override
        public void failure(Throwable error) {
            if (error instanceof Failure) {
                Failure failure = (Failure) error;
                failure(failure.getMessage(), failure.code(), failure.data());
                return;
            }
            String message = error.getMessage() == null ? error.getClass().getSimpleName() : error.getMessage();
            failure(message, "exception", Collections.singletonMap("type", error.getClass().getName()));
        }

        @Override
        public synchronized boolean isCancelled() {
            return cancelled;
        }

        @Override
        public void onCancel(Runnable listener) {
            synchronized (this) {
                if (!cancelled) {
                    cancelListeners.add(listener);
                    return;
                }
            }
            mainThread.post(listener);
        }

        void cancel() {
            List<Runnable> listeners;
            synchronized (this) {
                if (settled) {
                    return;
                }
                settled = true;
                cancelled = true;
                listeners = new ArrayList<>(cancelListeners);
                cancelListeners.clear();
            }
            for (Runnable listener : listeners) {
                mainThread.post(listener);
            }
        }

        private synchronized boolean settle() {
            if (settled) {
                return false;
            }
            settled = true;
            pending.remove(call);
            return true;
        }
    }

    private static native void nativeResolve(long call, boolean ok, byte[] json, Object[] buffers);

    private static native void nativeEmit(byte[] event, byte[] json, Object[] buffers, boolean retain, boolean batched);
}
