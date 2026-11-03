package dev.haylen;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.core.app.NotificationManagerCompat;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

// Reads and asks for the permissions that `haylen.system` names, the engine through `state` and `request` and the plugins through `HaylenPluginContext.requirePermission`. Each permission is a set of permissions of the manifest for the version of Android that runs, which the engine asks for with the prompt of the system through one launcher of the Activity Result API, registered under the key `haylen.permissions` in `onCreate` of every activity, before it starts. Requests wait in a queue and show one at a time. A permission that the app does not hold counts as denied once the engine asked for it and Android no longer shows its prompt, which Android tells by no longer asking for a rationale, and the engine remembers which permissions of `haylen.system` it asked for across launches. A person who allowed the approximate location keeps the precise one to ask for, which Android offers in a prompt of its own.
final class HaylenPermissions {
    // What a check or a request answers: the name of the state, the sentence of an unsupported answer and the requirements the project lacks.
    interface Answer {
        void answer(String state, String message, List<Map<String, Object>> missing);
    }

    private static final String PREFERENCES = "haylen.permissions";
    private static final Handler mainThread = new Handler(Looper.getMainLooper());
    private static final ArrayDeque<Runnable> queue = new ArrayDeque<>();
    private static ActivityResultLauncher<String[]> launcher;
    private static Runnable settle;

    private HaylenPermissions() {}

    // Called in `onCreate` of every activity, before it starts. A request that showed while the activity was recreated answers through the launcher of the new one.
    static void activityCreated(HaylenActivity activity) {
        launcher = activity.getActivityResultRegistry().register("haylen.permissions", activity, new ActivityResultContracts.RequestMultiplePermissions(), HaylenPermissions::answered);
    }

    // Called from any thread of the engine with the name of a permission, which hears through `nativeAnswered` with the same request the state, without a prompt. The engine runs only while an activity exists, and without one the state stays unknown.
    static void state(long request, byte[] kind) {
        String name = new String(kind, StandardCharsets.UTF_8);
        mainThread.post(() -> {
            HaylenActivity activity = HaylenBridge.activity();
            if (activity == null) {
                nativeAnswered(request, encode("unknown", "", Collections.emptyList()));
                return;
            }
            check(name, activity, activity.requirements(), (state, message, missing) -> nativeAnswered(request, encode(state, message, missing)));
        });
    }

    // Called from any thread of the engine, which hears through `nativeAnswered` with the same request what the person decided, or what the manifest of the app lacks.
    static void request(long request, byte[] kind) {
        String name = new String(kind, StandardCharsets.UTF_8);
        mainThread.post(() -> {
            HaylenActivity activity = HaylenBridge.activity();
            if (activity == null) {
                nativeAnswered(request, encode("unknown", "", Collections.emptyList()));
                return;
            }
            ask(name, activity, activity.requirements(), (state, message, missing) -> nativeAnswered(request, encode(state, message, missing)));
        });
    }

    // Called from any thread of the engine, such as a thread of the HTTP client whose request failed, with the name of a permission, and answers at once whether the app holds it, without a prompt. Without an activity the engine runs nothing that needs it.
    static boolean isHeld(byte[] kind) {
        HaylenActivity activity = HaylenBridge.activity();
        return activity == null || isGranted(new String(kind, StandardCharsets.UTF_8), activity);
    }

    // Runs on the main thread. A permission that needs no prompt on this version of Android answers its state at once.
    static void check(String name, Context context, HaylenRequirements requirements, Answer answer) {
        String[] needed = needed(name, context);
        if (needed.length == 0) {
            answer.answer(heldWithoutPrompt(name, context), "", Collections.emptyList());
            return;
        }
        if (isGranted(name, context)) {
            answer.answer("granted", "", Collections.emptyList());
            return;
        }
        if (!reportMissing(name, context, requirements, answer)) {
            answer.answer(isRefused(name, needed, context) ? "denied" : "prompt", "", Collections.emptyList());
        }
    }

