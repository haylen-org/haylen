package dev.haylen;

import android.app.UiModeManager;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import androidx.activity.OnBackPressedCallback;
import androidx.core.content.IntentCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import com.google.androidgamesdk.GameActivity;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import org.json.JSONObject;

// Hosts a Haylen app. The class `GameActivity`, an `AppCompatActivity`, loads the native library that the `android.app.lib_name` meta-data names and draws the app in a `SurfaceView` of an ordinary view hierarchy, so plugins place views over the app, register Activity Result launchers and use fragments, dialogs and Compose.
// The manifest gives the activity the `Theme.Haylen.Splash` theme, whose splash screen hands over to a view with the same look that stays until the app has drawn its first frame.
// The template declares the activity single top, and `HaylenLinkActivity` hands it the links and notifications that open the app, so a screen of a plugin that shows over the app outlives the launcher icon and the links.
public class HaylenActivity extends GameActivity implements InputManager.InputDeviceListener {
    // The link that `HaylenLinkActivity` started the activity with, next to the launcher intent that the task of the app keeps.
    static final String EXTRA_LINK = "dev.haylen.link";

    private final HaylenSplash splash = new HaylenSplash(this);
    private HaylenOverlayLayer overlays;
    private HaylenEditText editor;
    private HaylenNetwork network;
    private OnBackPressedCallback backCallback;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        splash.install();
        HaylenBridge.attach(this);
        // The class `GameActivity` loads the native library here and starts the app on its render thread.
        super.onCreate(savedInstanceState);
        Intent link = IntentCompat.getParcelableExtra(getIntent(), EXTRA_LINK, Intent.class);
        if (link != null) {
            setIntent(link);
        }
        nativeTelevision(getSystemService(UiModeManager.class).getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION);
        nativeOrientation(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT);

        // The views over the app share the layout of its surface, above it.
        FrameLayout content = findViewById(contentViewId);
        overlays = new HaylenOverlayLayer(this, content);
        editor = new HaylenEditText(this);
        content.addView(editor, new FrameLayout.LayoutParams(1, 1));
        HaylenEditText.attach(editor);
        splash.show();
        ViewCompat.setOnApplyWindowInsetsListener(getWindow().getDecorView(), this::applyInsets);

        backCallback = new OnBackPressedCallback(false) {
            @Override
            public void handleOnBackPressed() {
                onBack();
            }
        };
        getOnBackPressedDispatcher().addCallback(this, backCallback);
        getSystemService(InputManager.class).registerInputDeviceListener(this, null);
        network = new HaylenNetwork(this);
        network.register();

