package dev.haylen;

import android.app.Activity;
import android.graphics.Rect;
import androidx.core.content.ContextCompat;
import androidx.core.util.Consumer;
import androidx.window.java.layout.WindowInfoTrackerCallbackAdapter;
import androidx.window.layout.DisplayFeature;
import androidx.window.layout.FoldingFeature;
import androidx.window.layout.WindowInfoTracker;
import androidx.window.layout.WindowLayoutInfo;

// Tells the engine the fold of a foldable or dual-screen device through Jetpack WindowManager, which reports the layout of the window when the activity starts following it and whenever it changes, such as when the device folds or unfolds or the window moves across the hinge.
final class HaylenFold implements Consumer<WindowLayoutInfo> {
    private final Activity activity;
    private final WindowInfoTrackerCallbackAdapter tracker;

    HaylenFold(Activity activity) {
        this.activity = activity;
        tracker = new WindowInfoTrackerCallbackAdapter(WindowInfoTracker.getOrCreate(activity));
    }

    void register() {
        tracker.addWindowLayoutInfoListener(activity, ContextCompat.getMainExecutor(activity), this);
    }

    void unregister() {
        tracker.removeWindowLayoutInfoListener(this);
    }

    // The bounds are in the coordinates of the window, which the surface of the app fills from edge to edge, so they are framebuffer pixels. A window that no fold crosses has none.
    @Override
    public void accept(WindowLayoutInfo info) {
        for (DisplayFeature feature : info.getDisplayFeatures()) {
            if (feature instanceof FoldingFeature fold) {
                Rect bounds = fold.getBounds();
                nativeFold(true, bounds.left, bounds.top, bounds.right, bounds.bottom, fold.getOrientation() == FoldingFeature.Orientation.VERTICAL, fold.getState() == FoldingFeature.State.HALF_OPENED, fold.isSeparating(), fold.getOcclusionType() == FoldingFeature.OcclusionType.FULL);
                return;
            }
        }
        nativeFold(false, 0, 0, 0, 0, false, false, false, false);
    }

    private static native void nativeFold(boolean present, int left, int top, int right, int bottom, boolean vertical, boolean halfOpened, boolean separating, boolean occluding);
}
