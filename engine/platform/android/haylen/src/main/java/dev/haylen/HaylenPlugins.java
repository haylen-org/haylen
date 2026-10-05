package dev.haylen;

import android.app.Application;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

// The native parts of the plugins of the app, in load order. The class `HaylenPluginProvider`, which the manifest of the `dev.haylen:haylen-plugins` library declares, loads them when the process starts, `HaylenActivity` forwards its creation, its destruction and its other events to them while the AndroidX lifecycle of the activity forwards its start, resume, pause and stop, and the engine reads their ids and hands them its errors.
final class HaylenPlugins {
    private static final String TAG = "haylen";
    private static final String META_DATA_PREFIX = "dev.haylen.plugin.";
    // The file of the APK assets that `haylen.py prepare` writes with the plugins of `app.json` in load order, their versions and their parameter values.
    private static final String PLUGINS_FILE = "haylen-plugins.json";

    private static final Handler mainThread = new Handler(Looper.getMainLooper());
    private static List<Loaded> loaded = Collections.emptyList();
    private static boolean started;
    private static boolean checked;

    private HaylenPlugins() {}

    // Creates every plugin class that the manifest names and calls `onLoad` in the load order of `haylen-plugins.json`, where every plugin follows the plugins it requires. A class that cannot be created, or an `onLoad` that fails, is logged and leaves its plugin out, so the app runs without its native part.
    static void load(Application application) {
        Map<String, String> classes = readClasses(application);
        Map<String, JSONObject> declared = readDeclared(application);

        List<Loaded> all = new ArrayList<>();
        for (String id : order(classes.keySet(), declared.keySet())) {
            HaylenPlugin plugin = create(application, id, classes.get(id));
            if (plugin == null) {
                continue;
            }
            JSONObject entry = declared.get(id);
            JSONObject config = entry != null ? entry.optJSONObject("config") : null;
            HaylenPluginContext context = new HaylenPluginContext(id, application, config != null ? config : new JSONObject());
            try {
                plugin.onLoad(context);
            } catch (Exception error) {
                Log.e(TAG, "The plugin \"" + id + "\" failed to load, so the app runs without its native part.", error);
                context.unregisterAll();
                continue;
            }
            Log.i(TAG, "Loaded the plugin \"" + id + "\" " + (entry != null ? entry.optString("version") : "that \"app.json\" does not list") + ".");
            all.add(new Loaded(plugin, context));
        }
        loaded = Collections.unmodifiableList(all);
        started = true;
    }

    // Called on the main thread when an activity is created. A plugin module that lacks the dependency on `dev.haylen:haylen-plugins` leaves the app without the provider, so the plugins that the manifest names never load, which the log tells once.
    static void checkLoaded(Application application) {
        if (started || checked) {
            return;
        }
        checked = true;
        Set<String> ids = readClasses(application).keySet();
        if (!ids.isEmpty()) {
            Log.e(TAG, "The manifest names the plugins \"" + String.join("\", \"", ids) + "\" in \"dev.haylen.plugin.<id>\" meta-data, but \"dev.haylen.HaylenPluginProvider\" never ran, so the app runs without their native parts. Make every plugin module depend on \"dev.haylen:haylen-plugins\", whose manifest declares the provider.");
        }
    }

    // Called from `JNI_OnLoad` of the native library, which reports the plugins to the engine.
    static byte[] ids() {
        JSONArray ids = new JSONArray();
        for (Loaded entry : loaded) {
            ids.put(entry.context.id());
        }
        return ids.toString().getBytes(StandardCharsets.UTF_8);
    }

    // Called from the frame thread of the engine with the JSON report of every error that stops the app.
    static void reportError(byte[] json) {
        String text = new String(json, StandardCharsets.UTF_8);
        mainThread.post(() -> {
            JSONObject error;
            try {
                error = new JSONObject(text);
            } catch (JSONException failure) {
                Log.e(TAG, "The engine reported an error that is not a JSON object.", failure);
                return;
            }
            for (Loaded entry : loaded) {
                entry.plugin.onAppError(error);
            }
        });
    }

    // From here on the lifecycle of the activity hands the plugins its start, resume, pause and stop.
    static void activityCreated(HaylenActivity activity, Bundle savedInstanceState) {
        for (Loaded entry : loaded) {
            entry.plugin.onActivityCreated(activity, savedInstanceState);
        }
        activity.getLifecycle().addObserver(new ActivityLifecycle(activity));
    }

    // The covers that plugins left open end with the activity, so the next app starts uncovered.
    static void activityDestroyed(HaylenActivity activity) {
        for (Loaded entry : loaded) {
            entry.plugin.onActivityDestroyed(activity);
        }
        for (Loaded entry : loaded) {
            entry.context.endCovers();
        }
    }

