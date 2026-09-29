package dev.haylen;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import org.json.JSONObject;

// The native part of a plugin on Android. The haylen library creates one instance of every class that a dev.haylen.plugin.<id> meta-data entry of the manifest names when the process starts, calls onLoad with the context of the plugin, and then forwards the events of the activity to it on the main thread, to every plugin in load order.
public abstract class HaylenPlugin {
    // Called once per process, before Application.onCreate finishes. Plugins register their methods here and set up their SDKs. A plugin whose onLoad throws is logged and left out, so the app runs without its native part.
    public void onLoad(HaylenPluginContext context) throws Exception {}

    public void onActivityCreated(Activity activity, Bundle savedInstanceState) {}

    public void onActivityStarted(Activity activity) {}

    public void onActivityResumed(Activity activity) {}

    public void onActivityPaused(Activity activity) {}

    public void onActivityStopped(Activity activity) {}

    // The activity removes the views that the plugin placed over the app after this returns.
    public void onActivityDestroyed(Activity activity) {}

    // The activity already holds the new intent, so getIntent returns it too.
    public void onNewIntent(Intent intent) {}

    // Returns whether the plugin handled the result, which ends the search, so a plugin answers only the request codes it started.
    public boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        return false;
    }

    // Returns whether the plugin handled the result, which ends the search, so a plugin answers only the request codes it started.
    public boolean onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        return false;
    }

    public void onConfigurationChanged(Configuration configuration) {}

    public void onWindowFocusChanged(boolean hasFocus) {}

    public void onTrimMemory(int level) {}

    // Receives every error that stops the app as {message, file, line, traceback, frames}, where frames lists {source, line, function, kind} from the innermost call outward.
    public void onAppError(JSONObject error) {}
}
