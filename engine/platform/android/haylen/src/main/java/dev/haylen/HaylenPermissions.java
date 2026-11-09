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
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

// Reads and asks for the permissions of `system.permission`: the ones of the engine, `screens` and `localNetwork`, through `state` and `request`, and the ones that plugins register through `HaylenPluginContext`. Each permission is a `HaylenPermission`, a set of permissions of the manifest for the version of Android that runs, which the engine asks for with the prompt of the system through one launcher of the Activity Result API, registered under the key `haylen.permissions` in `onCreate` of every activity, before it starts. Requests wait in a queue and show one at a time. A permission that the app does not hold counts as denied once the engine asked for it and Android no longer shows its prompt, which Android tells by no longer asking for a rationale, and the engine remembers which permissions it asked for across launches.
final class HaylenPermissions {
    // What a check or a request answers: the name of the state, the sentence of an unsupported answer and the requirements the project lacks.
    interface Answer {
        void answer(String state, String message, List<Map<String, Object>> missing);
    }

    // The local network needs a permission only for an app that targets Android 17 on Android 17 or later.
    private static final class LocalNetwork extends HaylenPermission {
        LocalNetwork() {
            super("localNetwork", "the local network", Manifest.permission.ACCESS_LOCAL_NETWORK);
        }

        @Override
        public String[] required(Context context) {
            boolean needed = Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN && context.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.CINNAMON_BUN;
            return needed ? super.required(context) : new String[0];
        }
    }

    private static final String PREFERENCES = "haylen.permissions";
    private static final Handler mainThread = new Handler(Looper.getMainLooper());
    private static final ArrayDeque<Runnable> queue = new ArrayDeque<>();
    private static final List<HaylenPermission> engine = List.of(new HaylenPermission("screens", "every screen"), new LocalNetwork());
    private static ActivityResultLauncher<String[]> launcher;
    private static Runnable settle;

    private HaylenPermissions() {}

    // Called in `onCreate` of every activity, before it starts. A request that showed while the activity was recreated answers through the launcher of the new one.
    static void activityCreated(HaylenActivity activity) {
        launcher = activity.getActivityResultRegistry().register("haylen.permissions", activity, new ActivityResultContracts.RequestMultiplePermissions(), HaylenPermissions::answered);
    }

    // Called from any thread of the engine with the name of a permission of the engine, which hears through `nativeAnswered` with the same request the state, without a prompt. The engine runs only while an activity exists, and without one the state stays unknown.
    static void state(long request, byte[] kind) {
        HaylenPermission permission = ofEngine(new String(kind, StandardCharsets.UTF_8));
        mainThread.post(() -> {
            HaylenActivity activity = HaylenBridge.activity();
            if (activity == null) {
                nativeAnswered(request, encode("unknown", "", Collections.emptyList()));
                return;
            }
            check(permission, activity, activity.requirements(), (state, message, missing) -> nativeAnswered(request, encode(state, message, missing)));
        });
    }

    // Called from any thread of the engine, which hears through `nativeAnswered` with the same request what the person decided, or what the manifest of the app lacks.
    static void request(long request, byte[] kind) {
        HaylenPermission permission = ofEngine(new String(kind, StandardCharsets.UTF_8));
        mainThread.post(() -> {
            HaylenActivity activity = HaylenBridge.activity();
            if (activity == null) {
                nativeAnswered(request, encode("unknown", "", Collections.emptyList()));
                return;
            }
            ask(permission, activity, activity.requirements(), (state, message, missing) -> nativeAnswered(request, encode(state, message, missing)));
        });
    }

    // Called from any thread of the engine, such as a thread of the HTTP client whose request failed, with the name of a permission of the engine, and answers at once whether the app holds it, without a prompt. Without an activity the engine runs nothing that needs it.
    static boolean isHeld(byte[] kind) {
        HaylenActivity activity = HaylenBridge.activity();
        return activity == null || ofEngine(new String(kind, StandardCharsets.UTF_8)).isGranted(activity);
    }

    // Runs on the main thread. A permission that needs no prompt on this version of Android answers its state at once.
    static void check(HaylenPermission permission, Context context, HaylenRequirements requirements, Answer answer) {
        String[] required = permission.required(context);
        if (required.length == 0) {
            answer.answer(permission.stateWithoutPrompt(context), "", Collections.emptyList());
            return;
        }
        if (permission.isGranted(context)) {
            answer.answer("granted", "", Collections.emptyList());
            return;
        }
        if (!reportMissing(permission, context, requirements, answer)) {
            answer.answer(isRefused(permission, required, context) ? "denied" : "prompt", "", Collections.emptyList());
        }
    }

