# The engine reaches these classes from C++ by name, so R8 must keep them in minified app builds.

-keep class dev.haylen.HaylenBridge {
    static void dispatch(long, byte[], byte[], byte[][]);
    static void cancel(long);
    static void setAppRunning(boolean);
}

-keep class dev.haylen.HaylenPlugins {
    static byte[] ids();
    static void reportError(byte[]);
}

# The `haylen` library creates plugins by the class names of the manifest meta-data, so their classes and constructors keep their names.
-keep class * extends dev.haylen.HaylenPlugin {
    public <init>();
}

-keep class dev.haylen.HaylenEditText {
    static void edit(byte[]);
    static void finish();
}

-keep class dev.haylen.HaylenActivity {
    static void lockOrientation(int);
    static void captureBack(boolean);
    static byte[] systemInfo();
    static void openUrl(long, byte[]);
    static void vibrate(long);
    static byte[] networkRequirement();
    static byte[] developmentServer();
}

-keep class dev.haylen.HaylenDialogs {
    static void show(long, byte[], byte[]);
    static void cancel(long);
}

-keep class dev.haylen.HaylenScreens {
    static void open(long, byte[], byte[], byte[], byte[][], byte[], boolean);
    static void cancel(long);
}

-keep class com.varn.VarnHttp { *; }
-keep class com.varn.VarnHttp$Response { *; }
-keep class com.varn.VarnHttp$ChunkSink { *; }
-keep class com.varn.NativeChunkSink { *; }

-keepclasseswithmembernames class dev.haylen.**, com.varn.** {
    native <methods>;
}
