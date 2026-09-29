package dev.haylen;

import android.app.Activity;
import androidx.core.graphics.Insets;
import java.util.ArrayList;
import java.util.List;

// The panels that plugins place over one activity. They follow the safe area of its window and its lifecycle: they join the window once it has had the focus, hide while the activity is stopped and leave before it is destroyed.
final class HaylenPanels {
    private final Activity activity;
    private final List<HaylenOverlay.Panel> panels = new ArrayList<>();
    private Insets safeArea = Insets.NONE;
    private boolean started;
    private boolean windowReady;

    HaylenPanels(Activity activity) {
        this.activity = activity;
    }

    Activity getActivity() {
        return activity;
    }

    Insets getSafeArea() {
        return safeArea;
    }

    boolean isStarted() {
        return started;
    }

    boolean isWindowReady() {
        return windowReady;
    }

    void add(HaylenOverlay.Panel panel) {
        panels.add(panel);
        panel.refresh();
    }

    void forget(HaylenOverlay.Panel panel) {
        panels.remove(panel);
    }

    void setStarted(boolean value) {
        started = value;
        refresh();
    }

    // A window has a valid token for panels once it has had the focus, which it first gets after the activity resumes.
    void windowFocused() {
        if (!windowReady) {
            windowReady = true;
            refresh();
        }
    }

    // The safe area of the device in the pixels of the activity window, the same one the engine uses.
    void setSafeArea(Insets value) {
        if (!value.equals(safeArea)) {
            safeArea = value;
            refresh();
        }
    }

    // Repositions every panel, such as after a change of the density, whose dp sizes and margins then take other pixels.
    void refresh() {
        for (HaylenOverlay.Panel panel : new ArrayList<>(panels)) {
            panel.refresh();
        }
    }

    // A window still attached when the activity is destroyed would leak, so every panel leaves first.
    void removeAll() {
        for (HaylenOverlay.Panel panel : new ArrayList<>(panels)) {
            panel.remove();
        }
    }

    static native void nativeReserveInsets(byte[] key, int left, int top, int right, int bottom);

    static native void nativeReleaseInsets(byte[] key);
}