        // The audio focus follows the lifecycle before the plugins do, so it is requested before they hear of a resume and abandoned after they hear of a pause.
        getLifecycle().addObserver(new HaylenAudioFocus(this));
        HaylenPlugins.activityCreated(this, savedInstanceState);
    }

    // The class `GameActivity` sets up the window before it creates the activity. The app draws edge to edge, under the cutout, with the system bars hidden until a swipe shows them, and the software keyboard lies over the app without resizing it, while the engine lifts the focused field above it.
    @Override
    protected void onSetUpWindow() {
        Window window = getWindow();
        window.setFormat(PixelFormat.RGBX_8888);
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
        WindowCompat.setDecorFitsSystemWindows(window, false);
        WindowManager.LayoutParams attributes = window.getAttributes();
        coverCutouts(attributes);
        window.setAttributes(attributes);
        WindowInsetsControllerCompat controller = WindowCompat.getInsetsController(window, window.getDecorView());
        controller.hide(WindowInsetsCompat.Type.systemBars());
        controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    }

    @Override
    protected InputEnabledSurfaceView createSurfaceView() {
        return new AppSurface();
    }

    // A launch while the activity is on top, such as the launcher icon, arrives here and becomes its intent.
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        receiveIntent(intent);
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        nativeOrientation(configuration.orientation == Configuration.ORIENTATION_PORTRAIT);
        overlays.refresh();
        HaylenPlugins.configurationChanged(configuration);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        HaylenPlugins.windowFocusChanged(hasFocus);
    }

    // Controllers send their sticks, triggers and hats to the focused view, which may be a view over the app, so they reach the app wherever the focus is.
    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK)) {
            return processMotionEvent(event);
        }
        return super.dispatchGenericMotionEvent(event);
    }

    // The app stops inside `GameActivity`, which runs its cleanup and lets the activity finish, while the bridge, the editor and the listeners it may still use are attached. The plugins hear of it next, and the views they left over the app go with the activity.
    @Override
    protected void onDestroy() {
        super.onDestroy();
        HaylenPlugins.activityDestroyed(this);
        overlays.removeAll();
        splash.dismiss();
        getSystemService(InputManager.class).unregisterInputDeviceListener(this);
        network.unregister();
        HaylenEditText.detach(editor);
        HaylenBridge.detach(this);
    }

    // Hiding the interface is the only trim level that says nothing about memory pressure.
    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (level != TRIM_MEMORY_UI_HIDDEN) {
            nativeLowMemory();
        }
        HaylenPlugins.trimMemory(level);
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {}

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        nativeControllerRemoved(deviceId);
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {}

    // Called from the frame thread of the engine with 0 for landscape, 1 for portrait and 2 for any orientation, the values `app.json` gives the manifest.
    static void lockOrientation(int value) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            return;
        }
        int requested = value == 0 ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE : value == 1 ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT : ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
        activity.runOnUiThread(() -> activity.setRequestedOrientation(requested));
    }

    // Called from the frame thread of the engine whenever the app starts or stops taking the back button, which it takes while it captures back or edits a text field. The back callback takes back while it is enabled, and otherwise Android leaves the app with its predictive back animation.
    static void captureBack(boolean captured) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity != null) {
            activity.runOnUiThread(() -> activity.backCallback.setEnabled(captured));
        }
    }

    // Called from the frame thread of the engine when an app first starts, with what Android tells about the device as JSON.
    static byte[] systemInfo() {
        Map<String, Object> info = new HashMap<>();
        info.put("osVersion", Build.VERSION.RELEASE);
        info.put("deviceModel", Build.MODEL);
        info.put("locale", Locale.getDefault().toLanguageTag());
        return new JSONObject(info).toString().getBytes(StandardCharsets.UTF_8);
    }

    // Called from the frame thread of the engine, which hears through `nativeUrlOpened` with the same request whether an app took the url.
    static void openUrl(long request, byte[] url) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            nativeUrlOpened(request, false);
            return;
        }
        String address = new String(url, StandardCharsets.UTF_8);
        activity.runOnUiThread(() -> {
            try {
                activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(address)));
                nativeUrlOpened(request, true);
            } catch (ActivityNotFoundException error) {
                nativeUrlOpened(request, false);
            }
        });
    }

    // Called from the frame thread of the engine. Devices without a vibrator do nothing.
    static void vibrate(long milliseconds) {
        HaylenActivity activity = HaylenBridge.activity();
        Vibrator vibrator = activity != null ? activity.getSystemService(Vibrator.class) : null;
        if (vibrator != null && vibrator.hasVibrator()) {
            vibrator.vibrate(VibrationEffect.createOneShot(milliseconds, VibrationEffect.DEFAULT_AMPLITUDE));
        }
    }

    // A link or a notification that reaches the running app, which becomes the intent of the activity before the plugins hear of it.
    void receiveIntent(Intent intent) {
        setIntent(intent);
        HaylenPlugins.newIntent(intent);
    }

    HaylenOverlayLayer overlays() {
        return overlays;
    }

    // Hidden bars only appear transiently over the app, so the safe area covers the cutout and any bars that stay visible, as in multi-window mode.
    private WindowInsetsCompat applyInsets(View view, WindowInsetsCompat insets) {
        Insets safe = insets.getInsets(WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.displayCutout());
        nativeSafeArea(safe.left, safe.top, safe.right, safe.bottom);
        overlays.setSafeArea(safe);

        boolean keyboardShown = insets.isVisible(WindowInsetsCompat.Type.ime());
        int keyboardHeight = keyboardShown ? insets.getInsets(WindowInsetsCompat.Type.ime()).bottom : 0;
        nativeKeyboard(0, view.getHeight() - keyboardHeight, keyboardHeight > 0 ? view.getWidth() : 0, keyboardHeight);
        editor.setKeyboardShown(keyboardShown);
        return ViewCompat.onApplyWindowInsets(view, insets);
    }

    // Back lets the text field being edited go first, and otherwise reaches the engine as escape.
    private void onBack() {
        if (!editor.dismiss()) {
            nativeBack();
        }
    }

    // Android 11 lets a window cover every cutout, Android 9 and 10 only those on the short edges, and earlier versions have no cutouts.
    private static void coverCutouts(WindowManager.LayoutParams attributes) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
    }

    // The surface the app draws on. The class `GameActivity` focuses it whenever its window gains the focus, which would take the focus and the keyboard from a text field over the app, such as the hidden field that edits a field of the UI or a form of a plugin, so it declines the focus while such a field has it.
    // Android marks the focused view with a highlight over it once keys drive the window, as on a TV, which would veil the whole app, while the app shows its own focus.
    private final class AppSurface extends InputEnabledSurfaceView {
        AppSurface() {
            super(HaylenActivity.this);
            setDefaultFocusHighlightEnabled(false);
        }

        @Override
        public boolean requestFocus(int direction, Rect previouslyFocusedRect) {
            View focused = getCurrentFocus();
            if (focused != null && focused != this && focused.onCheckIsTextEditor()) {
                return false;
            }
            return super.requestFocus(direction, previouslyFocusedRect);
        }
    }

    private static native void nativeSafeArea(int left, int top, int right, int bottom);

    private static native void nativeControllerRemoved(int deviceId);

    private static native void nativeTelevision(boolean television);

    private static native void nativeLowMemory();

    private static native void nativeKeyboard(int x, int y, int width, int height);

    private static native void nativeOrientation(boolean portrait);

    private static native void nativeBack();

    private static native void nativeUrlOpened(long request, boolean opened);
}