    // Runs on the main thread. A permission with a prerequisite that the app does not hold asks for the prerequisite first, which Android shows in a prompt of its own.
    static void ask(HaylenPermission permission, Context context, HaylenRequirements requirements, Answer answer) {
        if (permission.required(context).length == 0 || permission.isGranted(context)) {
            check(permission, context, requirements, answer);
            return;
        }
        if (reportMissing(permission, context, requirements, answer)) {
            return;
        }

        HaylenPermission prerequisite = permission.prerequisite();
        if (prerequisite != null && !prerequisite.isGranted(context)) {
            ask(prerequisite, context, requirements, (state, message, missing) -> afterPrerequisite(permission, context, requirements, answer, state, message, missing));
            return;
        }

        String[] asked = permission.requested(context, requirements);
        queue.add(() -> show(permission, asked, answer));
        if (queue.size() == 1) {
            queue.peek().run();
        }
    }

    // The sentence of a use of the capability that the person refused, the same on every platform.
    static String refusal(HaylenPermission permission) {
        return "The person has not allowed " + permission.getCapability() + ". Ask for it with \"system.requestPermission('" + permission.getName() + "')\", which shows the prompt of the system while the system still asks, or the person allows it in the settings of the system.";
    }

    // The JSON object of `Permission::Answer::fromJson`, which the handlers of the permissions of plugins answer with.
    static JSONObject toJson(String state, String message, List<Map<String, Object>> missing) {
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
        return answer;
    }

    private static byte[] encode(String state, String message, List<Map<String, Object>> missing) {
        return toJson(state, message, missing).toString().getBytes(StandardCharsets.UTF_8);
    }

    private static HaylenPermission ofEngine(String name) {
        for (HaylenPermission permission : engine) {
            if (permission.getName().equals(name)) {
                return permission;
            }
        }
        throw new IllegalArgumentException("The permission \"" + name + "\" is not one of the engine.");
    }

    private static void afterPrerequisite(HaylenPermission permission, Context context, HaylenRequirements requirements, Answer answer, String state, String message, List<Map<String, Object>> missing) {
        if (state.equals("granted")) {
            ask(permission, context, requirements, answer);
            return;
        }
        answer.answer(state.equals("unsupported") ? state : "denied", message, missing);
    }

    private static void show(HaylenPermission permission, String[] asked, Answer answer) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null || launcher == null) {
            answer.answer("unknown", "", Collections.emptyList());
            next();
            return;
        }
        activity.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE).edit().putBoolean(permission.getName(), true).apply();
        settle = () -> answer.answer(permission.isGranted(activity) ? "granted" : "denied", "", Collections.emptyList());
        launcher.launch(asked);
    }

    // A request that ends after the process ended reaches the launcher of the new activity, while no request waits for it. The answer reads what the app holds, since the person may grant less than the prompt asked for, such as the approximate location for a request of the precise one.
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

    // Only an activity tells whether Android still shows the prompt, so without one a permission the engine asked for counts as refused.
    private static boolean isRefused(HaylenPermission permission, String[] required, Context context) {
        if (!context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE).getBoolean(permission.getName(), false)) {
            return false;
        }
        HaylenActivity activity = HaylenBridge.activity();
        for (String name : required) {
            if (context.checkSelfPermission(name) != PackageManager.PERMISSION_GRANTED && (activity == null || !activity.shouldShowRequestPermissionRationale(name))) {
                return true;
            }
        }
        return false;
    }

    // A permission that the manifest does not declare is refused without a prompt, so the answer names the lines that declare it, which the log tells once.
    private static boolean reportMissing(HaylenPermission permission, Context context, HaylenRequirements requirements, Answer answer) {
        List<HaylenRequirements.Requirement> missing = new ArrayList<>();
        for (String name : permission.required(context)) {
            if (!requirements.hasPermission(name)) {
                missing.add(HaylenRequirements.Requirement.permission(name));
            }
        }
        if (missing.isEmpty()) {
            return false;
        }

        List<String> descriptions = new ArrayList<>();
        List<String> instructions = new ArrayList<>();
        List<Map<String, Object>> entries = new ArrayList<>();
        for (HaylenRequirements.Requirement requirement : missing) {
            requirements.report(requirement, Log.WARN, "\"system.requestPermission('" + permission.getName() + "')\" fails with the code \"unsupported\"");
            descriptions.add(requirement.description());
            instructions.add(requirement.instructions());
            entries.add(requirement.toMap());
        }
        String list = descriptions.size() == 1 ? descriptions.get(0) : String.join(", ", descriptions.subList(0, descriptions.size() - 1)) + " and " + descriptions.get(descriptions.size() - 1);
        answer.answer("unsupported", requirements.owner() + " needs " + list + ", which the app lacks. " + String.join(" ", instructions), entries);
        return true;
    }

    private static native void nativeAnswered(long request, byte[] answer);
}