    // Runs on the main thread. The location in the background needs the location while the app is in use first, which Android asks for in a prompt of its own.
    static void ask(String name, Context context, HaylenRequirements requirements, Answer answer) {
        if (needed(name, context).length == 0 || isGranted(name, context)) {
            check(name, context, requirements, answer);
            return;
        }
        if (reportMissing(name, context, requirements, answer)) {
            return;
        }
        if (name.equals("backgroundLocation") && !isGranted("location", context)) {
            ask("location", context, requirements, (state, message, missing) -> {
                if (state.equals("granted")) {
                    ask(name, context, requirements, answer);
                } else {
                    answer.answer(state.equals("unsupported") ? state : "denied", message, missing);
                }
            });
            return;
        }

        String[] asked = requested(name, context, requirements);
        queue.add(() -> show(name, asked, answer));
        if (queue.size() == 1) {
            queue.peek().run();
        }
    }

    // The sentence of a use of the capability that the person refused, the same on every platform.
    static String refusal(String name) {
        return "The person has not allowed " + describe(name) + ". Ask for it with \"system.requestPermission('" + name + "')\", which shows the prompt of the system while the system still asks, or the person allows it in the settings of the system.";
    }

    // Whether the name is one of the permissions of `haylen.system`.
    static boolean isKnown(String name) {
        return !describe(name).isEmpty();
    }

