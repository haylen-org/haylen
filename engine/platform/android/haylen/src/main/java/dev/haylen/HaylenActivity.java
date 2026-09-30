package dev.haylen;

import android.app.Activity;
import android.app.NativeActivity;
import android.app.UiModeManager;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;
import androidx.annotation.RequiresApi;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import org.json.JSONObject;

// Hosts a Haylen app. The app library is named by the android.app.lib_name meta-data, like any native activity.
// The manifest gives the activity the Theme.Haylen.Splash theme, whose splash screen stays until the app has drawn its first frame.
// Every event of the activity reaches the plugins too, in load order, and the views they place over the app live in its panels.
public class HaylenActivity extends NativeActivity implements InputManager.InputDeviceListener {
    private final HaylenSplash splash = new HaylenSplash(this);
    private final HaylenPanels panels = new HaylenPanels(this);
    private HaylenEditText editor;
    private HaylenAudioFocus audioFocus;
    private HaylenNetwork network;
    private OnBackInvokedCallback backCallback;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        splash.install();
        // A native activity only opens its library with dlopen, so loading it here first is what binds the JNI methods and runs JNI_OnLoad.
        System.loadLibrary(libraryName());
        HaylenBridge.attach(this);
        nativeTelevision(getSystemService(UiModeManager.class).getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION);
        super.onCreate(savedInstanceState);

