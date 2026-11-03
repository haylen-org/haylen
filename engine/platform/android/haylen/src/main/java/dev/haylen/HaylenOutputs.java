package dev.haylen;

import android.app.Activity;
import android.app.Presentation;
import android.hardware.display.DisplayManager;
import android.util.SparseArray;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import java.nio.charset.StandardCharsets;

// Shows the outputs of the app on the other displays of the device, each in a `Presentation` of the display manager that covers its display with a view whose surface the GPU device of the engine draws into. The surface reaches the engine as it comes, changes and goes, and the engine lets go of it before Android destroys it. A touch on the display of an output reaches the app in the points of the output. The outputs live on the UI thread and end with the activity.
final class HaylenOutputs {
    // The endings of `platform::Output::Ending`, in its order.
    private static final int ENDING_PERSON = 1;
    private static final int ENDING_DISCONNECTED = 2;
    private static final int ENDING_UNSUPPORTED = 3;

    // The kinds of touch of `platform::Event::Type` that `nativeTouch` takes.
    private static final int TOUCH_BEGAN = 0;
    private static final int TOUCH_MOVED = 1;
    private static final int TOUCH_ENDED = 2;
    private static final int TOUCH_CANCELLED = 3;

    private final Activity activity;
    private final DisplayManager manager;
    private final SparseArray<Presentation> shown = new SparseArray<>();

    HaylenOutputs(Activity activity) {
        this.activity = activity;
        manager = activity.getSystemService(DisplayManager.class);
    }

    // Called from the frame thread of the engine to open an output on a display. A display that went or that refuses the presentation ends the output with the reason.
    static void open(int number, int displayId, byte[] title) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity == null) {
            nativeEnded(number, ENDING_UNSUPPORTED, "The app has no activity to show an output from.".getBytes(StandardCharsets.UTF_8));
            return;
        }
        activity.runOnUiThread(() -> activity.outputs.show(number, displayId, new String(title, StandardCharsets.UTF_8)));
    }

    // Called from the frame thread of the engine once it let go of the surface of an output, which it forgets.
    static void close(int number) {
        HaylenActivity activity = HaylenBridge.activity();
        if (activity != null) {
            activity.runOnUiThread(() -> activity.outputs.dismiss(number));
        }
    }

    // Ends every output as the activity ends, before its windows go.
    void dismissAll() {
        while (shown.size() > 0) {
            dismiss(shown.keyAt(0));
        }
    }

    private void show(int number, int displayId, String title) {
        Display display = manager.getDisplay(displayId);
        if (display == null) {
            nativeEnded(number, ENDING_DISCONNECTED, new byte[0]);
            return;
        }

        Presentation presentation = new Presentation(activity, display);
        presentation.setTitle(title);
        SurfaceView view = new SurfaceView(presentation.getContext());
        float density = presentation.getContext().getResources().getDisplayMetrics().density;
        view.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {}

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                nativeSurface(number, holder.getSurface(), width, height, density);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                nativeSurface(number, null, 0, 0, density);
            }
        });
        view.setOnTouchListener((touched, event) -> {
            deliver(number, event, density);
            return true;
        });
        presentation.setContentView(view);

        // A presentation that goes without the app closing it went with its display or with the back button of that display.
        presentation.setOnDismissListener(dismissed -> {
            if (shown.get(number) == dismissed) {
                shown.remove(number);
                nativeEnded(number, manager.getDisplay(displayId) == null ? ENDING_DISCONNECTED : ENDING_PERSON, new byte[0]);
            }
        });
        shown.put(number, presentation);
        try {
            presentation.show();
        } catch (WindowManager.InvalidDisplayException exception) {
            shown.remove(number);
            nativeEnded(number, ENDING_UNSUPPORTED, ("The display refused the output. " + exception.getMessage()).getBytes(StandardCharsets.UTF_8));
        }
    }

    private void dismiss(int number) {
        Presentation presentation = shown.get(number);
        if (presentation != null) {
            shown.remove(number);
            presentation.dismiss();
        }
    }

    private static void deliver(int number, MotionEvent event, float density) {
        int type;
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                type = TOUCH_BEGAN;
                break;
            case MotionEvent.ACTION_MOVE:
                type = TOUCH_MOVED;
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                type = TOUCH_ENDED;
                break;
            case MotionEvent.ACTION_CANCEL:
                type = TOUCH_CANCELLED;
                break;
            default:
                return;
        }
        int count = event.getPointerCount();
        int[] ids = new int[count];
        float[] points = new float[count * 2];
        for (int index = 0; index < count; ++index) {
            ids[index] = event.getPointerId(index);
            points[index * 2] = event.getX(index) / density;
            points[index * 2 + 1] = event.getY(index) / density;
        }
        int changed = type == TOUCH_MOVED ? -1 : event.getActionIndex();
        nativeTouch(number, type, ids, points, changed);
    }

    private static native void nativeSurface(int number, Surface surface, int width, int height, float density);

    private static native void nativeEnded(int number, int ending, byte[] reason);

    private static native void nativeTouch(int number, int type, int[] ids, float[] points, int changed);
}
