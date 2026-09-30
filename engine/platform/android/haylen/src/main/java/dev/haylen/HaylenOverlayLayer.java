package dev.haylen;

import android.widget.FrameLayout;
import androidx.core.graphics.Insets;
import java.util.ArrayList;
import java.util.List;

// The views that plugins place over the app in one activity, which live in the layout of its surface, above it. They follow the safe area of the window and leave with the activity.
final class HaylenOverlayLayer {
    private final HaylenActivity activity;
    private final FrameLayout content;
    private final List<HaylenOverlay.Panel> panels = new ArrayList<>();
    private Insets safeArea = Insets.NONE;

    HaylenOverlayLayer(HaylenActivity activity, FrameLayout content) {
        this.activity = activity;
        this.content = content;
    }

    HaylenActivity getActivity() {
        return activity;
    }

    FrameLayout getContent() {
        return content;
    }

    Insets getSafeArea() {
        return safeArea;
    }

    void add(HaylenOverlay.Panel panel) {
        panels.add(panel);
        panel.refresh();
    }

    void forget(HaylenOverlay.Panel panel) {
        panels.remove(panel);
    }

    // The safe area of the device in the pixels of the activity window, the same one the engine uses.
    void setSafeArea(Insets value) {
        if (!value.equals(safeArea)) {
            safeArea = value;
            refresh();
        }
    }

    // Places every view again, such as after a change of the density, whose dp sizes and margins then take other pixels.
    void refresh() {
        for (HaylenOverlay.Panel panel : new ArrayList<>(panels)) {
            panel.refresh();
        }
    }

    // The edges that the views reserve go back to the app before the activity goes away.
    void removeAll() {
        for (HaylenOverlay.Panel panel : new ArrayList<>(panels)) {
            panel.remove();
        }
    }

    static native void nativeReserveInsets(byte[] key, int left, int top, int right, int bottom);

    static native void nativeReleaseInsets(byte[] key);
}
