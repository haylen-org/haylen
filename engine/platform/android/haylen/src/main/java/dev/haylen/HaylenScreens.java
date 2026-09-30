package dev.haylen;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContract;
import androidx.lifecycle.Lifecycle;
import androidx.savedstate.SavedStateRegistry;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

// The screens that plugins register and the screen that shows over the app, one at a time in the process. Every activity registers the launcher of each contract screen under the stable key `haylen.<id>.<screen>` before it starts, so a result that Android delivers to a new activity, after a recreation or the end of the process, finds its launcher. The activity keeps the screen that shows in its saved state, so when the process ended under it the next process reads it back, and its end reaches the next app as `screenRestored` with the state that the earlier app gave.
final class HaylenScreens {
    // A screen as its plugin registered it: the launcher of an Activity Result contract that every activity registers, or an opener that shows the screen itself.
    abstract static class Registration {
        void attach(HaylenActivity activity, String key) {}

        abstract void open(Object params, HaylenScreen screen) throws Exception;
    }

    // A `null` output, which the contracts of AndroidX give when the person backs out, ends the screen with the code `cancelled`.
    static final class ContractScreen<I, O> extends Registration {
        private final ActivityResultContract<I, O> contract;
        private final HaylenScreen.Input<I> input;
        private final HaylenScreen.Output<O> output;
        private ActivityResultLauncher<I> launcher;

        ContractScreen(ActivityResultContract<I, O> contract, HaylenScreen.Input<I> input, HaylenScreen.Output<O> output) {
            this.contract = contract;
            this.input = input;
            this.output = output;
        }

        @Override
        void attach(HaylenActivity activity, String key) {
            launcher = activity.getActivityResultRegistry().register(key, activity, contract, result -> deliver(key, result));
        }

        @Override
        void open(Object params, HaylenScreen screen) throws Exception {
            launcher.launch(input.create(params));
        }

        private void deliver(String key, O result) {
            HaylenScreen screen = find(key);
            if (screen == null) {
                Log.i(TAG, "The screen \"" + key + "\" answered after it ended, so no app receives the answer.");
                return;
            }
            if (result == null) {
                screen.fail("The person closed the screen \"" + screen.name() + "\".", "cancelled", null);
                return;
            }
            try {
                screen.finish(output.convert(result));
            } catch (Exception error) {
                screen.fail(error);
            }
        }
    }

    static final class OpenerScreen extends Registration {
        private final HaylenScreen.Opener opener;

        OpenerScreen(HaylenScreen.Opener opener) {
            this.opener = opener;
        }

        @Override
        void open(Object params, HaylenScreen screen) throws Exception {
            opener.open(params, screen);
        }
    }

    private static final String TAG = "haylen";
    // The key of the screen that shows in the saved state of the activity.
    private static final String SAVED_STATE_KEY = "haylen.screens";

    private static final Handler mainThread = new Handler(Looper.getMainLooper());
    private static final Map<String, Registration> registrations = new ConcurrentHashMap<>();
    // The screen that this process opened, and the one that showed when the earlier process ended, until each ends.
    private static HaylenScreen showing;
    private static HaylenScreen restored;

    private HaylenScreens() {}

    static String key(String plugin, String name) {
        return "haylen." + plugin + "." + name;
    }

    static void register(String plugin, String name, Registration registration) {
        registrations.put(key(plugin, name), registration);
    }

    // Plugin ids have no dots, so the key of a plugin with an empty name starts the keys of every screen of the plugin alone.
    static void unregister(String plugin) {
        registrations.keySet().removeIf(key -> key.startsWith(key(plugin, "")));
    }

    // Called in `onCreate` of every activity, before it starts. A kept screen that this process still shows is the same screen, which only the activity outlived.
    static void activityCreated(HaylenActivity activity) {
        SavedStateRegistry saved = activity.getSavedStateRegistry();
        Bundle kept = saved.consumeRestoredStateForKey(SAVED_STATE_KEY);
        synchronized (HaylenScreens.class) {
            if (kept != null && kept.containsKey("plugin") && showing == null && restored == null) {
                restored = new HaylenScreen(kept.getLong("id"), kept.getString("plugin"), kept.getString("screen"), kept.getString("state"), kept.getBoolean("opaque"), true);
            }
        }
        saved.registerSavedStateProvider(SAVED_STATE_KEY, HaylenScreens::save);
        for (Map.Entry<String, Registration> entry : registrations.entrySet()) {
            entry.getValue().attach(activity, entry.getKey());
        }
    }

    // The screen of the plugin that showed when the earlier process ended, which the plugin ends when its answer arrives, or `null`.
    static synchronized HaylenScreen restoredScreen(String plugin) {
        return restored != null && restored.plugin().equals(plugin) ? restored : null;
    }

    // Called from the frame thread of the engine once it covered the app, which shows the screen on the main thread. A screen that no plugin registered fails with the code `noHandler`, and one that finds the activity away from the foreground with the code `notActive`.
    static void open(long id, byte[] plugin, byte[] name, byte[] params, byte[][] buffers, byte[] state, boolean opaque) {
        HaylenScreen screen = new HaylenScreen(id, text(plugin), text(name), text(state), opaque, false);
        mainThread.post(() -> {
            Registration registration = registrations.get(screen.key());
            if (registration == null) {
                screen.fail("No screen \"" + screen.name() + "\" of the plugin \"" + screen.plugin() + "\" is registered on Android.", "noHandler", null);
                return;
            }
            HaylenActivity activity = HaylenBridge.activity();
            if (activity == null || !activity.getLifecycle().getCurrentState().isAtLeast(Lifecycle.State.RESUMED)) {
                screen.fail("The screen \"" + screen.name() + "\" cannot show while the app is away from the foreground.", "notActive", null);
                return;
            }
            synchronized (HaylenScreens.class) {
                showing = screen;
            }
            try {
                registration.open(HaylenPayload.decode(params, buffers), screen);
            } catch (Exception error) {
                screen.fail(error);
            }
        });
    }

    // Called from the frame thread of the engine when the app gives a screen up. The plugin closes the UI that it shows itself when its cancel listeners run, and a screen without listeners ends with the code `cancelled` at once, which drops the answer that its launcher may still receive.
    static void cancel(long id) {
        mainThread.post(() -> {
            HaylenScreen screen;
            synchronized (HaylenScreens.class) {
                screen = showing != null && showing.id() == id ? showing : null;
            }
            if (screen == null) {
                return;
            }
            boolean listens = screen.hasCancelListeners();
            screen.cancel();
            if (!listens) {
                screen.fail("The app gave the screen \"" + screen.name() + "\" up.", "cancelled", null);
            }
        });
    }

    static synchronized void ended(HaylenScreen screen) {
        if (showing == screen) {
            showing = null;
        }
        if (restored == screen) {
            restored = null;
        }
    }

    private static synchronized HaylenScreen find(String key) {
        if (showing != null && showing.key().equals(key)) {
            return showing;
        }
        return restored != null && restored.key().equals(key) ? restored : null;
    }

    private static synchronized Bundle save() {
        Bundle bundle = new Bundle();
        HaylenScreen screen = showing != null ? showing : restored;
        if (screen != null) {
            bundle.putLong("id", screen.id());
            bundle.putString("plugin", screen.plugin());
            bundle.putString("screen", screen.name());
            bundle.putString("state", screen.state());
            bundle.putBoolean("opaque", screen.isOpaque());
        }
        return bundle;
    }

    private static String text(byte[] bytes) {
        return new String(bytes, StandardCharsets.UTF_8);
    }
}
