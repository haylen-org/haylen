import zipfile
from collections.abc import Mapping

from haylen.platforms.android.android_apk import AndroidApk
from haylen.platforms.android.android_verifier import AndroidVerifier
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.product_component import ProductComponent
from tests.platforms.android.android_fixture import AndroidFixture
from tests.support.native_binaries import NativeBinaries


class AndroidVerifierTest:
    @staticmethod
    def product(fixture: AndroidFixture, app, entries: list[str], libraries: Mapping[str, bytes] | None = None, abis: tuple[str, ...] = ("arm64-v8a", "x86_64")) -> BuildProduct:
        """A built product whose APK holds the entries, each a library that defines the natives of the fixture unless `libraries` gives its bytes."""
        prepared = fixture.prepare(fixture.resolve(app, architectures=abis))
        apk = fixture.tooling.folder / f"app-{len(entries)}-{len(libraries or {})}.apk"
        with zipfile.ZipFile(apk, "w") as archive:
            for entry in entries:
                archive.writestr(entry, (libraries or {}).get(entry, NativeBinaries.elf(AndroidFixture.NATIVES)))
        components = tuple(ProductComponent(project.target, apk, "abi") for project in prepared.product.projects)
        return BuildProduct(prepared, apk, "apk", components, ())

    @staticmethod
    def outcomes(report) -> dict[str, list[tuple[str, str]]]:
        found: dict[str, list[tuple[str, str]]] = {}
        for check in report.checks:
            found.setdefault(check.name, []).append((check.outcome.value, check.detail))
        return found

    def test_every_abi_holds_the_library_of_the_activity_and_the_native_libraries(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"native": {"physics": {"cmake": "physics", "platforms": ["android"]}}})
        (app / "physics").mkdir()
        (app / "physics" / "CMakeLists.txt").write_text("", encoding="utf-8")
        monkeypatch.setattr(AndroidApk, "manifest", lambda apk, context: {"activity": {"dev.haylen.HaylenActivity"}})
        verifier = AndroidVerifier(fixture.context, lambda: None)

        whole = AndroidVerifierTest.product(fixture, app, [f"lib/{abi}/lib{name}.so" for abi in ("arm64-v8a", "x86_64") for name in ("haylen", "physics")])
        lacking = AndroidVerifierTest.product(fixture, app, ["lib/arm64-v8a/libhaylen.so", "lib/arm64-v8a/libphysics.so", "lib/x86_64/libhaylen.so"])

        assert verifier.verify(whole).passed
        report = verifier.verify(lacking)
        assert not report.passed
        assert AndroidVerifierTest.outcomes(report)["libraries"] == [("failed", 'The APK lacks the native libraries "lib/x86_64/libphysics.so", which the activity and Lua load.')]

    def test_the_merged_manifest_holds_what_the_engine_and_every_plugin_need(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"camera": {}}, "platforms": {"android": {"manifest": {"removedPermissions": ["android.permission.CAMERA"]}}}})
        plugin = fixture.plugin(app, "camera", {})
        manifest = '<manifest xmlns:android="http://schemas.android.com/apk/res/android">\n    <uses-permission android:name="android.permission.CAMERA" />\n    <application>\n        <activity android:name=".CameraActivity" />\n        <meta-data android:name="dev.haylen.plugin.camera" android:value="${id}" />\n    </application>\n</manifest>\n'
        (plugin / "android" / "src" / "main" / "AndroidManifest.xml").write_text(manifest, encoding="utf-8")
        merged = {"activity": {"dev.haylen.HaylenActivity"}, "provider": {"dev.haylen.HaylenPluginProvider"}, "meta-data": {"dev.haylen.plugin.camera"}}
        monkeypatch.setattr(AndroidApk, "manifest", lambda apk, context: merged)
        product = AndroidVerifierTest.product(fixture, app, [f"lib/{abi}/libhaylen.so" for abi in ("arm64-v8a", "x86_64")])

        checks = AndroidVerifierTest.outcomes(AndroidVerifier(fixture.context, lambda: None).verify(product))["manifest"]

        assert checks == [
            (
                "warning",
                'The plugin "camera" needs the permission "android.permission.CAMERA", which "platforms.android.manifest.removedPermissions" of "haylen.json" removes, so the calls that need it answer "unsupported". Keep the permission to use them.',
            ),
            ("failed", 'The plugin "camera" needs the activity "com.haylen.plugins.camera.CameraActivity", which the merged manifest of the built app lacks.'),
        ]

    def test_only_plugin_classes_need_the_provider_that_loads_them(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"links": {}}})
        plugin = fixture.plugin(app, "links", {})
        manifest = '<manifest xmlns:android="http://schemas.android.com/apk/res/android">\n    <application>\n        <activity android:name="dev.haylen.HaylenLinkActivity" android:exported="true" />\n    </application>\n</manifest>\n'
        (plugin / "android" / "src" / "main" / "AndroidManifest.xml").write_text(manifest, encoding="utf-8")
        product = AndroidVerifierTest.product(fixture, app, [f"lib/{abi}/libhaylen.so" for abi in ("arm64-v8a", "x86_64")])
        verifier = AndroidVerifier(fixture.context, lambda: None)

        without_class = {"activity": {"dev.haylen.HaylenActivity", "dev.haylen.HaylenLinkActivity"}}
        monkeypatch.setattr(AndroidApk, "manifest", lambda apk, context: without_class)
        assert AndroidVerifierTest.outcomes(verifier.verify(product))["manifest"] == [("passed", "The merged manifest holds what the engine and every plugin need.")]

        with_class = {**without_class, "meta-data": {"dev.haylen.plugin.links"}}
        monkeypatch.setattr(AndroidApk, "manifest", lambda apk, context: with_class)
        assert AndroidVerifierTest.outcomes(verifier.verify(product))["manifest"] == [
            ("failed", 'Loading the plugin classes of the meta-data "dev.haylen.plugin.links" needs the provider "dev.haylen.HaylenPluginProvider", which the merged manifest of the built app lacks.')
        ]

    def test_the_library_of_the_activity_defines_every_native_method_of_the_engine_for_every_abi(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({})
        monkeypatch.setattr(AndroidApk, "manifest", lambda apk, context: {"activity": {"dev.haylen.HaylenActivity"}})
        verifier = AndroidVerifier(fixture.context, lambda: None)
        abis = ("arm64-v8a", "armeabi-v7a")
        entries = [f"lib/{abi}/libhaylen.so" for abi in abis]

        whole = {"lib/armeabi-v7a/libhaylen.so": NativeBinaries.elf(AndroidFixture.NATIVES, wide=False)}
        assert AndroidVerifierTest.outcomes(verifier.verify(AndroidVerifierTest.product(fixture, app, entries, whole, abis)))["natives"] == [
            ("passed", 'The library "libhaylen.so" defines the JNI functions of every native method of the engine for "arm64-v8a", "armeabi-v7a".')
        ]

        # The library of the release that dropped the JNI functions of the runtime still needed them and defined only the entry of GameActivity.
        dropped = {"lib/armeabi-v7a/libhaylen.so": NativeBinaries.elf(["Java_com_google_androidgamesdk_GameActivity_initializeNativeCode"], needed=AndroidFixture.NATIVES, wide=False)}
        report = verifier.verify(AndroidVerifierTest.product(fixture, app, entries, dropped, abis))
        assert not report.passed
        assert AndroidVerifierTest.outcomes(report)["natives"] == [
            (
                "failed",
                'The library "lib/armeabi-v7a/libhaylen.so" lacks the JNI functions of 2 native methods of the engine, such as "JNI_OnLoad", "dev.haylen.HaylenActivity.nativeTelevision", so the app crashes as soon as Java calls one. Build the app again, and the engine SDK with "haylen sdk build --target android" when the library still lacks them.',
            )
        ]