    static void newIntent(Intent intent) {
        for (Loaded entry : loaded) {
            entry.plugin.onNewIntent(intent);
        }
    }

    static void configurationChanged(Configuration configuration) {
        for (Loaded entry : loaded) {
            entry.plugin.onConfigurationChanged(configuration);
        }
    }

    static void windowFocusChanged(boolean hasFocus) {
        for (Loaded entry : loaded) {
            entry.plugin.onWindowFocusChanged(hasFocus);
        }
    }

    static void trimMemory(int level) {
        for (Loaded entry : loaded) {
            entry.plugin.onTrimMemory(level);
        }
    }

    // The manifest merges the `dev.haylen.plugin.<id>` entries of every plugin module, each naming the class of its plugin.
    private static Map<String, String> readClasses(Application application) {
        Bundle metaData;
        try {
            metaData = application.getPackageManager().getApplicationInfo(application.getPackageName(), PackageManager.GET_META_DATA).metaData;
        } catch (PackageManager.NameNotFoundException error) {
            throw new IllegalStateException("The package of the app is not installed.", error);
        }
        Map<String, String> classes = new TreeMap<>();
        if (metaData == null) {
            return classes;
        }
        for (String key : metaData.keySet()) {
            if (key.startsWith(META_DATA_PREFIX)) {
                classes.put(key.substring(META_DATA_PREFIX.length()), metaData.getString(key));
            }
        }
        return classes;
    }

    // The plugins of `haylen-plugins.json` by id, in load order.
    private static Map<String, JSONObject> readDeclared(Application application) {
        Map<String, JSONObject> declared = new LinkedHashMap<>();
        try (InputStream input = application.getAssets().open(PLUGINS_FILE)) {
            ByteArrayOutputStream text = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            for (int read; (read = input.read(buffer)) > 0; ) {
                text.write(buffer, 0, read);
            }
            JSONArray plugins = new JSONObject(new String(text.toByteArray(), StandardCharsets.UTF_8)).getJSONArray("plugins");
            for (int index = 0; index < plugins.length(); ++index) {
                JSONObject entry = plugins.getJSONObject(index);
                declared.put(entry.getString("id"), entry);
            }
        } catch (IOException | JSONException error) {
            Log.w(TAG, "The assets of the app have no readable \"" + PLUGINS_FILE + "\", which \"haylen.py prepare\" writes: " + error.getMessage());
        }
        return declared;
    }

    // The plugins keep the load order of `haylen-plugins.json`, and plugins that it does not list follow by id.
    private static List<String> order(Set<String> ids, Set<String> declared) {
        Set<String> ordered = new LinkedHashSet<>(declared);
        ordered.addAll(ids);
        ordered.retainAll(ids);
        return new ArrayList<>(ordered);
    }

    private static HaylenPlugin create(Application application, String id, String className) {
        try {
            Class<?> type = Class.forName(className, true, application.getClassLoader());
            if (!HaylenPlugin.class.isAssignableFrom(type)) {
                Log.e(TAG, "The plugin \"" + id + "\" names the class \"" + className + "\", which does not extend \"dev.haylen.HaylenPlugin\", so the app runs without its native part.");
                return null;
            }
            return type.asSubclass(HaylenPlugin.class).getDeclaredConstructor().newInstance();
        } catch (ReflectiveOperationException | LinkageError error) {
            Log.e(TAG, "The plugin \"" + id + "\" names the class \"" + className + "\", which cannot be found or created with a public constructor without parameters, so the app runs without its native part.", error);
            return null;
        }
    }

    private static final class ActivityLifecycle implements DefaultLifecycleObserver {
        private final HaylenActivity activity;

        ActivityLifecycle(HaylenActivity activity) {
            this.activity = activity;
        }

        @Override
        public void onStart(@NonNull LifecycleOwner owner) {
            for (Loaded entry : loaded) {
                entry.plugin.onActivityStarted(activity);
            }
        }

        @Override
        public void onResume(@NonNull LifecycleOwner owner) {
            for (Loaded entry : loaded) {
                entry.plugin.onActivityResumed(activity);
            }
        }

        @Override
        public void onPause(@NonNull LifecycleOwner owner) {
            for (Loaded entry : loaded) {
                entry.plugin.onActivityPaused(activity);
            }
        }

        @Override
        public void onStop(@NonNull LifecycleOwner owner) {
            for (Loaded entry : loaded) {
                entry.plugin.onActivityStopped(activity);
            }
        }
    }

    private static final class Loaded {
        final HaylenPlugin plugin;
        final HaylenPluginContext context;

        Loaded(HaylenPlugin plugin, HaylenPluginContext context) {
            this.plugin = plugin;
            this.context = context;
        }
    }
}