    private static void show(String name, String[] asked, Answer answer) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null || launcher == null) {
            answer.answer("unknown", "", Collections.emptyList());
            next();
            return;
        }
        activity.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE).edit().putBoolean(name, true).apply();
        settle = () -> answer.answer(isGranted(name, activity) ? "granted" : "denied", "", Collections.emptyList());
        launcher.launch(asked);
    }

    // A request that ends after the process ended reaches the launcher of the new activity, while no request waits for it. The answer reads what the app holds, since the person may grant the approximate location for a request of the precise one.
    private static void answered(Map<String, Boolean> results) {
        Runnable waiting = settle;
        if (waiting == null) {
            return;
        }
        settle = null;
        waiting.run();
        next();
    }

    private static void next() {
        queue.poll();
        Runnable following = queue.peek();
        if (following != null) {
            following.run();
        }
    }

    // The permissions of the manifest that the kind needs on this version of Android, none where Android asks for nothing.
    private static String[] needed(String name, Context context) {
        int version = Build.VERSION.SDK_INT;
        return switch (name) {
            case "camera" -> new String[] {Manifest.permission.CAMERA};
            case "microphone" -> new String[] {Manifest.permission.RECORD_AUDIO};
            case "location" -> new String[] {Manifest.permission.ACCESS_COARSE_LOCATION};
            case "preciseLocation" -> new String[] {Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION};
            case "backgroundLocation" -> version >= Build.VERSION_CODES.Q ? new String[] {Manifest.permission.ACCESS_BACKGROUND_LOCATION} : new String[] {Manifest.permission.ACCESS_COARSE_LOCATION};
            case "notifications" -> version >= Build.VERSION_CODES.TIRAMISU ? new String[] {Manifest.permission.POST_NOTIFICATIONS} : new String[0];
            case "photos" -> version >= Build.VERSION_CODES.TIRAMISU ? new String[] {Manifest.permission.READ_MEDIA_IMAGES, Manifest.permission.READ_MEDIA_VIDEO} : new String[] {Manifest.permission.READ_EXTERNAL_STORAGE};
            case "localNetwork" -> needsLocalNetwork(context) ? new String[] {Manifest.permission.ACCESS_LOCAL_NETWORK} : new String[0];
            case "clipboard", "motion", "screens" -> new String[0];
            default -> throw new IllegalArgumentException("The permission \"" + name + "\" is not one of \"haylen.system\".");
        };
    }

    // What a request shows: the needed permissions, with the precise location next to the approximate one and the photos the person picks next to the whole library where the manifest declares them, so the prompt offers the choice.
    private static String[] requested(String name, Context context, HaylenRequirements requirements) {
        List<String> asked = new ArrayList<>();
        Collections.addAll(asked, needed(name, context));
        if (name.equals("location") && requirements.hasPermission(Manifest.permission.ACCESS_FINE_LOCATION)) {
            asked.add(Manifest.permission.ACCESS_FINE_LOCATION);
        }
        if (name.equals("photos") && Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE && requirements.hasPermission(Manifest.permission.READ_MEDIA_VISUAL_USER_SELECTED)) {
            asked.add(Manifest.permission.READ_MEDIA_VISUAL_USER_SELECTED);
        }
        return asked.toArray(new String[0]);
    }

    // The location counts at any precision, and the photos that the person picks count as the photos from Android 14.
    private static boolean isGranted(String name, Context context) {
        return switch (name) {
            case "location" -> holds(context, Manifest.permission.ACCESS_COARSE_LOCATION) || holds(context, Manifest.permission.ACCESS_FINE_LOCATION);
            case "preciseLocation" -> holds(context, Manifest.permission.ACCESS_FINE_LOCATION);
            case "photos" -> holdsAll(context, needed(name, context)) || (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE && holds(context, Manifest.permission.READ_MEDIA_VISUAL_USER_SELECTED));
            default -> holdsAll(context, needed(name, context));
        };
    }

    // Before Android 13 notifications need no permission, while the person may still turn them off in the settings, which no prompt turns on again.
    private static String heldWithoutPrompt(String name, Context context) {
        if (name.equals("notifications")) {
            return NotificationManagerCompat.from(context).areNotificationsEnabled() ? "granted" : "denied";
        }
        return "granted";
    }

    // Only an activity tells whether Android still shows the prompt, so without one a permission the engine asked for counts as refused.
    private static boolean isRefused(String name, String[] needed, Context context) {
        if (!context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE).getBoolean(name, false)) {
            return false;
        }
        HaylenActivity activity = HaylenBridge.activity();
        for (String permission : needed) {
            if (!holds(context, permission) && (activity == null || !activity.shouldShowRequestPermissionRationale(permission))) {
                return true;
            }
        }
        return false;
    }

    // A permission that the manifest does not declare is refused without a prompt, so the answer names the lines that declare it, which the log tells once.
    private static boolean reportMissing(String name, Context context, HaylenRequirements requirements, Answer answer) {
        List<HaylenRequirements.Requirement> missing = new ArrayList<>();
        for (String permission : needed(name, context)) {
            if (!requirements.hasPermission(permission)) {
                missing.add(HaylenRequirements.Requirement.permission(permission));
            }
        }
        if (missing.isEmpty()) {
            return false;
        }

        List<String> descriptions = new ArrayList<>();
        List<String> instructions = new ArrayList<>();
        List<Map<String, Object>> entries = new ArrayList<>();
        for (HaylenRequirements.Requirement requirement : missing) {
            requirements.report(requirement, Log.WARN, "\"system.requestPermission('" + name + "')\" fails with the code \"unsupported\"");
            descriptions.add(requirement.description());
            instructions.add(requirement.instructions());
            entries.add(requirement.toMap());
        }
        String list = descriptions.size() == 1 ? descriptions.get(0) : String.join(", ", descriptions.subList(0, descriptions.size() - 1)) + " and " + descriptions.get(descriptions.size() - 1);
        answer.answer("unsupported", requirements.owner() + " needs " + list + ", which the app lacks. " + String.join(" ", instructions), entries);
        return true;
    }

    private static String describe(String name) {
        return switch (name) {
            case "camera" -> "the camera";
            case "microphone" -> "the microphone";
            case "location" -> "the location";
            case "preciseLocation" -> "the precise location";
            case "backgroundLocation" -> "the location in the background";
            case "notifications" -> "notifications";
            case "photos" -> "the photos";
            case "clipboard" -> "the clipboard";
            case "motion" -> "the motion sensors";
            case "screens" -> "every screen";
            case "localNetwork" -> "the local network";
            default -> "";
        };
    }

    // Only an app that targets Android 17 on Android 17 or later needs the permission for the local network.
    private static boolean needsLocalNetwork(Context context) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN && context.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.CINNAMON_BUN;
    }

    private static boolean holds(Context context, String permission) {
        return context.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED;
    }

    private static boolean holdsAll(Context context, String[] permissions) {
        for (String permission : permissions) {
            if (!holds(context, permission)) {
                return false;
            }
        }
        return true;
    }

    private static byte[] encode(String state, String message, List<Map<String, Object>> missing) {
        JSONObject answer = new JSONObject();
        try {
            answer.put("state", state);
            answer.put("message", message);
            JSONArray list = new JSONArray();
            for (Map<String, Object> requirement : missing) {
                list.put(new JSONObject(requirement));
            }
            answer.put("missing", list);
        } catch (JSONException error) {
            throw new IllegalStateException("The answer of a permission could not be written as JSON.", error);
        }
        return answer.toString().getBytes(StandardCharsets.UTF_8);
    }

    private static native void nativeAnswered(long request, byte[] answer);
}
