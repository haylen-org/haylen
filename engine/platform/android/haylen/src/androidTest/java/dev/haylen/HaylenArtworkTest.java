package dev.haylen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.io.ByteArrayOutputStream;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class HaylenArtworkTest {
    private static byte[] png(int width, int height) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, bytes);
        bitmap.recycle();
        return bytes.toByteArray();
    }

    // A large image decodes at a scale that fits the controls, so its full bitmap never takes the memory of the UI thread.
    @Test
    public void decodesALargeImageWithinItsLongestSide() {
        Bitmap picture = new HaylenArtwork().decode(png(4000, 2000));

        assertTrue(Math.max(picture.getWidth(), picture.getHeight()) <= HaylenArtwork.MAX_SIDE);
        assertEquals(2, picture.getWidth() / picture.getHeight());
    }

    // An update that only moves the position hands the same bytes again, which decode nothing.
    @Test
    public void keepsThePictureOfTheBytesItDecodedLast() {
        HaylenArtwork artwork = new HaylenArtwork();
        Bitmap first = artwork.decode(png(64, 64));

        assertSame(first, artwork.decode(png(64, 64)));
        assertNull(artwork.decode(new byte[] {1, 2, 3}));
        assertNull(artwork.decode(null));
    }
}
