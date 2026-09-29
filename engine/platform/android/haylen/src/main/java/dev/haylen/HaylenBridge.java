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
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

// Native side of the platform bridge on Android. Handlers run on the main thread and may reply later from any thread.
public final class HaylenBridge {
    public interface Reply {
        void success(Object value);

        void failure(String message);
    }

    public interface MethodHandler {
        void handle(Object params, Reply reply);
    }

    private static final Map<String, MethodHandler> handlers = new ConcurrentHashMap<>();
    private static final Handler mainThread = new Handler(Looper.getMainLooper());
    private static Activity activity;

    // The built-in methods are registered when the class loads, before any app code can register, so an app handler of the same name always replaces them.
    static {
        register("device.info", (params, reply) -> {
            try {
                JSONObject info = new JSONObject();
                info.put("model", Build.MODEL);
                info.put("system", "Android");
                info.put("systemVersion", Build.VERSION.RELEASE);
                info.put("locale", Locale.getDefault().toLanguageTag());
                reply.success(info);
            } catch (JSONException error) {
                reply.failure(error.getMessage());
            }
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
        MethodHandler handler = handlers.get(method);
        if (handler == null) {
            resolve(call, false, failureJson("No native handler is registered for " + method + "."));
            return;
        }
        Object parsed;
        try {
            parsed = new JSONTokener(new String(paramsBytes, StandardCharsets.UTF_8)).nextValue();
        } catch (JSONException error) {
            resolve(call, false, failureJson(error.getMessage()));
            return;
        }
        mainThread.post(() -> {
            if (activity == null) {
                resolve(call, false, failureJson("The activity was destroyed before " + method + " ran."));
                return;
            }
            handler.handle(parsed, new Reply() {
                @Override
                public void success(Object value) {
                    resolve(call, true, toJson(value));
                }

                @Override
                public void failure(String message) {
                    resolve(call, false, failureJson(message));
                }
            });
        });
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

    private static String failureJson(String message) {
        return "{\"message\":" + JSONObject.quote(message == null ? "The native call failed." : message) + "}";
    }

    private static native void nativeResolve(long call, boolean ok, byte[] json);

    private static native void nativeEmit(byte[] event, byte[] json);
}
