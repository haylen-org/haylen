package dev.haylen;

import android.app.Activity;
import android.graphics.PixelFormat;
import android.os.Build;
import android.view.Choreographer;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import androidx.core.splashscreen.SplashScreen;

// Keeps the splash screen of Theme.Haylen.Splash on screen until the app has drawn its first frame, so nothing black shows in between.
// sokol_app draws only while the window of the activity has the focus, which the window gets only once it shows, so the system splash screen hands over to an overlay window with the same background and icon that lets the focus through and stays until the first frame.
final class HaylenSplash {
    private final Activity activity;
    private View overlay;
    private boolean overlayDrawn;

    HaylenSplash(Activity activity) {
        this.activity = activity;
    }

    // Called before the activity creates its window, as the SplashScreen API requires.
    void install() {
        SplashScreen.installSplashScreen(activity).setKeepOnScreenCondition(() -> !overlayDrawn);
    }

    // Called once the window of the activity is attached, which gives the overlay a parent window.
    void showOverlay() {
        if (overlay != null) {
            return;
        }
        if (nativeFramePresented()) {
            overlayDrawn = true;
            return;
        }

        // The splash screen draws its icon in a square of 288 dp, and the icon drawable already keeps the logo inside the circle Android masks it with.
        FrameLayout view = new FrameLayout(activity);
        view.setBackgroundColor(activity.getColor(R.color.haylen_splash_background));
        ImageView icon = new ImageView(activity);
        icon.setImageResource(R.drawable.haylen_splash_icon);
        int size = Math.round(288 * activity.getResources().getDisplayMetrics().density);
        view.addView(icon, new FrameLayout.LayoutParams(size, size, Gravity.CENTER));

        int flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS;
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.TYPE_APPLICATION_PANEL, flags, PixelFormat.OPAQUE);
        params.token = activity.getWindow().getDecorView().getWindowToken();
        HaylenActivity.coverCutouts(params);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            params.setFitInsetsTypes(0);
        }
        // The overlay has reached the screen one frame after it first draws, which is when the system splash screen may go.
        view.getViewTreeObserver().addOnDrawListener(() -> Choreographer.getInstance().postFrameCallback(frameTimeNanos -> overlayDrawn = true));
        activity.getWindowManager().addView(view, params);
        overlay = view;
        Choreographer.getInstance().postFrameCallback(this::removeOnceAppDrew);
    }

    void dismiss() {
        if (overlay != null) {
            activity.getWindowManager().removeViewImmediate(overlay);
            overlay = null;
        }
    }

    private void removeOnceAppDrew(long frameTimeNanos) {
        if (overlay == null) {
            return;
        }
        if (nativeFramePresented()) {
            dismiss();
            return;
        }
        Choreographer.getInstance().postFrameCallback(this::removeOnceAppDrew);
    }

    private static native boolean nativeFramePresented();
}
