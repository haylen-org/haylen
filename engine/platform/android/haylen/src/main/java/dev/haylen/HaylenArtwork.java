package dev.haylen;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import java.util.Arrays;

// The picture of the artwork that the now-playing controls show, from the bytes of a PNG or JPEG image that the app gives. It decodes at the smallest scale of two that fits `MAX_SIDE`, so a large image never holds a full bitmap on the UI thread, and it keeps the picture of the bytes it decoded last, so an update that only moves the position decodes nothing.
final class HaylenArtwork {
    // The longest side of the picture, which Android shows at a few hundred pixels at most.
    static final int MAX_SIDE = 1024;

    private byte[] bytes;
    private Bitmap picture;

    // The picture of the bytes, or `null` for no bytes or bytes that are no image.
    Bitmap decode(byte[] artwork) {
        if (artwork == null || artwork.length == 0) {
            return null;
        }

        if (Arrays.equals(artwork, bytes)) {
            return picture;
        }

        BitmapFactory.Options bounds = new BitmapFactory.Options();
        bounds.inJustDecodeBounds = true;
        BitmapFactory.decodeByteArray(artwork, 0, artwork.length, bounds);

        BitmapFactory.Options scaled = new BitmapFactory.Options();
        scaled.inSampleSize = 1;
        while (Math.max(bounds.outWidth, bounds.outHeight) / scaled.inSampleSize > MAX_SIDE) {
            scaled.inSampleSize *= 2;
        }

        bytes = artwork;
        picture = BitmapFactory.decodeByteArray(artwork, 0, artwork.length, scaled);
        return picture;
    }
}
