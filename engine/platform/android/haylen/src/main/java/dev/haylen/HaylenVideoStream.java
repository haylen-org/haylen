package dev.haylen;

import android.graphics.Bitmap;
import java.nio.ByteBuffer;

// A video stream that a plugin opens with `HaylenPluginContext.openVideoStream` and pushes frames into from any thread, such as the frames of a camera or a video decoder, which the app draws through `handle:videoStream(name)`. The engine keeps only the newest frame and uploads it once per frame at most. The stream belongs to the process, so it stays valid for good.
public final class HaylenVideoStream {
    // The layout of the pixels: four bytes per pixel in the order the name gives.
    public enum Format {
        RGBA8,
        BGRA8
    }

    private final long stream;

    private HaylenVideoStream(long stream) {
        this.stream = stream;
    }

    // Opens the stream once and returns it again with the same format, and throws an `IllegalArgumentException` for an empty name, a negative size or a stream of the name that is open with another format.
    static HaylenVideoStream open(String plugin, String name, int width, int height, Format format) {
        try {
            return new HaylenVideoStream(nativeOpen(HaylenBridge.utf8(plugin), HaylenBridge.utf8(name), format.ordinal(), width, height));
        } catch (UnsatisfiedLinkError error) {
            throw new IllegalStateException("A plugin opens streams once the first activity loaded the engine library, not before.", error);
        }
    }

    // Pushes a frame of `width` by `height` pixels in the format of the stream from the remaining bytes of the buffer, whose rows start `stride` bytes apart. A direct buffer crosses without a copy in Java. Throws an `IllegalArgumentException` for a size that is not positive, a stride shorter than a row or a buffer that holds fewer bytes than the frame.
    public void push(ByteBuffer pixels, int width, int height, int stride, double timestamp) {
        if (pixels.isDirect()) {
            nativePushBuffer(stream, pixels, pixels.position(), pixels.remaining(), width, height, stride, timestamp);
        } else {
            nativePushArray(stream, pixels.array(), pixels.arrayOffset() + pixels.position(), pixels.remaining(), width, height, stride, timestamp);
        }
    }

    // Pushes the pixels of a bitmap of `Bitmap.Config.ARGB_8888`, which Android keeps as RGBA bytes, into a stream of the format `RGBA8`. Throws an `IllegalArgumentException` for a bitmap of another config and an `IllegalStateException` for a stream of the format `BGRA8` or a recycled bitmap.
    public void push(Bitmap bitmap, double timestamp) {
        nativePushBitmap(stream, bitmap, timestamp);
    }

    private static native long nativeOpen(byte[] plugin, byte[] name, int format, int width, int height);

    private static native void nativePushBuffer(long stream, ByteBuffer pixels, int offset, int length, int width, int height, int stride, double timestamp);

    private static native void nativePushArray(long stream, byte[] pixels, int offset, int length, int width, int height, int stride, double timestamp);

    private static native void nativePushBitmap(long stream, Bitmap bitmap, double timestamp);
}
