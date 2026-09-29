package dev.haylen.samples.platform;

import android.app.Activity;
import android.app.Application;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import dev.haylen.HaylenBridge;
import java.util.HashMap;
import java.util.Map;
import org.json.JSONObject;

// Answers sample.echo and sample.ticker, and sends sample.activity when the activity of the app resumes or pauses.
final class SamplePlugin {
    private static final Handler mainThread = new Handler(Looper.getMainLooper());

    private SamplePlugin() {}

    static void register(Application application) {
        HaylenBridge.register("sample.echo", (params, reply) -> {
            String text = params instanceof JSONObject ? ((JSONObject) params).optString("text", "") : "";
            if (text.isEmpty()) {
                reply.failure("sample.echo needs a text.");
                return;
            }
            Map<String, Object> answer = new HashMap<>();
            answer.put("echo", text);
            answer.put("characters", text.codePointCount(0, text.length()));
            answer.put("language", "Java");
            answer.put("system", "Android " + Build.VERSION.RELEASE + ", API " + Build.VERSION.SDK_INT);
            reply.success(answer);
        });

        HaylenBridge.register("sample.ticker", (params, reply) -> {
            JSONObject options = params instanceof JSONObject ? (JSONObject) params : new JSONObject();
            int count = Math.max(1, options.optInt("count", 5));
            long interval = Math.max(1, options.optLong("interval", 500));
            for (int tick = 1; tick <= count; tick++) {
                Map<String, Object> payload = new HashMap<>();
                payload.put("count", tick);
                payload.put("total", count);
                payload.put("source", "Java");
                mainThread.postDelayed(() -> HaylenBridge.emit("sample.tick", payload), interval * tick);
            }
            Map<String, Object> answer = new HashMap<>();
            answer.put("started", true);
            answer.put("count", count);
            answer.put("interval", interval);
            reply.success(answer);
        });

        application.registerActivityLifecycleCallbacks(new ActivityEvents());
    }

    private static void sendActivity(String state) {
        Map<String, Object> payload = new HashMap<>();
        payload.put("state", state);
        payload.put("source", "Java");
        HaylenBridge.emit("sample.activity", payload);
    }

    // Only resuming and pausing matter to the sample, so the other callbacks stay empty.
    private static final class ActivityEvents implements Application.ActivityLifecycleCallbacks {
        @Override
        public void onActivityResumed(Activity activity) {
            sendActivity("resumed");
        }

        @Override
        public void onActivityPaused(Activity activity) {
            sendActivity("paused");
        }

        @Override
        public void onActivityCreated(Activity activity, Bundle state) {}

        @Override
        public void onActivityStarted(Activity activity) {}

        @Override
        public void onActivityStopped(Activity activity) {}

        @Override
        public void onActivitySaveInstanceState(Activity activity, Bundle state) {}

        @Override
        public void onActivityDestroyed(Activity activity) {}
    }
}
