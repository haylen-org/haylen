import re

import pytest

from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.folders import Folders
from haylen.platforms.apple.apple_project import AppleProject
from haylen.platforms.generated_tree import GeneratedTree
from tests.platforms.apple.apple_fixture import AppleFixture


class AppleProjectTest:
    """The generated Xcode project of an app: written the same way for the same inputs, with one app target that compiles the Apple parts of the plugins for Lua and C++ apps alike, and the files of the app in place of the files of the template."""

    CAMERA = {
        "implementations": {"ios": "native", "macos": "native"},
        "contributions": [
            {"type": "appleClass", "platforms": ["ios", "macos"], "name": "CameraPlugin"},
            {"type": "appleSources", "platforms": ["ios", "macos"], "path": "apple"},
            {"type": "framework", "platforms": ["ios", "macos"], "name": "AVFoundation.framework"},
            {"type": "swiftPackage", "platforms": ["ios", "macos"], "name": "Lens", "url": "https://example.com/lens.git", "exactVersion": "2.1.0", "products": ["Lens"]},
            {"type": "binaryFramework", "platforms": ["ios"], "path": "apple/Vendor.xcframework", "embed": True},
            {"type": "infoPlist", "platforms": ["ios", "macos"], "values": {"NSCameraUsageDescription": "Scans codes."}},
            {"type": "buildScript", "platforms": ["ios"], "name": "Stamp", "script": "echo stamp"},
        ],
    }

    @pytest.fixture
    def apple(self, tmp_path, monkeypatch) -> AppleFixture:
        return AppleFixture(tmp_path, monkeypatch)

    def camera(self, apple: AppleFixture, app) -> None:
        folder = apple.plugin(app, "camera", AppleProjectTest.CAMERA)
        (folder / "apple" / "Vendor.xcframework").mkdir(parents=True)
        (folder / "apple" / "CameraPlugin.swift").write_text("// The camera.\n", encoding="utf-8")

    def test_preparing_twice_writes_nothing(self, apple):
        app = apple.app({"plugins": {"camera": {}}})
        self.camera(apple, app)
        first = apple.prepare(app)
        spec = first.folders.project / AppleProject.SPEC
        written = spec.stat().st_mtime_ns
        second = apple.prepare(app)
        assert first.changed
        assert not second.changed
        assert spec.stat().st_mtime_ns == written
        assert [command[1] for command in apple.recorded.commands if command[0] == "/tools/xcodegen"] == ["generate"]

    def test_a_deleted_project_prepares_into_the_same_files(self, apple):
        app = apple.app({"plugins": {"camera": {}}})
        self.camera(apple, app)
        first = apple.prepare(app)
        Folders.remove(first.folders.project)
        second = apple.prepare(app)
        assert second.state.output == first.state.output
        assert GeneratedTree(second.folders.project).root == first.folders.project

    def test_lua_and_cpp_apps_take_the_same_parts_of_a_plugin(self, apple):
        lua = apple.app({"plugins": {"camera": {}}})
        cpp = apple.cpp_app({"plugins": {"camera": {}}})
        for app in (lua, cpp):
            self.camera(apple, app)
        lua_target = AppleFixture.spec(apple.prepare(lua))["targets"]["App"]
        cpp_target = AppleFixture.spec(apple.prepare(cpp))["targets"]["App"]

        def plugin_parts(target: dict) -> tuple:
            sources = [source for source in target["sources"] if source.get("group") == "Plugins"]
            plugins = [source | {"path": source["path"].split("/camera/")[1]} for source in sources]
            dependencies = [dependency for dependency in target["dependencies"] if "package" in dependency or "sdk" in dependency or "Vendor" in dependency.get("framework", "")]
            vendor = [dependency | {"framework": dependency["framework"].split("/camera/")[1]} if "framework" in dependency else dependency for dependency in dependencies]
            return plugins, vendor, target["postBuildScripts"]

        assert plugin_parts(lua_target) == plugin_parts(cpp_target)
        assert any(dependency.get("framework", "").endswith("HaylenPlayer.xcframework") for dependency in lua_target["dependencies"])
        assert not any(dependency.get("framework", "").endswith("HaylenPlayer.xcframework") for dependency in cpp_target["dependencies"])
        assert {"path": str(cpp / "src"), "group": "App", "type": "syncedFolder"} in cpp_target["sources"]
        assert cpp_target["settings"]["base"]["HEADER_SEARCH_PATHS"] == ["$(inherited)", str(cpp / "include")]
        assert cpp_target["settings"]["base"]["GCC_PREPROCESSOR_DEFINITIONS"] == ["$(inherited)", *AppleFixture.DEFINITIONS, "GAME=1"]

    def test_the_swift_packages_of_plugins_are_packages_of_the_project(self, apple):
        app = apple.app({"plugins": {"camera": {}}})
        self.camera(apple, app)
        spec = AppleFixture.spec(apple.prepare(app))
        assert spec["packages"] == {"Lens": {"url": "https://example.com/lens.git", "exactVersion": "2.1.0"}}
        assert {"package": "Lens", "product": "Lens"} in spec["targets"]["App"]["dependencies"]

    def test_two_plugins_with_one_swift_package_of_different_versions_fail(self, apple):
        app = apple.app({"plugins": {"first": {}, "second": {}}})
        for name, version in (("first", "2.1.0"), ("second", "2.2.0")):
            package = {"type": "swiftPackage", "platforms": ["ios"], "name": "Lens", "url": "https://example.com/lens.git", "exactVersion": version, "products": ["Lens"]}
            apple.plugin(app, name, {"implementations": {"ios": "native"}, "contributions": [package]})
        with pytest.raises(InvalidContribution, match=r'"first" .* and "second"'):
            apple.resolve(app)

    def test_the_files_of_the_app_replace_the_files_of_the_template(self, apple):
        app = apple.app({"platforms": {"apple": {"files": {"Assets.xcassets": "icons", "main.m": "native/main.m"}}}})
        (app / "icons" / "app-icon.appiconset").mkdir(parents=True)
        (app / "icons" / "app-icon.appiconset" / "Contents.json").write_text("{}", encoding="utf-8")
        (app / "native").mkdir()
        (app / "native" / "main.m").write_text("// Own main.\n", encoding="utf-8")
        project = apple.prepare(app).folders.project
        assert (project / "main.m").read_text(encoding="utf-8") == "// Own main.\n"
        assert sorted(path.relative_to(project / "Assets.xcassets").as_posix() for path in (project / "Assets.xcassets").rglob("*") if path.is_file()) == ["app-icon.appiconset/Contents.json"]

    def test_files_name_only_paths_of_the_template(self, apple):
        app = apple.app({"platforms": {"apple": {"files": {"project.yml": "haylen.json"}}}})
        with pytest.raises(InvalidManifest, match=re.escape('"platforms.apple.files.project.yml"')):
            apple.prepare(app)

    def test_the_oldest_system_version_goes_in_the_section_of_a_platform(self, apple):
        app = apple.app({"platforms": {"apple": {"minimumSystemVersion": "17.0"}}})
        with pytest.raises(InvalidManifest) as raised:
            apple.resolve(app)
        assert (
            'The section "platforms.apple" is named after no platform, so it takes no "minimumSystemVersion", which belongs to one platform. The sections "platforms.catalyst", "platforms.ios", "platforms.macos", "platforms.tvos" and "platforms.visionos" take it.'
            in raised.value.message
        )

    def test_the_sources_of_the_app_compile_into_the_app_target(self, apple):
        app = apple.app({"platforms": {"apple": {"sources": ["native/apple"], "buildSettings": {"SWIFT_VERSION": "6.0"}}}})
        (app / "native" / "apple").mkdir(parents=True)
        target = AppleFixture.spec(apple.prepare(app))["targets"]["App"]
        assert {"path": str(app / "native" / "apple"), "group": "App", "type": "syncedFolder"} in target["sources"]
        assert target["settings"]["base"]["SWIFT_VERSION"] == "6.0"

    def test_build_settings_never_set_what_the_backend_owns(self, apple):
        app = apple.app({"platforms": {"apple": {"buildSettings": {"SWIFT_VERSION": "6.0", "ARCHS": "x86_64", "IPHONEOS_DEPLOYMENT_TARGET": "12.0", "PRODUCT_BUNDLE_IDENTIFIER": "com.other"}}}})
        with pytest.raises(InvalidManifest) as raised:
            apple.prepare(app)
        message = raised.value.message
        assert 'The key "platforms.apple.buildSettings.ARCHS" sets a value that comes from the architectures of the target, which "--architecture" chooses.' in message
        assert (
            '"platforms.apple.buildSettings.IPHONEOS_DEPLOYMENT_TARGET" sets a value that comes from the oldest system version, which "minimumSystemVersion" of the section of the platform settles.'
            in message
        )
        assert '"platforms.apple.buildSettings.PRODUCT_BUNDLE_IDENTIFIER" sets a value that comes from "identifier" of "haylen.json".' in message
        assert "SWIFT_VERSION" not in message

    def test_devices_sign_with_the_team_of_the_app(self, apple):
        app = apple.app({"platforms": {"apple": {"team": "ABCDE12345"}}})
        settings = AppleFixture.spec(apple.prepare(app, "ios"))["targets"]["App"]["settings"]["base"]
        assert settings["DEVELOPMENT_TEAM"] == "ABCDE12345"
        unsigned = apple.app({}, "unsigned")
        with pytest.raises(InvalidManifest, match=re.escape('"platforms.apple.team"')):
            apple.prepare(unsigned, "ios")

    def test_simulators_and_this_mac_sign_ad_hoc(self, apple):
        app = apple.app({})
        for target in ("ios-simulator", "macos", "catalyst"):
            settings = AppleFixture.spec(apple.prepare(app, target))["targets"]["App"]["settings"]["base"]
            assert settings["CODE_SIGN_IDENTITY"] == "-"
            assert "DEVELOPMENT_TEAM" not in settings

    def test_the_target_of_the_platform_builds_the_architectures_of_the_product(self, apple):
        app = apple.app({})
        spec = AppleFixture.spec(apple.prepare(app, "catalyst"))
        target = spec["targets"]["App"]
        assert target["platform"] == "iOS"
        assert target["settings"]["base"]["SUPPORTS_MACCATALYST"] == "YES"
        assert spec["options"]["deploymentTarget"] == {"iOS": "16.4"}
        visionos = AppleFixture.spec(apple.prepare(app, "visionos-simulator"))["targets"]["App"]
        assert visionos["platform"] == "visionOS"
        assert visionos["settings"]["base"]["ARCHS"] == "arm64"
        assert not any(source["path"].endswith("LaunchScreen.storyboard") for source in visionos["sources"])

    def test_the_package_ships_as_it_is_in_debug(self, apple):
        app = apple.app({})
        project = apple.prepare(app).folders.project
        assert (project / "app" / "source" / "main.lua").is_file()
        assert "#if !DEBUG" in (project / AppleProject.BOOTSTRAP).read_text(encoding="utf-8")

    def test_an_extension_builds_as_a_target_that_the_app_embeds(self, apple):
        app = apple.app({"plugins": {"shelf": {}}})
        extension = {"type": "appExtension", "platforms": ["tvos"], "name": "Shelf", "kind": "appExtension", "point": "com.apple.tv-top-shelf", "sources": "shelf"}
        folder = apple.plugin(app, "shelf", {"implementations": {"tvos": "native"}, "contributions": [extension]})
        (folder / "shelf").mkdir()
        prepared = apple.prepare(app, "tvos-simulator")
        spec = AppleFixture.spec(prepared)
        assert spec["targets"]["Shelf"]["type"] == "app-extension"
        assert {"target": "Shelf", "embed": True} in spec["targets"]["App"]["dependencies"]
        assert AppleFixture.plist(prepared, "extensions/Shelf/Info.plist")["NSExtension"] == {"NSExtensionPointIdentifier": "com.apple.tv-top-shelf"}
