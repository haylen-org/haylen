package dev.haylen;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

// Reads and asks for the permissions that the person grants the app while it runs, which `haylen.system` names. The local network is one from Android 17 for an app that targets it, so the engine asks for it with the prompt of the system through a launcher of the Activity Result API, and an older system or target answers that the app holds it. The launcher is registered in `onCreate` of every activity, before it starts, and answers every request that waits for it at once.
final class HaylenPermissions {
    private static final String LOCAL_NETWORK = "localNetwork";
    private static final Handler mainThread = new Handler(Looper.getMainLooper());
    private static final List<Long> waiting = new ArrayList<>();
    private static ActivityResultLauncher<String> localNetwork;
    private static volatile boolean refused;

    private HaylenPermissions() {}

    // Called in `onCreate` of every activity, before it starts.
    static void activityCreated(HaylenActivity activity) {
        localNetwork = activity.getActivityResultRegistry().register("haylen.permissions.localNetwork", activity, new ActivityResultContracts.RequestPermission(), HaylenPermissions::answered);
    }

    // Called from any thread of the engine with the name of a permission, and answers the name of its state.
    static byte[] state(byte[] kind) {
        String name = new String(kind, StandardCharsets.UTF_8);
        HaylenActivity activity = HaylenBridge.activity();
        if (!name.equals(LOCAL_NETWORK) || activity == null) {
            return "unknown".getBytes(StandardCharsets.UTF_8);
        }
        if (isGranted(activity)) {
            return "granted".getBytes(StandardCharsets.UTF_8);
        }
        // A person who refused the prompt and keeps refusing it has Android answer every later request without one, which it tells by no longer asking for a rationale.
        boolean denied = refused && !activity.shouldShowRequestPermissionRationale(Manifest.permission.ACCESS_LOCAL_NETWORK);
        return (denied ? "denied" : "prompt").getBytes(StandardCharsets.UTF_8);
    }

    // Called from any thread of the engine, which hears through `nativeAnswered` with the same request what the person decided, or what the manifest of the app lacks.
    static void request(long request, byte[] kind) {
        String name = new String(kind, StandardCharsets.UTF_8);
        mainThread.post(() -> ask(request, name));
    }

    private static void ask(long request, String name) {
        HaylenActivity activity = HaylenBridge.activity();
        if (!name.equals(LOCAL_NETWORK) || activity == null || localNetwork == null) {
            answer(request, "unknown", "The app has no activity to ask the person for the permission.", Collections.emptyList());
            return;
        }
        if (isGranted(activity)) {
            answer(request, "granted", "", Collections.emptyList());
            return;
        }

        // A permission the manifest does not declare is refused without a prompt, so the request names the line that declares it instead.
        HaylenRequirements.Requirement requirement = HaylenRequirements.Requirement.permission(Manifest.permission.ACCESS_LOCAL_NETWORK);
        if (!activity.requirements().hasPermission(requirement.name())) {
            activity.requirements().report(requirement, Log.WARN, "\"system.requestPermission('localNetwork')\" fails with the code \"unsupported\" and the app never reaches the local network");
            answer(request, "denied", "The engine needs " + requirement.description() + ", which the app lacks. " + requirement.instructions(), Collections.singletonList(requirement.toMap()));
            return;
        }

        waiting.add(request);
        if (waiting.size() == 1) {
            localNetwork.launch(Manifest.permission.ACCESS_LOCAL_NETWORK);
        }
    }

    private static void answered(boolean granted) {
        refused = !granted;
        List<Long> requests = new ArrayList<>(waiting);
        waiting.clear();
        for (long request : requests) {
            answer(request, granted ? "granted" : "denied", "", Collections.emptyList());
        }
    }

    // Only an app that targets Android 17 on Android 17 or later needs the permission, which a person may grant and take back while the app runs.
    private static boolean isGranted(HaylenActivity activity) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.CINNAMON_BUN || activity.getApplicationInfo().targetSdkVersion < Build.VERSION_CODES.CINNAMON_BUN) {
            return true;
        }
        return activity.checkSelfPermission(Manifest.permission.ACCESS_LOCAL_NETWORK) == PackageManager.PERMISSION_GRANTED;
    }

    private static void answer(long request, String state, String message, List<Map<String, Object>> missing) {
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
            throw new IllegalStateException("The answer of a permission request could not be written as JSON.", error);
        }
        nativeAnswered(request, answer.toString().getBytes(StandardCharsets.UTF_8));
    }

    private static native void nativeAnswered(long request, byte[] answer);
}
