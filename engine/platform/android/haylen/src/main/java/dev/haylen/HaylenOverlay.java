package dev.haylen;

import android.graphics.Rect;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import androidx.core.graphics.Insets;
import java.nio.charset.StandardCharsets;

// Places native views of a plugin over the app, such as banner ads or forms. The views join the layout of the surface of the app, above it, so they draw, take the touches inside their frame and may take the focus and the keyboard, such as the fields of a form, while every other touch reaches the app. They follow the safe area, rotations and size changes, and end with the activity, so a plugin places its views again for a new activity. Every method runs on the main thread.
public final class HaylenOverlay {
    private final String id;
    private int serial;

    HaylenOverlay(String id) {
        this.id = id;
    }

    // Places the view over the app and returns its panel.
    public Panel add(View view, HaylenPlacement placement) {
        requireMainThread();
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            throw new IllegalStateException("The plugin " + id + " can only place a view over the app while an activity exists.");
        }
        HaylenOverlayLayer layer = activity.overlays();
        Panel panel = new Panel(layer, id + "#" + ++serial, view, placement.copy());
        layer.add(panel);
        return panel;
    }

    static void requireMainThread() {
        if (Looper.myLooper() != Looper.getMainLooper()) {
            throw new IllegalStateException("The overlay places views on the main thread only.");
        }
    }

    // A view over the app. It reserves the edge of its anchor while it shows, when its placement asks for it.
    public static final class Panel {
        private final HaylenOverlayLayer owner;
        private final byte[] key;
        private final View view;
        private final int viewWidth;
        private final int viewHeight;
        private final View.OnLayoutChangeListener layoutListener = this::onLayout;
        private HaylenPlacement placement;
        private boolean visible = true;
        private boolean removed;
        private int width;
        private int height;
        private Insets reserved;

        Panel(HaylenOverlayLayer owner, String key, View view, HaylenPlacement placement) {
            this.owner = owner;
            this.key = key.getBytes(StandardCharsets.UTF_8);
            this.view = view;
            this.placement = placement;
            // A size that the placement leaves to the view is the size of its own layout parameters, or its measured size without them.
            ViewGroup.LayoutParams own = view.getLayoutParams();
            viewWidth = own != null ? own.width : ViewGroup.LayoutParams.WRAP_CONTENT;
            viewHeight = own != null ? own.height : ViewGroup.LayoutParams.WRAP_CONTENT;
            view.addOnLayoutChangeListener(layoutListener);
            owner.getContent().addView(view, new FrameLayout.LayoutParams(viewWidth, viewHeight));
        }

        // Moves or resizes the view.
        public void update(HaylenPlacement value) {
            requireMainThread();
            placement = value.copy();
            refresh();
        }

        // Hides or shows the view. A hidden view reserves nothing.
        public void setVisible(boolean value) {
            requireMainThread();
            visible = value;
            refresh();
        }

        // Takes the view off the app for good and gives its edge back, so the plugin may place the view again.
        public void remove() {
            requireMainThread();
            if (removed) {
                return;
            }
            removed = true;
            view.removeOnLayoutChangeListener(layoutListener);
            owner.getContent().removeView(view);
            reserve();
            owner.forget(this);
        }

        // The frame of the view in the pixels of the activity window, or null while it does not show.
        public Rect bounds() {
            requireMainThread();
            if (removed || view.getVisibility() != View.VISIBLE || !view.isLaidOut()) {
                return null;
            }
            int[] location = new int[2];
            view.getLocationInWindow(location);
            return new Rect(location[0], location[1], location[0] + view.getWidth(), location[1] + view.getHeight());
        }

        // Brings the view in line with its placement and the safe area.
        void refresh() {
            if (removed) {
                return;
            }
            float density = owner.getActivity().getResources().getDisplayMetrics().density;
            int viewWidthPixels = placement.widthDp == HaylenPlacement.MEASURED ? viewWidth : Math.round(placement.widthDp * density);
            int viewHeightPixels = placement.heightDp == HaylenPlacement.MEASURED ? viewHeight : Math.round(placement.heightDp * density);
            view.setLayoutParams(place(viewWidthPixels, viewHeightPixels, density));
            view.setVisibility(visible ? View.VISIBLE : View.GONE);
            reserve();
        }

        // The layout places the view by gravity and keeps it away from the anchored edges with its margins, while it moves a centered view by the difference of its margins, so a centered axis puts its offset into one of them.
        private FrameLayout.LayoutParams place(int viewWidthPixels, int viewHeightPixels, float density) {
            Insets insets = placement.insideSafeArea ? owner.getSafeArea() : Insets.NONE;
            int margin = Math.round(placement.marginDp * density);
            int horizontal = placement.anchor.horizontal;
            int vertical = placement.anchor.vertical;
            int gravity = (horizontal < 0 ? Gravity.LEFT : horizontal > 0 ? Gravity.RIGHT : Gravity.CENTER_HORIZONTAL) | (vertical < 0 ? Gravity.TOP : vertical > 0 ? Gravity.BOTTOM : Gravity.CENTER_VERTICAL);
            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(viewWidthPixels, viewHeightPixels, gravity);
            int x = horizontal < 0 ? insets.left + margin : horizontal > 0 ? insets.right + margin : (insets.left - insets.right) / 2;
            int y = vertical < 0 ? insets.top + margin : vertical > 0 ? insets.bottom + margin : (insets.top - insets.bottom) / 2;
            params.leftMargin = horizontal < 0 ? x : horizontal == 0 ? Math.max(x, 0) : 0;
            params.rightMargin = horizontal > 0 ? x : horizontal == 0 ? Math.max(-x, 0) : 0;
            params.topMargin = vertical < 0 ? y : vertical == 0 ? Math.max(y, 0) : 0;
            params.bottomMargin = vertical > 0 ? y : vertical == 0 ? Math.max(-y, 0) : 0;
            return params;
        }

        private void onLayout(View laidOut, int left, int top, int right, int bottom, int oldLeft, int oldTop, int oldRight, int oldBottom) {
            if (view.getVisibility() == View.VISIBLE) {
                width = right - left;
                height = bottom - top;
                reserve();
            }
        }

        // The view reserves from the edge of the window to its far side, the edge its anchor names first vertically, and tells the engine only what changed.
        private void reserve() {
            Insets value = null;
            if (visible && !removed && placement.reserve && height > 0) {
                Insets insets = placement.insideSafeArea ? owner.getSafeArea() : Insets.NONE;
                int margin = Math.round(placement.marginDp * owner.getActivity().getResources().getDisplayMetrics().density);
                int horizontal = placement.anchor.horizontal;
                int vertical = placement.anchor.vertical;
                if (vertical < 0) {
                    value = Insets.of(0, insets.top + margin + height, 0, 0);
                } else if (vertical > 0) {
                    value = Insets.of(0, 0, 0, insets.bottom + margin + height);
                } else if (horizontal < 0) {
                    value = Insets.of(insets.left + margin + width, 0, 0, 0);
                } else if (horizontal > 0) {
                    value = Insets.of(0, 0, insets.right + margin + width, 0);
                }
            }
            if (value == null ? reserved == null : value.equals(reserved)) {
                return;
            }
            reserved = value;
            if (value == null) {
                HaylenOverlayLayer.nativeReleaseInsets(key);
            } else {
                HaylenOverlayLayer.nativeReserveInsets(key, value.left, value.top, value.right, value.bottom);
            }
        }
    }
}
