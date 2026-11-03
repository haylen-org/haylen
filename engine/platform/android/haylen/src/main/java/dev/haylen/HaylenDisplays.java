package dev.haylen;

import android.app.Activity;
import android.content.Context;
import android.graphics.Insets;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.Surface;
import android.view.WindowInsets;
import java.nio.charset.StandardCharsets;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

// Tells the engine the displays of the device through the display manager, which reports when a display connects, disconnects or changes, such as an external display, a cast screen or a secondary screen, which display shows the activity, and how far that display turned from its natural orientation, which the motion sensors follow. Each display has its own points from its top left corner, in density-independent pixels.
final class HaylenDisplays implements DisplayManager.DisplayListener {
    private final Activity activity;
    private final DisplayManager manager;

    HaylenDisplays(Activity activity) {
        this.activity = activity;
        manager = activity.getSystemService(DisplayManager.class);
    }

    // The engine reads the displays once the window of the activity exists, which follows this first report.
    void register() {
        manager.registerDisplayListener(this, new Handler(Looper.getMainLooper()));
        report();
    }

    void unregister() {
        manager.unregisterDisplayListener(this);
    }

    @Override
    public void onDisplayAdded(int displayId) {
        report();
    }

    @Override
    public void onDisplayRemoved(int displayId) {
        report();
    }

    @Override
    public void onDisplayChanged(int displayId) {
        report();
    }

    @SuppressWarnings("deprecation")
    private Display getActivityDisplay() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R ? activity.getDisplay() : activity.getWindowManager().getDefaultDisplay();
    }

    void report() {
        try {
            JSONArray list = new JSONArray();
            for (Display display : manager.getDisplays()) {
                list.put(describe(display));
            }
            nativeDisplays(list.toString().getBytes(StandardCharsets.UTF_8), getActivityDisplay().getDisplayId());
            nativeRotation(getActivityDisplay().getRotation());
        } catch (JSONException exception) {
            throw new IllegalStateException("The displays could not be described.", exception);
        }
    }

    // The modes of a display give its pixels in its natural orientation, which a quarter turn swaps. The work area of the display of the activity leaves out the bars of the system where Android reports them.
    private JSONObject describe(Display display) throws JSONException {
        Context context = activity.createDisplayContext(display);
        float density = context.getResources().getDisplayMetrics().density;
        Display.Mode current = display.getMode();
        boolean turned = display.getRotation() == Surface.ROTATION_90 || display.getRotation() == Surface.ROTATION_270;
        float width = (turned ? current.getPhysicalHeight() : current.getPhysicalWidth()) / density;
        float height = (turned ? current.getPhysicalWidth() : current.getPhysicalHeight()) / density;
        float left = 0;
        float top = 0;
        float right = 0;
        float bottom = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && display.getDisplayId() == getActivityDisplay().getDisplayId()) {
            Insets bars = activity.getWindowManager().getMaximumWindowMetrics().getWindowInsets().getInsets(WindowInsets.Type.systemBars());
            left = bars.left / density;
            top = bars.top / density;
            right = bars.right / density;
            bottom = bars.bottom / density;
        }

        JSONArray modes = new JSONArray();
        for (Display.Mode mode : display.getSupportedModes()) {
            modes.put(new JSONArray().put(mode.getPhysicalWidth()).put(mode.getPhysicalHeight()).put(mode.getRefreshRate()));
        }
        return new JSONObject()
            .put("id", String.valueOf(display.getDisplayId()))
            .put("name", display.getName())
            .put("width", width)
            .put("height", height)
            .put("workArea", new JSONArray().put(left).put(top).put(width - left - right).put(height - top - bottom))
            .put("scale", density)
            .put("refreshRate", display.getRefreshRate())
            .put("primary", display.getDisplayId() == Display.DEFAULT_DISPLAY)
            .put("modes", modes);
    }

    private static native void nativeDisplays(byte[] json, int windowDisplay);

    private static native void nativeRotation(int quarterTurns);
}
