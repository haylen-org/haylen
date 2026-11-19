package dev.haylen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.view.MotionEvent;
import org.junit.Test;

public class HaylenTouchpadSwipeTest {
    // A swipe of two fingers on a touchpad scrolls a play area or a list as a trackpad does, in phases, instead of touching it with a fake finger.
    @Test
    public void turnsTheFakeFingerOfASwipeIntoAPhasedScrollInPoints() {
        HaylenTouchpadSwipe swipe = new HaylenTouchpadSwipe();

        assertEquals(new HaylenTouchpadSwipe.Scroll(HaylenTouchpadSwipe.BEGAN, 100.0f, 200.0f, 0.0f, 0.0f), swipe.next(MotionEvent.ACTION_DOWN, 100.0f, 200.0f, 2.0f));
        assertEquals(new HaylenTouchpadSwipe.Scroll(HaylenTouchpadSwipe.CHANGED, 110.0f, 240.0f, 5.0f, 20.0f), swipe.next(MotionEvent.ACTION_MOVE, 110.0f, 240.0f, 2.0f));
        assertEquals(new HaylenTouchpadSwipe.Scroll(HaylenTouchpadSwipe.CHANGED, 104.0f, 236.0f, -3.0f, -2.0f), swipe.next(MotionEvent.ACTION_MOVE, 104.0f, 236.0f, 2.0f));
        assertEquals(new HaylenTouchpadSwipe.Scroll(HaylenTouchpadSwipe.ENDED, 104.0f, 236.0f, 0.0f, 0.0f), swipe.next(MotionEvent.ACTION_UP, 104.0f, 236.0f, 2.0f));
    }

    @Test
    public void endsAGestureThatTheSystemCancelledAndIgnoresOtherActions() {
        HaylenTouchpadSwipe swipe = new HaylenTouchpadSwipe();
        swipe.next(MotionEvent.ACTION_DOWN, 0.0f, 0.0f, 1.0f);

        assertNull(swipe.next(MotionEvent.ACTION_HOVER_MOVE, 5.0f, 5.0f, 1.0f));
        assertEquals(HaylenTouchpadSwipe.ENDED, swipe.next(MotionEvent.ACTION_CANCEL, 5.0f, 5.0f, 1.0f).phase());
    }
}
