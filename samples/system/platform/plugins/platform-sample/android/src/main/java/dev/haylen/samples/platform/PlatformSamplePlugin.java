package dev.haylen.samples.platform;

import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import dev.haylen.HaylenActivity;
import dev.haylen.HaylenPlugin;
import dev.haylen.HaylenPluginContext;
import java.util.HashMap;
import java.util.Map;
import org.json.JSONObject;

// Native part of the platform sample on Android, in Java. It answers platform-sample.echo and platform-sample.ticker, and sends platform-sample.activity when the activity resumes or pauses.
public final class PlatformSamplePlugin extends HaylenPlugin {
    private final Handler mainThread = new Handler(Looper.getMainLooper());
    private HaylenPluginContext context;

    @Override
    public void onLoad(HaylenPluginContext context) {
        this.context = context;
        context.register("echo", (params, reply) -> {
            String text = params instanceof JSONObject ? ((JSONObject) params).optString("text", "") : "";
            if (text.isEmpty()) {
                reply.failure("The method \"platform-sample.echo\" needs a text.");
                return;
            }
            Map<String, Object> answer = new HashMap<>();
            answer.put("echo", text);
            answer.put("characters", text.codePointCount(0, text.length()));
            answer.put("language", "Java");
            answer.put("system", "Android " + Build.VERSION.RELEASE + ", API " + Build.VERSION.SDK_INT);
            reply.success(answer);
        });

        context.register("ticker", (params, reply) -> {
            JSONObject options = params instanceof JSONObject ? (JSONObject) params : new JSONObject();
            int count = Math.max(1, options.optInt("count", 5));
            long interval = Math.max(1, options.optLong("interval", 500));
            for (int tick = 1; tick <= count; tick++) {
                Map<String, Object> payload = new HashMap<>();
                payload.put("count", tick);
                payload.put("total", count);
                payload.put("source", "Java");
                mainThread.postDelayed(() -> context.emit("tick", payload), interval * tick);
            }
            Map<String, Object> answer = new HashMap<>();
            answer.put("started", true);
            answer.put("count", count);
            answer.put("interval", interval);
            reply.success(answer);
        });
    }

    @Override
    public void onActivityResumed(HaylenActivity activity) {
        sendActivity("resumed");
    }

    @Override
    public void onActivityPaused(HaylenActivity activity) {
        sendActivity("paused");
    }

    private void sendActivity(String state) {
        Map<String, Object> payload = new HashMap<>();
        payload.put("state", state);
        payload.put("source", "Java");
        context.emit("activity", payload);
    }
}
