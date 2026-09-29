package dev.haylen;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

// Native side of the platform bridge on Android. Handlers run on the main thread or on the shared background thread and may reply later from any thread. A handler that throws fails its call instead of crashing the app.
public final class HaylenBridge {
    // The thread a handler runs on. MAIN handlers may touch the activity and views. BACKGROUND handlers share one background thread, so work that does not touch the UI, such as disk or database access, never holds up the main thread.
    public enum Threading {
        MAIN,
        BACKGROUND
    }

    public interface Reply {
        void success(Object value);

        void failure(String message);

        // Fails the call with a code that tells failures apart and data for the app, both optional.
        void failure(String message, String code, Object data);

        // Fails the call with a thrown error: a Failure keeps its code and data, and any other error fails with the code exception.
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
    private static volatile Activity activity;

    // The built-in methods are registered when the class loads, before any app code can register, so an app handler of the same name always replaces them.
    static {
        register("device.info", (params, reply) -> {
            JSONObject info = new JSONObject();
            info.put("model", Build.MODEL);
            info.put("system", "Android");
            info.put("systemVersion", Build.VERSION.RELEASE);
            info.put("locale", Locale.getDefault().toLanguageTag());
            reply.success(info);
        });
        register("system.locale", (params, reply) -> reply.success(Locale.getDefault().toLanguageTag()));
        register("system.openUrl", (params, reply) -> {
            String url = params instanceof JSONObject ? ((JSONObject) params).optString("url", "") : "";
            if (url.isEmpty()) {
                reply.failure("The url is missing.");
                return;
            }
            try {
                activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
                reply.success(true);
            } catch (ActivityNotFoundException error) {
                reply.failure("The url could not be opened.");
            }
        });
        register("haptics.vibrate", (params, reply) -> {
            long duration = params instanceof JSONObject ? ((JSONObject) params).optLong("duration", 40) : 40;
            Vibrator vibrator = activity.getSystemService(Vibrator.class);
            if (vibrator != null && vibrator.hasVibrator()) {
                vibrator.vibrate(VibrationEffect.createOneShot(duration, VibrationEffect.DEFAULT_AMPLITUDE));
            }
            reply.success(null);
        });
    }

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

    // Sends an event to the app, which receives it through haylen.platform.on.
    public static void emit(String event, Object payload) {
        emit(event, payload, false);
    }

    // Sends an event to the app. A retained event that arrives while nothing listens waits for the first listener. An event sent while no app runs, such as before the first activity loads the native library, waits here until an app starts.
    public static void emit(String event, Object payload, boolean retain) {
        KeptEvent kept = new KeptEvent(event, utf8(event), utf8(toJson(payload)), retain);
        synchronized (keptEvents) {
            if (appRunning) {
                nativeEmit(kept.name, kept.json, kept.retain);
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

    public static Activity activity() {
        return activity;
    }

    static void attach(Activity owner) {
        activity = owner;
    }

    static void detach() {
        activity = null;
    }

    // Called from the frame thread of the engine when an app starts and before it stops. The engine takes events only while an app runs, so the events kept meanwhile reach the app that starts, in order.
    static void setAppRunning(boolean running) {
        synchronized (keptEvents) {
            appRunning = running;
            if (!running) {
                return;
            }
            for (KeptEvent kept : keptEvents) {
                nativeEmit(kept.name, kept.json, kept.retain);
            }
            keptEvents.clear();
        }
    }

    // Called from the native frame thread for every call that no C++ handler answers. The parameters are parsed on the thread of the handler, so the frame thread only hands the call over.
    static void dispatch(long call, byte[] methodBytes, byte[] paramsBytes) {
        String method = new String(methodBytes, StandardCharsets.UTF_8);
        PendingReply reply = new PendingReply(call);
        Registration registration = handlers.get(method);
        if (registration == null) {
            reply.failure("No native handler is registered for " + method + ".", "noHandler", null);
            return;
        }
        pending.put(call, reply);
        Runnable run = () -> handle(method, registration, paramsBytes, reply);
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

    private static void handle(String method, Registration registration, byte[] paramsBytes, PendingReply reply) {
        if (reply.isCancelled()) {
            return;
        }
        if (registration.threading == Threading.MAIN && activity == null) {
            reply.failure("The activity was destroyed before " + method + " ran.");
            return;
        }

        Object params;
        try {
            params = new JSONTokener(new String(paramsBytes, StandardCharsets.UTF_8)).nextValue();
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

    private static void resolve(long call, boolean ok, String json) {
        nativeResolve(call, ok, utf8(json));
    }

    // Text crosses JNI as UTF-8 bytes, because the JNI string functions use a modified UTF-8 that breaks characters outside the Basic Multilingual Plane, such as emoji.
    private static byte[] utf8(String text) {
        return text.getBytes(StandardCharsets.UTF_8);
    }

    private static String toJson(Object value) {
        if (value == null) {
            return "null";
        }
        if (value instanceof String) {
            return JSONObject.quote((String) value);
        }
        Object wrapped = JSONObject.wrap(value);
        return wrapped == null ? "null" : wrapped.toString();
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
        final byte[] json;
        final boolean retain;

        KeptEvent(String event, byte[] name, byte[] json, boolean retain) {
            this.event = event;
            this.name = name;
            this.json = json;
            this.retain = retain;
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
            if (settle()) {
                resolve(call, true, toJson(value));
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
                resolve(call, false, failure.toString());
            } catch (JSONException error) {
                resolve(call, false, JSONObject.quote(message == null ? "The native call failed." : message));
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

    private static native void nativeResolve(long call, boolean ok, byte[] json);

    private static native void nativeEmit(byte[] event, byte[] json, boolean retain);
}
