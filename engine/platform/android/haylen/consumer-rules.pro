# The engine reaches these classes from C++ by name, so R8 must keep them in minified app builds.

-keep class dev.haylen.HaylenBridge {
    static void dispatch(long, byte[], byte[]);
}

-keep class dev.haylen.HaylenEditText {
    static void edit(byte[]);
    static void finish();
}

-keep class dev.haylen.HaylenActivity {
    static void lockOrientation(int);
}

-keep class com.varn.VarnHttp { *; }
-keep class com.varn.VarnHttp$Response { *; }
-keep class com.varn.VarnHttp$ChunkSink { *; }
-keep class com.varn.NativeChunkSink { *; }

-keepclasseswithmembernames class dev.haylen.**, com.varn.** {
    native <methods>;
}
