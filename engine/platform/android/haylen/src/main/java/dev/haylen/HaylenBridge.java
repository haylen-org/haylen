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
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

// Native side of the platform bridge on Android. Handlers run on the main thread and may reply later from any thread. A handler that throws fails its call instead of crashing the app.
public final class HaylenBridge {
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

    private static final Map<String, MethodHandler> handlers = new ConcurrentHashMap<>();
    private static final Map<Long, PendingReply> pending = new ConcurrentHashMap<>();
    private static final Handler mainThread = new Handler(Looper.getMainLooper());
    private static Activity activity;

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
        register("system.open_url", (params, reply) -> {
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
        handlers.put(method, handler);
    }

    public static void unregister(String method) {
        handlers.remove(method);
    }

    // Sends an event to the app, which receives it through haylen.platform.on.
    public static void emit(String event, Object payload) {
        nativeEmit(utf8(event), utf8(toJson(payload)));
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

    // Called from the native frame thread for every call that no C++ handler answers.
    static void dispatch(long call, byte[] methodBytes, byte[] paramsBytes) {
        String method = new String(methodBytes, StandardCharsets.UTF_8);
        PendingReply reply = new PendingReply(call);
        MethodHandler handler = handlers.get(method);
        if (handler == null) {
            reply.failure("No native handler is registered for " + method + ".", "no_handler", null);
            return;
        }
        Object parsed;
        try {
            parsed = new JSONTokener(new String(paramsBytes, StandardCharsets.UTF_8)).nextValue();
        } catch (JSONException error) {
            reply.failure(error.getMessage());
            return;
        }
        pending.put(call, reply);
        mainThread.post(() -> {
            if (activity == null) {
                reply.failure("The activity was destroyed before " + method + " ran.");
                return;
            }
            if (reply.isCancelled()) {
                return;
            }
            try {
                handler.handle(parsed, reply);
            } catch (Exception error) {
                reply.failure(error);
            }
        });
    }

    // Called from the native frame thread when the app cancels a call or its timeout passes.
    static void cancel(long call) {
        PendingReply reply = pending.remove(call);
        if (reply != null) {
            reply.cancel();
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

    private static native void nativeEmit(byte[] event, byte[] json);
}
