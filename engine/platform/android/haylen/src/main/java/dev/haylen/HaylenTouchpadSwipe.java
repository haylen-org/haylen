package dev.haylen;

import android.view.MotionEvent;

// The two-finger swipe of a touchpad, which Android 14 and later hand over as the moves of one fake finger with the classification `CLASSIFICATION_TWO_FINGER_SWIPE`, as the scroll of a precise device with the phases of its gesture, the way the trackpads of macOS scroll. The fingers that touch the touchpad begin the gesture, each move scrolls by the points the fake finger moved, and the fingers that leave end it. Only the UI thread reaches it.
final class HaylenTouchpadSwipe {
    // The phases in the order of `Event::ScrollPhase`.
    static final int BEGAN = 1;
    static final int CHANGED = 2;
    static final int ENDED = 3;

    // A scroll at a place of the window in pixels, by a distance in points, which moving the fingers down and to the right makes positive, as on macOS.
    record Scroll(int phase, float x, float y, float deltaX, float deltaY) {}

    private float lastX;
    private float lastY;

    // The scroll of a move of the fake finger, from its action and its place in pixels, with the density of the display in pixels a point, or `null` for an action that is no part of the gesture.
    Scroll next(int action, float x, float y, float density) {
        switch (action) {
            case MotionEvent.ACTION_DOWN -> {
                lastX = x;
                lastY = y;
                return new Scroll(BEGAN, x, y, 0.0f, 0.0f);
            }
            case MotionEvent.ACTION_MOVE -> {
                Scroll moved = new Scroll(CHANGED, x, y, (x - lastX) / density, (y - lastY) / density);
                lastX = x;
                lastY = y;
                return moved;
            }
            case MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                return new Scroll(ENDED, x, y, 0.0f, 0.0f);
            }
            default -> {
                return null;
            }
        }
    }
}