        // The app draws edge to edge, under the cutout, with the system bars hidden until a swipe shows them.
        Window window = getWindow();
        WindowCompat.setDecorFitsSystemWindows(window, false);
        WindowManager.LayoutParams attributes = window.getAttributes();
        coverCutouts(attributes);
        window.setAttributes(attributes);
        WindowInsetsControllerCompat controller = WindowCompat.getInsetsController(window, window.getDecorView());
        controller.hide(WindowInsetsCompat.Type.systemBars());
        controller.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);

        // The software keyboard lies over the app without resizing it, and the engine lifts the focused field above it.
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);
        editor = new HaylenEditText(this);
        addContentView(editor, new FrameLayout.LayoutParams(1, 1));
        HaylenEditText.attach(editor);
        ViewCompat.setOnApplyWindowInsetsListener(window.getDecorView(), (view, insets) -> {
            // Hidden bars only appear transiently over the app, so the safe area covers the cutout and any bars that stay visible, as in multi-window mode.
            Insets safe = insets.getInsets(WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.displayCutout());
            nativeSafeArea(safe.left, safe.top, safe.right, safe.bottom);
            panels.setSafeArea(safe);
            boolean keyboardShown = insets.isVisible(WindowInsetsCompat.Type.ime());
            int keyboardHeight = keyboardShown ? insets.getInsets(WindowInsetsCompat.Type.ime()).bottom : 0;
            nativeKeyboard(0, view.getHeight() - keyboardHeight, keyboardHeight > 0 ? view.getWidth() : 0, keyboardHeight);
            editor.setKeyboardShown(keyboardShown);
            return ViewCompat.onApplyWindowInsets(view, insets);
        });
        getSystemService(InputManager.class).registerInputDeviceListener(this, null);
        nativeOrientation(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT);
        audioFocus = new HaylenAudioFocus(this);
        network = new HaylenNetwork(this);
        network.register();
        HaylenPlugins.activityCreated(this, savedInstanceState);
    }

    @Override
    protected void onStart() {
        super.onStart();
        panels.setStarted(true);
        HaylenPlugins.activityStarted(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        audioFocus.request();
        HaylenPlugins.activityResumed(this);
    }

    @Override
    protected void onPause() {
        HaylenPlugins.activityPaused(this);
        audioFocus.abandon();
        super.onPause();
    }

    @Override
    protected void onStop() {
        HaylenPlugins.activityStopped(this);
        panels.setStarted(false);
        super.onStop();
    }

    // The template declares the activity single task, so a launch while it runs, such as a deep link or a notification, arrives here and becomes its intent.
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        HaylenPlugins.newIntent(intent);
    }

    // A native activity has no activity result API, so the plugins that start activities for a result receive it here.
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (!HaylenPlugins.activityResult(requestCode, resultCode, data)) {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (!HaylenPlugins.requestPermissionsResult(requestCode, permissions, grantResults)) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        nativeOrientation(configuration.orientation == Configuration.ORIENTATION_PORTRAIT);
        panels.refresh();
        HaylenPlugins.configurationChanged(configuration);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            panels.windowFocused();
        }
        HaylenPlugins.windowFocusChanged(hasFocus);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        splash.showOverlay();
    }

    // The app stops inside the native activity, which runs its cleanup and lets the activity finish, while the bridge, the editor and the listeners it may still use are attached. The plugins hear of it next, and the panels they leave go before the activity does.
    @Override
    protected void onDestroy() {
        super.onDestroy();
        HaylenPlugins.activityDestroyed(this);
        panels.removeAll();
        splash.dismiss();
        getSystemService(InputManager.class).unregisterInputDeviceListener(this);
        network.unregister();
        HaylenEditText.attach(null);
        HaylenBridge.detach();
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

    // Android 11 lets a window cover every cutout, Android 9 and 10 only those on the short edges, and earlier versions have no cutouts.
    static void coverCutouts(WindowManager.LayoutParams attributes) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
    }

    // Called from the frame thread of the engine with 0 for landscape, 1 for portrait and 2 for any orientation, the values app.json gives the manifest.
    static void lockOrientation(int value) {
        Activity activity = HaylenBridge.activity();
        if (activity == null) {
            return;
        }
        int requested = value == 0 ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE : value == 1 ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT : ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
        activity.runOnUiThread(() -> activity.setRequestedOrientation(requested));
    }

    // Called from the frame thread of the engine whenever the app starts or stops taking the back button, which it takes while it captures back or edits a text field. Before Android 13 back arrives as a key, which the engine takes or leaves itself.
    static void captureBack(boolean captured) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && HaylenBridge.activity() instanceof HaylenActivity activity) {
            activity.runOnUiThread(() -> activity.setBackCaptured(captured));
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

    // Called from the frame thread of the engine, which hears through nativeUrlOpened with the same request whether an app took the url.
    static void openUrl(long request, byte[] url) {
        Activity activity = HaylenBridge.activity();
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
        Activity activity = HaylenBridge.activity();
        Vibrator vibrator = activity != null ? activity.getSystemService(Vibrator.class) : null;
        if (vibrator != null && vibrator.hasVibrator()) {
            vibrator.vibrate(VibrationEffect.createOneShot(milliseconds, VibrationEffect.DEFAULT_AMPLITUDE));
        }
    }

    // Back reaches the app only through a registered callback, and without one Android plays its back animation and leaves the app.
    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    private void setBackCaptured(boolean captured) {
        if (captured == (backCallback != null)) {
            return;
        }
        OnBackInvokedDispatcher dispatcher = getOnBackInvokedDispatcher();
        if (captured) {
            backCallback = this::onBack;
            dispatcher.registerOnBackInvokedCallback(OnBackInvokedDispatcher.PRIORITY_DEFAULT, backCallback);
            return;
        }
        dispatcher.unregisterOnBackInvokedCallback(backCallback);
        backCallback = null;
    }

    // Back lets the text field being edited go first, and otherwise reaches the engine as escape.
    private void onBack() {
        if (!editor.dismiss()) {
            nativeBack();
        }
    }

    HaylenPanels panels() {
        return panels;
    }

    private String libraryName() {
        try {
            ActivityInfo info = getPackageManager().getActivityInfo(getComponentName(), PackageManager.GET_META_DATA);
            return info.metaData.getString("android.app.lib_name");
        } catch (PackageManager.NameNotFoundException error) {
            throw new IllegalStateException("The activity is missing from the manifest.", error);
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
