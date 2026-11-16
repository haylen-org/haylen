import json
import plistlib

import pytest

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.native.native_library import NativeLibrary
from haylen.platforms.apple.app_bundle import AppBundle
from haylen.platforms.apple.apple_devices import AppleDevices
from haylen.platforms.apple.apple_launcher import AppleLauncher
from haylen.platforms.apple.apple_native_libraries import AppleNativeLibraries
from haylen.platforms.apple.apple_targets import AppleTargets
from haylen.platforms.apple.apple_verifier import AppleVerifier
from haylen.platforms.apple.info_plist_keys import InfoPlistKeys
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.product_component import ProductComponent
from haylen.platforms.verification_check import VerificationCheck
from tests.platforms.apple.apple_fixture import AppleFixture


class AppleBuildTest:
    """What the Apple backend reads of built apps and of this Mac: the verification of an app bundle against what the engine and its plugins need, the registry of static native libraries, the simulators and devices, and the lines of the launcher of simulators."""

    LISTING = "\n".join(
        [
            "App.app/App (architecture x86_64):",
            "Contents of (__TEXT,__objc_classname) section",
            "000000010128a820  StorePlugin",
            "000000010128a837  _TtC9HaylenApp9AppBridge",
            "000000010128a84d  ",
            "App.app/App (architecture arm64):",
            "Contents of (__TEXT,__objc_classname) section",
            "0000000100f0a820  StorePlugin",
        ]
    )

    @pytest.fixture
    def apple(self, tmp_path, monkeypatch) -> AppleFixture:
        return AppleFixture(tmp_path, monkeypatch)

    def test_release_builds_find_plugin_classes_by_their_runtime_names(self):
        assert AppBundle.class_names(AppleBuildTest.LISTING) == {"StorePlugin", "_TtC9HaylenApp9AppBridge"}

    def built(self, apple: AppleFixture, info: dict, linked: str, classes: str) -> BuildProduct:
        app = apple.app({"plugins": {"store": {}}})
        contributions = [
            {"type": "appleClass", "platforms": ["ios"], "name": "StorePlugin"},
            {"type": "framework", "platforms": ["ios"], "name": "StoreKit.framework"},
            {"type": "infoPlist", "platforms": ["ios"], "values": {"NSCameraUsageDescription": "Scans codes.", "Store": "$(HAYLEN_BUNDLE_IDENTIFIER)"}},
        ]
        apple.plugin(app, "store", {"implementations": {"ios": "native"}, "contributions": contributions})
        prepared = apple.prepare(app)
        bundle = apple.folder / "Test App.app"
        bundle.mkdir()
        (bundle / "Info.plist").write_bytes(plistlib.dumps({"CFBundleExecutable": "Test App", "CFBundleIdentifier": "com.haylen.tests", **info}))
        (bundle / "Test App").write_text("", encoding="utf-8")
        (bundle / "PrivacyInfo.xcprivacy").write_bytes((apple.sdk.folder / "PrivacyInfo.xcprivacy").read_bytes())
        architectures = " ".join(sorted({project.target.architecture for project in prepared.product.projects}))
        answers = {"lipo": f"{architectures}\n", "otool -L": linked, "otool -v": classes, "codesign": "", "segedit": ""}
        apple.recorded.answer = lambda command: next(answer for key, answer in answers.items() if " ".join(command).startswith(key))
        components = tuple(ProductComponent(project.target, bundle / "Test App", "code") for project in prepared.product.projects)
        return BuildProduct(prepared, bundle, "app", components, ())

    def test_an_app_with_every_part_passes_the_verification(self, apple):
        info = {
            "UIApplicationSceneManifest": {},
            "UISupportedInterfaceOrientations": list(InfoPlistKeys.PHONE_ORIENTATIONS),
            "UISupportedInterfaceOrientations~ipad": list(InfoPlistKeys.PAD_ORIENTATIONS),
            "CADisableMinimumFrameDurationOnPhone": True,
            "UIStatusBarHidden": True,
        }
        info |= {"NSCameraUsageDescription": "Reworded.", "Store": "com.haylen.tests"}
        linked = "Test App:\n\t/System/Library/Frameworks/UIKit.framework/UIKit (compat)\n\t/System/Library/Frameworks/Metal.framework/Metal (compat)\n\t/System/Library/Frameworks/StoreKit.framework/StoreKit (compat)\n"
        product = self.built(apple, info, linked, AppleBuildTest.LISTING)
        report = AppleVerifier(apple.context).verify(product, AppleTargets.named("ios-simulator"))
        assert report.failures == ()

    def test_the_verification_names_what_a_built_app_lacks(self, apple):
        product = self.built(apple, {"UIStatusBarHidden": True}, "Test App:\n", "")
        report = AppleVerifier(apple.context).verify(product, AppleTargets.named("ios-simulator"))
        failed = {check.name: check.detail for check in report.checks if check.outcome is VerificationCheck.Outcome.FAILED}
        assert '"StoreKit.framework" of the plugin "store"' in failed["frameworks"]
        assert '"StorePlugin" of the plugin "store"' in failed["classes"]
        assert '"NSCameraUsageDescription"' in failed["infoPlist"]
        assert '"UIApplicationSceneManifest"' in failed["infoPlist"]

    def test_static_libraries_register_their_symbols_for_lua(self, tmp_path):
        library = NativeLibrary("native-test-static", "app", {}, tmp_path, ("ios",), True, ("native_test_add", "native_test_scale"))
        source = AppleNativeLibraries.registry([library])
        assert 'extern "C" void native_test_add(void);' in source
        assert 'registerLinked("native-test-static", {{"native_test_add", reinterpret_cast<void*>(&native_test_add)}, {"native_test_scale", reinterpret_cast<void*>(&native_test_scale)}});' in source

    def test_only_ios_and_tvos_choose_to_link_a_library_statically(self, apple):
        files = {platform: "native/libvendor.a" for platform in ("ios", "tvos", "catalyst", "visionos", "macos")}
        app = apple.app({"native": {"vendor": {"files": files, "link": "static", "symbols": ["vendor_open"]}}})
        (app / "native").mkdir()
        (app / "native" / "libvendor.a").write_text("archive", encoding="utf-8")
        for target in ("ios-simulator", "tvos-simulator"):
            assert apple.resolve(app, target).primary.native_libraries[0].static

        for target, platform in (("catalyst", "catalyst"), ("visionos-simulator", "visionos"), ("macos", "macos")):
            with pytest.raises(InvalidManifest) as raised:
                apple.resolve(app, target)
            assert raised.value.message == (
                f'The native library "vendor" of "app" links statically, which only the apps of iOS and tvOS and the apps of targets that load no dynamic library do, and the apps of "{target}" load it dynamically. Leave "{platform}" out of its platforms, or make it dynamic.'
            )

    def test_static_libraries_link_into_the_app_with_their_registry(self, apple):
        app = apple.app({"native": {"vendor": {"files": {"ios": "native/libvendor.a"}, "link": "static", "symbols": ["vendor_open"]}}})
        (app / "native").mkdir()
        (app / "native" / "libvendor.a").write_text("archive", encoding="utf-8")
        prepared = apple.prepare(app)
        target = AppleFixture.spec(prepared)["targets"]["App"]
        assert target["settings"]["base"]["OTHER_LDFLAGS"] == ["$(inherited)", str(app / "native" / "libvendor.a")]
        assert {"path": str(prepared.folders.project / AppleNativeLibraries.REGISTRY)} in target["sources"]
        assert "vendor_open" in (prepared.folders.project / AppleNativeLibraries.REGISTRY).read_text(encoding="utf-8")

    def test_dynamic_libraries_embed_without_linking(self, apple):
        app = apple.app({"native": {"vendor": {"files": {"macos": "native/libvendor.dylib"}}}})
        (app / "native").mkdir()
        (app / "native" / "libvendor.dylib").write_text("library", encoding="utf-8")
        target = AppleFixture.spec(apple.prepare(app, "macos"))["targets"]["App"]
        assert {"framework": str(app / "native" / "libvendor.dylib"), "embed": True, "link": False, "codeSign": True} in target["dependencies"]

    def test_simulators_and_devices_list_with_their_targets(self, apple):
        simulators = {
            "devices": {
                "com.apple.CoreSimulator.SimRuntime.iOS-26-0": [{"udid": "A", "name": "iPhone 17e", "state": "Booted"}],
                "com.apple.CoreSimulator.SimRuntime.xrOS-26-0": [{"udid": "B", "name": "Apple Vision Pro", "state": "Shutdown"}],
            }
        }
        apple.recorded.answer = lambda command: json.dumps(simulators)
        found = AppleDevices(apple.context).simulators()
        assert [(device.identifier, device.state, device.targets) for device in found] == [("A", "booted", ("ios-simulator",)), ("B", "shutdown", ("visionos-simulator",))]

    def test_the_launcher_of_simulators_leaves_out_its_own_lines(self):
        assert AppleLauncher.simulator_line("com.haylen.tests", "com.haylen.tests: 4242") is None
        assert AppleLauncher.simulator_line("com.haylen.tests", "Child process terminated with signal 15: Terminated") is None
        assert AppleLauncher.simulator_line("com.haylen.tests", "Hello") == "Hello"
