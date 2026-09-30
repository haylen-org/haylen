package dev.haylen;

import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import org.json.JSONObject;

// The native part of a plugin on Android. The haylen library creates one instance of every class that a dev.haylen.plugin.<id> meta-data entry of the manifest names when the process starts, calls onLoad with the context of the plugin, and then forwards the events of the activity to it on the main thread, to every plugin in load order.
public abstract class HaylenPlugin {
    // Called once per process, before Application.onCreate finishes. Plugins register their methods here and set up their SDKs. A plugin whose onLoad throws is logged and left out, so the app runs without its native part.
    public void onLoad(HaylenPluginContext context) throws Exception {}

    // Called before the activity starts, which is when it takes the Activity Result launchers of the plugins, registered under stable keys so the results that arrive after a recreation or the end of the process reach them.
    public void onActivityCreated(HaylenActivity activity, Bundle savedInstanceState) {}

    public void onActivityStarted(HaylenActivity activity) {}

    public void onActivityResumed(HaylenActivity activity) {}

    public void onActivityPaused(HaylenActivity activity) {}

    public void onActivityStopped(HaylenActivity activity) {}

    // The activity removes the views that the plugin placed over the app after this returns.
    public void onActivityDestroyed(HaylenActivity activity) {}

    // The activity already holds the new intent, so getIntent returns it too.
    public void onNewIntent(Intent intent) {}

    public void onConfigurationChanged(Configuration configuration) {}

    public void onWindowFocusChanged(boolean hasFocus) {}

    public void onTrimMemory(int level) {}

    // Receives every error that stops the app as {message, file, line, traceback, frames}, where frames lists {source, line, function, kind} from the innermost call outward.
    public void onAppError(JSONObject error) {}
}
