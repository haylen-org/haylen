package dev.haylen;

// An audio stream that a plugin opens with `HaylenPluginContext.openAudioStream` and pushes interleaved samples into from one thread at a time, such as a microphone, a synthesized voice or decoded network audio, which the app plays through `handle:audioStream(name)`. The samples fill a lock-free ring that the mixer reads. The stream belongs to the process, so it stays valid for good.
public final class HaylenAudioStream {
    // The samples: 32-bit floats from -1 to 1 or 16-bit integers.
    public enum Format {
        FLOAT32,
        INT16
    }

    private final long stream;

    private HaylenAudioStream(long stream) {
        this.stream = stream;
    }

    // Opens the stream once and returns it again with the same rate, channels and format, and throws an `IllegalArgumentException` for an empty name, a rate, channel count or capacity below 1, or a stream of the name that is open with another rate, channel count or format.
    static HaylenAudioStream open(String plugin, String name, int sampleRate, int channels, Format format, int capacity) {
        try {
            return new HaylenAudioStream(nativeOpen(HaylenBridge.utf8(plugin), HaylenBridge.utf8(name), sampleRate, channels, format.ordinal(), capacity));
        } catch (UnsatisfiedLinkError error) {
            throw new IllegalStateException("A plugin opens streams once the first activity loaded the engine library, not before.", error);
        }
    }

    // Pushes the first `frames` interleaved frames of the samples and returns how many fit into the ring, which drops the rest while it is full. The array crosses without a copy in Java. Throws an `IllegalArgumentException` for more frames than the samples hold and an `IllegalStateException` for a stream of the format `INT16`.
    public int push(float[] samples, int frames) {
        return nativePushFloats(stream, samples, frames);
    }

    // The same for a stream of the format `INT16`, and an `IllegalStateException` for a stream of the format `FLOAT32`.
    public int push(short[] samples, int frames) {
        return nativePushShorts(stream, samples, frames);
    }

    private static native long nativeOpen(byte[] plugin, byte[] name, int sampleRate, int channels, int format, int capacity);

    private static native int nativePushFloats(long stream, float[] samples, int frames);

    private static native int nativePushShorts(long stream, short[] samples, int frames);
}
