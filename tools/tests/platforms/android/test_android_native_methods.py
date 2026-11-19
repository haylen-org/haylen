from haylen.platforms.android.android_native_methods import AndroidNativeMethods
from haylen.platforms.android.java_class_file import JavaClassFile


class AndroidNativeMethodsTest:
    def test_a_native_method_resolves_by_its_short_or_its_long_jni_name(self):
        native = AndroidNativeMethods.Native("dev/haylen/Haylen_Bridge", JavaClassFile.Method("native_Emit", "(J[BLjava/lang/String;)V"))
        methods = AndroidNativeMethods((native,))

        assert native.short_name == "Java_dev_haylen_Haylen_1Bridge_native_1Emit"
        assert native.long_name == "Java_dev_haylen_Haylen_1Bridge_native_1Emit__J_3BLjava_lang_String_2"
        assert methods.missing({native.short_name}) == []
        assert methods.missing({native.long_name}) == []
        assert methods.missing({"Java_dev_haylen_HaylenBridge_nativeEmit"}) == [native]
