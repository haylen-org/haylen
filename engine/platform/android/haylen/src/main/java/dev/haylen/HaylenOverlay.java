package dev.haylen;

import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import androidx.core.graphics.Insets;
import java.nio.charset.StandardCharsets;

// Places native views of a plugin over the app, such as banner ads. Views inside a native activity never draw and the engine takes every touch of its window, so every view lives in a panel window of its own over the activity window. A panel never takes the focus, because the app draws only while its activity has it, and touches reach the view inside its frame and the app everywhere else. Panels follow the safe area, rotations and size changes, hide while the activity is stopped and end with the activity, so a plugin places its views again for a new activity. Every method runs on the main thread.
public final class HaylenOverlay {
    private final String id;
    private int serial;

    HaylenOverlay(String id) {
        this.id = id;
    }

    // Places the view over the app and returns its panel. The panel shows once the window of the activity has had the focus.
    public Panel add(View view, HaylenPlacement placement) {
        requireMainThread();
        if (!(HaylenBridge.activity() instanceof HaylenActivity activity)) {
            throw new IllegalStateException("The plugin " + id + " can only place a view over the app while an activity exists.");
        }
        HaylenPanels panels = activity.panels();
        Panel panel = new Panel(panels, id + "#" + ++serial, view, placement.copy());
        panels.add(panel);
        return panel;
    }

    static void requireMainThread() {
        if (Looper.myLooper() != Looper.getMainLooper()) {
            throw new IllegalStateException("The overlay places views on the main thread only.");
        }
    }

    // A view over the app in its own panel window. The panel reserves the edge of its anchor while it shows, when its placement asks for it.
    public static final class Panel {
        private final HaylenPanels owner;
        private final byte[] key;
        private final View view;
        private final int viewWidth;
        private final int viewHeight;
        private final FrameLayout root;
        private final WindowManager.LayoutParams params;
        private HaylenPlacement placement;
        private boolean visible = true;
        private boolean attached;
        private boolean removed;
        private int width;
        private int height;
        private Insets reserved;

        Panel(HaylenPanels owner, String key, View view, HaylenPlacement placement) {
            this.owner = owner;
            this.key = key.getBytes(StandardCharsets.UTF_8);
            this.view = view;
            this.placement = placement;
            // A size that the placement leaves to the view is the size of its own layout parameters, or its measured size without them.
            ViewGroup.LayoutParams own = view.getLayoutParams();
            viewWidth = own != null ? own.width : ViewGroup.LayoutParams.WRAP_CONTENT;
            viewHeight = own != null ? own.height : ViewGroup.LayoutParams.WRAP_CONTENT;
            root = new FrameLayout(owner.getActivity());
            root.addView(view, new FrameLayout.LayoutParams(viewWidth, viewHeight));
            int flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS;
            params = new WindowManager.LayoutParams(WindowManager.LayoutParams.WRAP_CONTENT, WindowManager.LayoutParams.WRAP_CONTENT, WindowManager.LayoutParams.TYPE_APPLICATION_PANEL, flags, PixelFormat.TRANSLUCENT);
            params.setTitle("Haylen panel " + key);
            // The panel covers cutouts like the activity and ignores the system bars, since its position already keeps it inside the safe area.
            HaylenActivity.coverCutouts(params);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                params.setFitInsetsTypes(0);
            }
            root.addOnLayoutChangeListener((laidOut, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                if (root.getVisibility() == View.VISIBLE) {
                    width = right - left;
                    height = bottom - top;
                    reserve();
                }
            });
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

        // Takes the view off the app for good and gives its edge back. The view leaves the panel, so the plugin may place it again.
        public void remove() {
            requireMainThread();
            if (removed) {
                return;
            }
            removed = true;
            if (attached) {
                owner.getActivity().getWindowManager().removeViewImmediate(root);
                attached = false;
            }
            root.removeView(view);
            reserve();
            owner.forget(this);
        }

        // The frame of the view in the pixels of the activity window, or null while it does not show.
        public Rect bounds() {
            requireMainThread();
            if (!attached || root.getVisibility() != View.VISIBLE || !root.isLaidOut()) {
                return null;
            }
            int[] panel = new int[2];
            int[] window = new int[2];
            root.getLocationOnScreen(panel);
            owner.getActivity().getWindow().getDecorView().getLocationOnScreen(window);
            int left = panel[0] - window[0];
            int top = panel[1] - window[1];
            return new Rect(left, top, left + root.getWidth(), top + root.getHeight());
        }

        // Brings the window of the panel in line with the placement, the safe area and the state of the activity. The window joins the activity window once that window has had the focus and a token.
        void refresh() {
            if (removed) {
                return;
            }
            float density = owner.getActivity().getResources().getDisplayMetrics().density;
            int viewWidthPixels = placement.widthDp == HaylenPlacement.MEASURED ? viewWidth : Math.round(placement.widthDp * density);
            int viewHeightPixels = placement.heightDp == HaylenPlacement.MEASURED ? viewHeight : Math.round(placement.heightDp * density);
            ViewGroup.LayoutParams size = view.getLayoutParams();
            if (size.width != viewWidthPixels || size.height != viewHeightPixels) {
                view.setLayoutParams(new FrameLayout.LayoutParams(viewWidthPixels, viewHeightPixels));
            }
            root.setVisibility(visible && owner.isStarted() ? View.VISIBLE : View.GONE);
            place(density);

            WindowManager manager = owner.getActivity().getWindowManager();
            if (attached) {
                manager.updateViewLayout(root, params);
            } else if (owner.isWindowReady()) {
                params.token = owner.getActivity().getWindow().getDecorView().getWindowToken();
                manager.addView(root, params);
                attached = true;
            }
            reserve();
        }

        // The window manager lays the panel out against the activity window by gravity, so the offsets from the anchored edges are all the panel keeps up to date.
        private void place(float density) {
            Insets insets = placement.insideSafeArea ? owner.getSafeArea() : Insets.NONE;
            int margin = Math.round(placement.marginDp * density);
            int horizontal = placement.anchor.horizontal;
            int vertical = placement.anchor.vertical;
            params.gravity = (horizontal < 0 ? Gravity.LEFT : horizontal > 0 ? Gravity.RIGHT : Gravity.CENTER_HORIZONTAL) | (vertical < 0 ? Gravity.TOP : vertical > 0 ? Gravity.BOTTOM : Gravity.CENTER_VERTICAL);
            params.x = horizontal < 0 ? insets.left + margin : horizontal > 0 ? insets.right + margin : (insets.left - insets.right) / 2;
            params.y = vertical < 0 ? insets.top + margin : vertical > 0 ? insets.bottom + margin : (insets.top - insets.bottom) / 2;
        }

        // The panel reserves from the edge of the window to its far side, the edge its anchor names first vertically, and tells the engine only what changed.
        private void reserve() {
            Insets value = null;
            if (attached && visible && !removed && placement.reserve && height > 0) {
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
                HaylenPanels.nativeReleaseInsets(key);
            } else {
                HaylenPanels.nativeReserveInsets(key, value.left, value.top, value.right, value.bottom);
            }
        }
    }
}
