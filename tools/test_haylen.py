"""Tests of the rules of haylen.py that need no build: how it merges Info.plist keys, entitlements and privacy manifests, what it checks in a built app, when it generates App.xcodeproj again, how it prints to the terminal and how its commands read their arguments."""

import argparse
import ast
import base64
import collections
import dataclasses
import contextlib
import functools
import hashlib
import http.client
import importlib.util
import http.server
import io
import json
import os
import plistlib
import re
import shlex
import shutil
import signal
import socket
import subprocess
import sys
import tarfile
import tempfile
import textwrap
import threading
import time
import tokenize
import types
import unicodedata
import unittest
import urllib.parse
import zipfile
from collections.abc import Iterator
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import haylen  # noqa: E402


class PluginKeysTest(unittest.TestCase):
    def test_objects_merge_and_arrays_gain_missing_items(self):
        merged = {"SKAdNetworkItems": [{"SKAdNetworkIdentifier": "a"}], "Settings": {"first": 1}}
        owners = {"SKAdNetworkItems": '"app.json"', "Settings": '"app.json"'}
        haylen.merge_plugin_keys(merged, {"SKAdNetworkItems": [{"SKAdNetworkIdentifier": "a"}, {"SKAdNetworkIdentifier": "b"}], "Settings": {"second": 2}}, owners, 'the plugin "ads"', '"Info.plist"')
        self.assertEqual(merged, {"SKAdNetworkItems": [{"SKAdNetworkIdentifier": "a"}, {"SKAdNetworkIdentifier": "b"}], "Settings": {"first": 1, "second": 2}})

    def test_two_plugins_that_disagree_stop_the_build_with_both_named(self):
        merged, owners = {}, {}
        haylen.merge_plugin_keys(merged, {"GADApplicationIdentifier": "one"}, owners, 'the plugin "first"', '"Info.plist"')
        with self.assertRaisesRegex(haylen.BuildError, 'the plugin "first" and "two" for the plugin "second"'):
            haylen.merge_plugin_keys(merged, {"GADApplicationIdentifier": "two"}, owners, 'the plugin "second"', '"Info.plist"')


    def test_a_parameter_of_some_platforms_leaves_its_keys_out_of_the_others(self):
        manifest = {"id": "camera", "version": "1.0.0", "platforms": ["ios", "macos"], "parameters": {"macDevices": {"type": "boolean", "platforms": ["macos"], "default": True, "description": "Mac entitlements."}, "usage": {"type": "string", "default": "Shows the camera.", "description": "Usage text."}},
            "apple": {"infoPlist": {"NSCameraUsageDescription": "${usage}"}, "entitlements": {"com.apple.security.device.camera": "${macDevices}"}}}
        camera = haylen.Plugin(Path(tempfile.gettempdir()), manifest, None)
        app = mock.Mock(plugins=[camera], plugin_values={"camera": {"macDevices": True, "usage": "Shows the camera."}})
        self.assertEqual(haylen.apple_plugin_keys(app, ("ios",), "entitlements", {}, {}, "entitlements"), {})
        self.assertEqual(haylen.apple_plugin_keys(app, ("macos",), "entitlements", {}, {}, "entitlements"), {"com.apple.security.device.camera": True})
        self.assertEqual(haylen.apple_plugin_keys(app, ("ios", "catalyst"), "infoPlist", {}, {}, '"Info.plist"'), {"NSCameraUsageDescription": "Shows the camera."})


class DeveloperKeysTest(unittest.TestCase):
    def test_the_value_of_the_developer_wins_with_a_warning(self):
        warnings: list[str] = []
        completed = haylen.complete_keys({"NSCameraUsageDescription": "Ours"}, {"NSCameraUsageDescription": "Theirs"}, {"NSCameraUsageDescription": 'the plugin "camera"'}, "ios/Info.plist", warnings)
        self.assertEqual(completed, {"NSCameraUsageDescription": "Ours"})
        self.assertEqual(warnings, ['The key "NSCameraUsageDescription" of `ios/Info.plist` keeps its value "Ours", while the plugin "camera" gives "Theirs".'])

    def test_generated_keys_fill_what_the_developer_lacks(self):
        warnings: list[str] = []
        own = {"UIRequiresFullScreen": True, "CFBundleURLTypes": [{"CFBundleURLSchemes": ["own"]}], "UIApplicationSceneManifest": {"UIApplicationSupportsMultipleScenes": True}}
        generated = {"UISupportedInterfaceOrientations": ["UIInterfaceOrientationPortrait"], "CFBundleURLTypes": [{"CFBundleURLSchemes": ["demo"]}], "UIApplicationSceneManifest": {"UISceneConfigurations": {}}}
        owners = {key: '"app.json"' for key in generated}
        completed = haylen.complete_keys(own, generated, owners, "ios/Info.plist", warnings)
        self.assertEqual(completed["UIRequiresFullScreen"], True)
        self.assertEqual(completed["UISupportedInterfaceOrientations"], ["UIInterfaceOrientationPortrait"])
        self.assertEqual(completed["CFBundleURLTypes"], [{"CFBundleURLSchemes": ["own"]}, {"CFBundleURLSchemes": ["demo"]}])
        self.assertEqual(completed["UIApplicationSceneManifest"], {"UIApplicationSupportsMultipleScenes": True, "UISceneConfigurations": {}})
        self.assertEqual(warnings, [])
        self.assertEqual(own["CFBundleURLTypes"], [{"CFBundleURLSchemes": ["own"]}])


class PrivacyTest(unittest.TestCase):
    def test_reasons_join_their_type_and_tracking_wins(self):
        engine = {"NSPrivacyAccessedAPITypes": [{"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryFileTimestamp", "NSPrivacyAccessedAPITypeReasons": ["C617.1"]}]}
        plugin = {"NSPrivacyTracking": True, "NSPrivacyTrackingDomains": ["ads.example.com"], "NSPrivacyAccessedAPITypes": [{"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryFileTimestamp", "NSPrivacyAccessedAPITypeReasons": ["3B52.1"]}]}
        developer = {"NSPrivacyTracking": False, "NSPrivacyTrackingDomains": ["ads.example.com", "own.example.com"]}
        merged = haylen.merge_privacy([engine, plugin, developer])
        self.assertEqual(merged["NSPrivacyAccessedAPITypes"], [{"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryFileTimestamp", "NSPrivacyAccessedAPITypeReasons": ["C617.1", "3B52.1"]}])
        self.assertTrue(merged["NSPrivacyTracking"])
        self.assertEqual(merged["NSPrivacyTrackingDomains"], ["ads.example.com", "own.example.com"])

    def test_a_manifest_without_keys_adds_nothing(self):
        self.assertEqual(haylen.merge_privacy([{}, {}]), {})


class FormatTest(unittest.TestCase):
    def test_multi_line_lambdas_need_markers_while_the_array_forms_of_new_and_delete_do_not(self):
        scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, scratch)
        source = scratch / "Source.cpp"
        source.write_text("\n".join([
            "void* operator new[](std::size_t size) {",
            "}",
            "void operator delete[](void* memory, std::size_t) noexcept {",
            "}",
            "const auto call = [this](int value) {",
            "};",
            "// clang-format off",
            "const auto guarded = [&value] {",
            "};",
            "// clang-format on",
        ]))
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            findings = haylen.unguarded_lambdas([source])
        self.assertEqual(len(findings), 1)
        self.assertIn("Source.cpp:5", findings[0])


class RequirementTest(unittest.TestCase):
    def test_a_built_value_holds_the_items_and_keys_it_needs(self):
        built = {"CFBundleURLTypes": [{"CFBundleURLName": "other"}, {"CFBundleURLName": "demo", "CFBundleURLSchemes": ["demo", "more"]}], "NSCameraUsageDescription": "Reworded", "com.apple.developer.game-center": True}
        self.assertTrue(haylen.holds(built, {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["demo"]}]}))
        self.assertTrue(haylen.holds(built, {"NSCameraUsageDescription": "The text of the plugin"}))
        self.assertFalse(haylen.holds(built, {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["missing"]}]}))
        self.assertFalse(haylen.holds(built, {"NSMicrophoneUsageDescription": "Needed"}))
        self.assertFalse(haylen.holds({"com.apple.developer.game-center": False}, {"com.apple.developer.game-center": True}))

    def test_plugin_values_name_the_build_settings_of_the_identity_of_the_app(self):
        app = mock.Mock(identifier="com.haylen.music", version="1.2.0", build=4)
        app.name = "Music"
        apple = {"entitlements": {"com.apple.security.application-groups": ["group.$(HAYLEN_BUNDLE_IDENTIFIER)"]}, "infoPlist": {"Version": "$(MARKETING_VERSION)", "Build": "$(CURRENT_PROJECT_VERSION)", "Other": "$(SRCROOT)/file"}}
        self.assertEqual(haylen.expand_app_settings(apple, haylen.apple_app_settings(app)), {"entitlements": {"com.apple.security.application-groups": ["group.com.haylen.music"]}, "infoPlist": {"Version": "1.2.0", "Build": "4", "Other": "$(SRCROOT)/file"}})

    def test_release_builds_find_plugin_classes_by_their_runtime_names(self):
        listing = "\n".join(["App.app/App (architecture x86_64):", "Contents of (__TEXT,__objc_classname) section", "000000010128a820  StorePlugin", "000000010128a837  _TtC9HaylenApp9AppBridge", "000000010128a84d  ", "App.app/App (architecture arm64):", "Contents of (__TEXT,__objc_classname) section", "0000000100f0a820  StorePlugin"])
        self.assertEqual(haylen.apple_objc_classes(listing), {"StorePlugin", "_TtC9HaylenApp9AppBridge"})

    def test_the_entitlements_of_a_universal_simulator_app_come_from_a_slice_of_its_executable(self):
        written = {"com.apple.security.application-groups": ["group.com.haylen.music"]}
        def run(command, **_):
            if command[0] == "codesign":
                return subprocess.CompletedProcess(command, 0, plistlib.dumps({}), b"")
            Path(command[-1]).write_bytes(plistlib.dumps(written))
            self.assertNotEqual(command[1], "App", "The section comes from one slice, not the universal executable.")
            return subprocess.CompletedProcess(command, 0, b"", b"")
        with mock.patch.object(haylen.subprocess, "run", side_effect=run), mock.patch.object(haylen, "capture", return_value="x86_64 arm64\n"), mock.patch.object(haylen, "run") as thin:
            self.assertEqual(haylen.apple_entitlements(Path("App.app"), Path("App")), written)
        self.assertEqual([str(part) for part in thin.call_args.args[0][:4]], ["lipo", "App", "-thin", "x86_64"])

    def test_a_snippet_holds_the_key_and_its_value(self):
        self.assertEqual(haylen.plist_snippet("NSCameraUsageDescription", "Scans codes."), "<key>NSCameraUsageDescription</key>\n<string>Scans codes.</string>")
        self.assertEqual(plistlib.loads(("<plist><dict>" + haylen.plist_snippet("UIApplicationSceneManifest", {"UIApplicationSupportsMultipleScenes": True}) + "</dict></plist>").encode()), {"UIApplicationSceneManifest": {"UIApplicationSupportsMultipleScenes": True}})

    def test_a_requirement_names_the_file_and_keeps_the_snippet_as_it_is(self):
        stdout, stderr = io.StringIO(), io.StringIO()
        requirement = haylen.Requirement('The plugin "demo" needs "android.permission.CAMERA".', "Add to `app/src/main/AndroidManifest.xml`:", '<uses-permission android:name="android.permission.CAMERA" />')
        with mock.patch.object(haylen, "terminal", haylen.Terminal(stdout, stderr, {"FORCE_COLOR": "1"})):
            haylen.report_requirements([requirement])
        lines = stderr.getvalue().splitlines()
        self.assertEqual(lines[0], "\x1b[1;33mWarning:\x1b[0m The plugin \x1b[38;5;73mdemo\x1b[0m needs \x1b[38;5;73mandroid.permission.CAMERA\x1b[0m.")
        self.assertEqual(lines[1], "  Add to app/src/main/AndroidManifest.xml:")
        self.assertEqual(lines[2], '    <uses-permission android:name="android.permission.CAMERA" />')
        self.assertEqual(stdout.getvalue(), "")


class ProjectGenerationTest(unittest.TestCase):
    def test_a_project_without_the_include_is_never_generated(self):
        self.assertEqual(haylen.apple_project_action(False, {}, "inputs", "edited", "template"), "keep")

    def test_a_project_generated_from_the_same_inputs_is_kept_with_its_edits(self):
        self.assertEqual(haylen.apple_project_action(True, {"inputs": "inputs", "project": "project"}, "inputs", "project", "template"), "keep")
        self.assertEqual(haylen.apple_project_action(True, {"inputs": "inputs", "project": "project"}, "inputs", "edited", "template"), "keep")

    def test_new_inputs_generate_a_project_without_edits(self):
        self.assertEqual(haylen.apple_project_action(True, {"inputs": "old", "project": "project"}, "inputs", "project", "template"), "generate")

    def test_a_missing_project_or_an_untouched_copy_of_the_template_is_generated(self):
        self.assertEqual(haylen.apple_project_action(True, {"inputs": "inputs", "project": "project"}, "inputs", None, "template"), "generate")
        self.assertEqual(haylen.apple_project_action(True, {}, "inputs", "template", "template"), "generate")

    def test_new_inputs_for_a_project_with_edits_need_a_comparison(self):
        self.assertEqual(haylen.apple_project_action(True, {"inputs": "old", "project": "project"}, "inputs", "edited", "template"), "compare")
        self.assertEqual(haylen.apple_project_action(True, {}, "inputs", "cloned", "template"), "compare")

    def test_the_inputs_follow_the_files_of_the_plugins(self):
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            (root / "project.yml").write_text("name: App\n")
            (root / "haylen" / "plugins" / "demo" / "sources").mkdir(parents=True)
            (root / "haylen" / "project.yml").write_text("{}\n")
            first = haylen.apple_project_inputs(root)
            (root / "haylen" / "plugins" / "demo" / "sources" / "Demo.swift").write_text("")
            self.assertNotEqual(haylen.apple_project_inputs(root), first)


class StatusBarTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.scratch)
        self.app = self.scratch / "app"
        self.app.mkdir()
        self.root = self.scratch / "apple"
        for folder, _, _ in haylen.APPLE_TEMPLATES.values():
            (self.root / folder).mkdir(parents=True, exist_ok=True)
            (self.root / folder / "Info.plist").write_bytes(plistlib.dumps({}))

    def launch(self, window: dict) -> dict:
        (self.app / "app.json").write_text(json.dumps({"name": "App", "identifier": "com.example.app", "version": "1.0", "window": window}))
        generated = self.scratch / "generated"
        haylen.write_apple_plists(haylen.App(self.app, "ios"), self.root, generated)
        return plistlib.loads((generated / "ios" / "Info.plist").read_bytes())

    def test_the_app_launches_with_the_status_bar_of_app_json(self):
        self.assertTrue(self.launch({})["UIStatusBarHidden"])
        shown = self.launch({"statusBar": "dark"})
        self.assertEqual((shown["UIStatusBarHidden"], shown["UIStatusBarStyle"]), (False, "UIStatusBarStyleDarkContent"))
        with self.assertRaises(haylen.BuildError):
            self.launch({"statusBar": "white"})

    def test_iphones_may_refresh_above_60_hertz(self):
        self.assertTrue(self.launch({})["CADisableMinimumFrameDurationOnPhone"])


class BackgroundAudioTest(unittest.TestCase):
    def write_app(self, folder: Path, audio: dict) -> Path:
        shutil.copytree(haylen.COMMON_TEMPLATE / "content", folder / "content")
        document = json.loads((haylen.COMMON_TEMPLATE / "app.json").read_text(encoding="utf-8"))
        (folder / "app.json").write_text(json.dumps({**document, "audio": audio}), encoding="utf-8")
        return folder

    def test_apple_apps_that_play_in_the_background_get_the_audio_background_mode(self):
        with tempfile.TemporaryDirectory() as scratch:
            app = self.write_app(Path(scratch) / "app", {"iosSession": "playback", "background": True})
            root = Path(scratch) / "apple"
            for folder, _, _ in haylen.APPLE_TEMPLATES.values():
                (root / folder).mkdir(parents=True, exist_ok=True)
                (root / folder / "Info.plist").write_bytes(plistlib.dumps({"UIBackgroundModes": ["fetch"]} if folder == "ios" else {}))
            generated = Path(scratch) / "generated"
            haylen.write_apple_plists(haylen.App(app, "ios"), root, generated)
            plists = {folder: plistlib.loads((generated / folder / "Info.plist").read_bytes()) for folder in ("ios", "tvos", "visionos", "macos")}
        # The modes of the developer stay, the mode of the audio joins them, and a Mac app needs none.
        self.assertEqual(plists["ios"]["UIBackgroundModes"], ["fetch", "audio"])
        self.assertEqual(plists["tvos"]["UIBackgroundModes"], ["audio"])
        self.assertEqual(plists["visionos"]["UIBackgroundModes"], ["audio"])
        self.assertNotIn("UIBackgroundModes", plists["macos"])

    def test_android_apps_that_play_in_the_background_depend_on_the_playback_library(self):
        with tempfile.TemporaryDirectory() as scratch:
            values = {}
            for name, audio in (("radio", {"iosSession": "playback", "background": True}), ("game", {})):
                app = self.write_app(Path(scratch) / name, audio)
                root = Path(scratch) / f"{name}-android"
                with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
                    haylen.prepare_android(haylen.App(app, "android"), root, "haylen", "Debug", 1)
                values[name] = (root / "haylen" / "haylen.properties").read_text(encoding="utf-8").splitlines()
        self.assertIn("engineLibraries=haylen-playback", values["radio"])
        self.assertIn("engineLibraries=", values["game"])
        self.assertIn("haylen-playback", haylen.ANDROID_LIBRARIES)
        self.assertIn('haylen.getProperty("engineLibraries")', (haylen.COMMON_TEMPLATE / "platform" / "android" / "app" / "build.gradle.kts").read_text(encoding="utf-8"))


class DesktopLinkTest(unittest.TestCase):
    def setUp(self):
        scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, scratch)
        self.root = scratch / "links"
        self.root.mkdir()
        self.manifest = {"id": "links", "name": "Links", "version": "1.0.0", "description": "Opens the app from links.", "platforms": ["windows", "linux"], "parameters": {"scheme": {"type": "string", "default": "island-links", "description": "The scheme."}}, "desktop": {"urlSchemes": ["${scheme}", "island"]}}

    def app(self) -> mock.Mock:
        plugin = haylen.Plugin(self.root, self.manifest, None)
        app = mock.Mock(plugins=[plugin], plugin_values={"links": {"scheme": "island-links"}}, executable="tiny-island", identifier="com.haylen.tinyisland")
        app.name = 'Tiny "Island"'
        return app

    def test_plugins_declare_the_url_schemes_of_their_links_on_the_desktops(self):
        self.assertEqual(haylen.PluginManifestCheck.problems_of(self.root, self.manifest), [])
        self.assertEqual(haylen.desktop_url_schemes(self.app(), "linux"), ["island-links", "island"])
        self.manifest["desktop"] = {"urlSchemes": ["1island"], "schemes": []}
        self.assertEqual(haylen.PluginManifestCheck.problems_of(self.root, self.manifest), ['The key "desktop.schemes" is not a key of "plugin.json".', 'The key "desktop.urlSchemes" must list the URL schemes of the links that open the app, such as "camera-scanner", or name a string parameter that holds one, such as "${scheme}".'])

    def test_the_installer_of_windows_registers_each_scheme_with_the_executable_and_the_link(self):
        script = haylen.windows_link_script(self.app(), ["island"])
        self.assertIn('Root: HKA; Subkey: "Software\\Classes\\island"; ValueType: string; ValueName: ""; ValueData: "URL:Tiny ""Island"""; Flags: uninsdeletekey', script)
        self.assertIn('Subkey: "Software\\Classes\\island\\shell\\open\\command"; ValueType: string; ValueName: ""; ValueData: """{app}\\tiny-island.exe"" --link ""%1"""', script)

    def test_the_desktop_entry_of_linux_takes_the_links_of_its_schemes(self):
        app = self.app()
        app.description = "An island."
        resources = self.root / "linux" / "resources"
        resources.mkdir(parents=True)
        (resources / "app.desktop").write_text("[Desktop Entry]\nType=Application\n")
        (resources / "app.metainfo.xml").write_text("<component/>")
        with mock.patch.object(haylen, "complete_metainfo", return_value=""), mock.patch.object(haylen, "appstream_elements", return_value=[]):
            haylen.write_linux_entries(app, self.root / "linux", self.root / "generated")
        entry = (self.root / "generated" / "app.desktop").read_text(encoding="utf-8").splitlines()
        self.assertIn("Exec=tiny-island --link %u", entry)
        self.assertIn("MimeType=x-scheme-handler/island-links;x-scheme-handler/island;", entry)


class SplashTest(unittest.TestCase):
    def test_the_dark_background_becomes_the_dark_appearance_and_the_night_resources(self):
        colorset = haylen.apple_colorset((16, 20, 24, 255), (0, 0, 0, 255))
        self.assertEqual(len(colorset["colors"]), 2)
        self.assertNotIn("appearances", colorset["colors"][0])
        self.assertEqual(colorset["colors"][1]["appearances"], [{"appearance": "luminosity", "value": "dark"}])
        self.assertEqual(colorset["colors"][1]["color"]["components"]["red"], "0.000")
        self.assertEqual(len(haylen.apple_colorset((16, 20, 24, 255))["colors"]), 1)

        with tempfile.TemporaryDirectory() as scratch:
            resources = Path(scratch)
            haylen.write_android_splash(mock.Mock(background=(16, 20, 24, 255), dark_background=None, splash_logo=None), resources)
            self.assertIn("#FF101418", (resources / "values" / "haylen_splash.xml").read_text())
            self.assertFalse((resources / "values-night").exists())
            haylen.write_android_splash(mock.Mock(background=(16, 20, 24, 255), dark_background=(0, 0, 0, 128), splash_logo=None), resources)
            self.assertIn("#80000000", (resources / "values-night" / "haylen_splash.xml").read_text())

    def test_the_web_splash_writes_backgrounds_into_config(self):
        with tempfile.TemporaryDirectory() as scratch:
            site = Path(scratch)
            (site / "haylen.wasm").write_bytes(b"\x00asm")
            app = types.SimpleNamespace(
                folder=site,
                name="App",
                identifier="com.example.app",
                transparent=False,
                splash_logo=None,
                background=(250, 250, 248, 255),
                dark_background=(16, 20, 24, 255),
                plugin_values={},
                plugins=[],
            )
            haylen.write_web_settings(app, site, release=[])
            config = json.loads((site / "config.json").read_text(encoding="utf-8"))
            splash = config["splash"]
            self.assertEqual(splash["background"], "rgba(250, 250, 248, 1.000)")
            self.assertEqual(splash["darkBackground"], "rgba(16, 20, 24, 1.000)")


class PropertiesTest(unittest.TestCase):
    def test_values_survive_as_ascii_with_escapes(self):
        self.assertEqual(haylen.java_property("Ilha Tropical ✓\\n"), "Ilha Tropical \\u2713\\\\n")


class StoryboardTest(unittest.TestCase):
    def test_storyboards_of_apple_tv_are_documents_of_apple_tv(self):
        # The interface compiler of tvOS fails without a message on a storyboard of the iOS document type.
        for path in [*haylen.ENGINE_DIR.glob("platform/apple/**/*.storyboard"), *haylen.TEMPLATES_DIR.glob("**/*.storyboard")]:
            text = path.read_text(encoding="utf-8")
            self.assertEqual('targetRuntime="AppleTV"' in text, 'type="com.apple.InterfaceBuilder.AppleTV.Storyboard"' in text, path)


class StarterAppTest(unittest.TestCase):
    def test_the_starter_app_describes_itself_apart_from_its_name(self):
        # The desktop entry of Linux takes its comment and the AppStream metainfo its summary from the description, which validators reject when it repeats the name.
        app = haylen.App(haylen.COMMON_TEMPLATE, "linux")
        self.assertNotEqual(app.description, app.name)


class AndroidNdkTest(unittest.TestCase):
    def test_the_android_project_strips_native_libraries_with_the_ndk_of_the_engine(self):
        # Without the NDK that built them, the Android Gradle plugin packs the native libraries of a debug build with all their debug information.
        with tempfile.TemporaryDirectory() as scratch:
            app = Path(scratch) / "app"
            shutil.copytree(haylen.COMMON_TEMPLATE / "content", app / "content")
            shutil.copy2(haylen.COMMON_TEMPLATE / "app.json", app / "app.json")
            root = Path(scratch) / "android"
            with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
                haylen.prepare_android(haylen.App(app, "android"), root, "main", "Debug", 1)
            properties = (root / "haylen" / "haylen.properties").read_text(encoding="utf-8")
        self.assertIn(f"ndkVersion={haylen.ANDROID_NDK_VERSION}", properties.splitlines())
        self.assertIn('ndkVersion = haylen.getProperty("ndkVersion")', (haylen.COMMON_TEMPLATE / "platform" / "android" / "app" / "build.gradle.kts").read_text(encoding="utf-8"))


class BuildNumberTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.scratch)
        self.app = self.scratch / "app"
        shutil.copytree(haylen.COMMON_TEMPLATE / "content", self.app / "content")
        self.document = json.loads((haylen.COMMON_TEMPLATE / "app.json").read_text(encoding="utf-8"))

    def write_app(self, **values) -> haylen.App:
        (self.app / "app.json").write_text(json.dumps(self.document | values))
        return haylen.App(self.app, "android")

    def test_new_apps_start_at_build_1_apart_from_their_version(self):
        self.assertEqual((self.document["version"], self.document["build"]), ("1.0.0", 1))
        del self.document["build"]
        self.assertEqual(self.write_app().build, 1)

    def test_the_build_number_is_the_version_code_and_the_bundle_version(self):
        app = self.write_app(version="2.4.1", build=12)
        root = self.scratch / "android"
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            haylen.prepare_android(app, root, "main", "Debug", 1)
        properties = (root / "haylen" / "haylen.properties").read_text(encoding="utf-8").splitlines()
        self.assertIn("versionName=2.4.1", properties)
        self.assertIn("versionCode=12", properties)
        settings = haylen.apple_app_settings(app)
        self.assertEqual((settings["MARKETING_VERSION"], settings["CURRENT_PROJECT_VERSION"]), ("2.4.1", "12"))

    def test_a_build_number_is_a_whole_number_from_1(self):
        for value in (0, -2, 1.5, "3", True, None, haylen.MAXIMUM_BUILD + 1):
            with self.subTest(value=value), self.assertRaisesRegex(haylen.BuildError, f'The "build" of `.*` must be a whole number from 1 to {haylen.MAXIMUM_BUILD} that grows with every upload of the app'):
                self.write_app(build=value)
        self.assertEqual(self.write_app(build=haylen.MAXIMUM_BUILD).build, haylen.MAXIMUM_BUILD)


class TextFileTest(unittest.TestCase):
    def test_every_text_file_is_read_and_written_as_utf_8(self):
        # Windows reads and writes text in its own code page by default, which turns the copyright sign of `app.json` into a byte that no UTF-8 tool reads.
        tree = ast.parse(Path(haylen.__file__).read_text(encoding="utf-8"))
        calls = [node for node in ast.walk(tree) if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr in ("read_text", "write_text")]
        self.assertTrue(calls)
        self.assertEqual([node.lineno for node in calls if not any(keyword.arg == "encoding" for keyword in node.keywords)], [])

    def test_a_text_outside_the_code_page_of_a_pipe_prints_without_stopping_the_command(self):
        # Python writes to a pipe or a file of Windows in the code page of the system, which lacks most characters of the names and the logs of apps.
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch) / "omega"
            result = subprocess.run([sys.executable, str(haylen.ROOT / "haylen.py"), "new", str(folder), "--name", "Game Ω"], env={**os.environ, "PYTHONIOENCODING": "cp1252", "PYTHONUTF8": "0", "NO_COLOR": "1"}, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", "replace"))
            self.assertIn(b'Created the Lua app "Game ?"', result.stdout)
            self.assertEqual(json.loads((folder / "app.json").read_text(encoding="utf-8"))["name"], "Game Ω")


class FileNameTest(unittest.TestCase):
    def test_tracked_files_use_dashes_where_nothing_requires_underscores(self):
        root = Path(__file__).resolve().parent.parent
        files = subprocess.run(["git", "ls-files"], cwd=root, capture_output=True, text=True, check=True).stdout.splitlines()
        imported = set()
        for name in files:
            if name.endswith(".py"):
                for node in ast.walk(ast.parse((root / name).read_text(encoding="utf-8"))):
                    if isinstance(node, ast.Import):
                        imported.update(alias.name.split(".")[0] for alias in node.names)
                    elif isinstance(node, ast.ImportFrom) and node.level == 0:
                        imported.add(node.module.split(".")[0])

        def allowed(name: str) -> bool:
            # Android resource names allow only letters, digits and underscores, Python imports modules and discovers tests by names that are identifiers, and GitHub reads issue templates only from the folder ".github/ISSUE_TEMPLATE".
            if name.startswith(".github/ISSUE_TEMPLATE/"):
                return "_" not in name.removeprefix(".github/ISSUE_TEMPLATE/")
            folder, separator, _ = name.partition("/src/main/res/")
            if separator:
                return "_" not in folder
            path = Path(name)
            return path.suffix == ".py" and "_" not in path.parent.as_posix() and (path.stem in imported or path.name.startswith("test_"))

        self.assertEqual([name for name in files if "_" in name and not allowed(name)], [], "These tracked files and folders use underscores where their names take dashes.")


class CMakeNameTest(unittest.TestCase):
    # The first argument of these commands names a target or a project, also in the CMake text that haylen.py writes inside its strings, and the word after "COMPONENT" names an install component.
    DEFINITION = re.compile(r"^[\s'\"]*(?:add_library|add_executable|add_custom_target|haylen_add_app|project)\s*\(\s*([^\s)]+)")
    COMPONENT = re.compile(r"\bCOMPONENT\s+([^\s)]+)")

    def test_targets_projects_and_components_use_dashes(self):
        root = Path(__file__).resolve().parent.parent
        files = subprocess.run(["git", "ls-files", "*CMakeLists.txt", "*.cmake", "*.cmake.in", "haylen.py"], cwd=root, capture_output=True, text=True, check=True).stdout.splitlines()
        underscored = []
        for name in files:
            for line in (root / name).read_text(encoding="utf-8").splitlines():
                if line.lstrip().startswith("#"):
                    continue
                for match in [*self.DEFINITION.finditer(line), *self.COMPONENT.finditer(line)]:
                    # A variable names nothing by itself, and the tokens of the plugin template that become identifiers count as underscores.
                    named = re.sub(r"\$\{[^}]*\}", "", match.group(1))
                    if "_" in named or "{{SYMBOL}}" in named or "{{MACRO}}" in named:
                        underscored.append(f"{name}: {match.group(1)}")
        self.assertEqual(underscored, [], "These CMake targets, projects and install components use underscores where their names take dashes.")


class LayerIsolationTest(unittest.TestCase):
    """The engine reaches SDL only through its platform layer and each graphics API only through its GPU device, so the Lua API never changes with the platform or the graphics API that draws."""

    SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cpp", ".m", ".mm"}

    def sources(self) -> dict[str, str]:
        root = Path(__file__).resolve().parent.parent
        files = subprocess.run(["git", "ls-files", "engine"], cwd=root, capture_output=True, text=True, check=True).stdout.splitlines()
        return {name: (root / name).read_text(encoding="utf-8") for name in files if Path(name).suffix in self.SOURCE_SUFFIXES}

    def test_only_the_platform_layer_of_sdl_reaches_sdl(self):
        library = re.compile(r'#\s*(include|import)\s*[<"]SDL3/|\bSDL_\w+\s*\(')
        reaching = [name for name, text in self.sources().items() if not re.match(r"engine/(src|tests)/platform/sdl/", name) and library.search(text)]
        self.assertEqual(reaching, [], "These files outside the platform layer of SDL include the headers of SDL or call it.")

    def test_only_the_gpu_devices_include_the_headers_of_their_graphics_apis(self):
        api = re.compile(r'#\s*(include|import)\s*[<"](Metal/|QuartzCore/CAMetalLayer|d3d1[12]|dxgi|vulkan/|volk\.h|GLES\d?/|GL/|webgpu/)')
        including = [name for name, text in self.sources().items() if not re.match(r"engine/(src/gpu|tests/gpu|tests/support)/", name) and api.search(text)]
        self.assertEqual(including, [], "These files outside the GPU devices include the headers of a graphics API.")

    def test_the_lua_bindings_know_no_host(self):
        binding = re.compile(r"engine/src/(lua/|.*Lua\.(cpp|hpp)$)")
        host = re.compile(r'#\s*include\s*"(gpu/NullDevice|platform/(NativeHost|NativeWindow|Player|Services)\.hpp|platform/(sdl|apple|android|web|windows|linux|headless)/)')
        knowing = [name for name, text in self.sources().items() if binding.match(name) and host.search(text)]
        self.assertEqual(knowing, [], "These Lua bindings include a host or a platform folder, while the Lua API stays the same on every platform and graphics API.")


class TextInputTest(unittest.TestCase):
    """The text input of SDL reads keys as text only on Windows and Linux, where no native field edits, so keys in play on every other platform never reach an input method, such as the accent menu of macOS."""

    def test_only_the_text_input_of_windows_and_linux_starts_the_text_input_of_sdl(self):
        root = Path(__file__).resolve().parent.parent
        sources = LayerIsolationTest().sources()
        starting = [name for name, text in sources.items() if re.search(r"\bSDL_StartTextInput\w*\s*\(", text)]
        self.assertEqual(starting, ["engine/src/platform/sdl/TextInput.cpp"])
        build = (root / "engine/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertRegex(build, r'if\(HAYLEN_PLATFORM MATCHES "\^\(windows\|linux\)\$"\)\n\s*list\(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sdl/TextInput\.cpp\)')
        self.assertEqual(build.count("src/platform/sdl/TextInput.cpp"), 1)


class WritingTest(unittest.TestCase):
    root = Path(__file__).resolve().parent.parent

    def tracked(self, *patterns: str) -> list[str]:
        return subprocess.run(["git", "ls-files", "--", *patterns], cwd=self.root, capture_output=True, text=True, check=True).stdout.splitlines()

    def test_line_comments_are_sentences_that_start_with_a_capital_letter(self):
        # A comment that goes on in an indented line, such as a table under its sentence, and the markers of tools are no sentences of their own.
        markers = {"//": ("*.c", "*.cpp", "*.h", "*.hpp", "*.m", "*.mm", "*.js", "*.java", "*.kt", "*.kts", "*.swift", "*.glsl"), "--": ("*.lua",), "#": ("*.py", "*.cmake", "*CMakeLists.txt")}
        tools = ("clang-format", "NOLINT", "MARK:", "noqa", "type:", "pragma", "!", "#")
        wrong = []
        for marker, patterns in markers.items():
            for name in self.tracked(*patterns):
                if "/patches/" in name:
                    continue
                for number, line in enumerate((self.root / name).read_text(encoding="utf-8").splitlines(), 1):
                    stripped = line.lstrip()
                    if not stripped.startswith(marker) or stripped.startswith(marker * 2 if marker != "--" else "---") or stripped.startswith("--[["):
                        continue
                    text = stripped[len(marker):]
                    if not text.strip() or text.startswith(("  ", "\t")) or text.strip().startswith(tools):
                        continue
                    if text.strip()[0].islower() or text.rstrip()[-1] not in ".:!?)`\"]":
                        wrong.append(f"{name}:{number}: {text.strip()}")
        self.assertEqual(wrong, [], "These comments are no complete sentences that start with a capital letter.")

    def test_texts_that_lua_gives_the_interface_start_with_a_capital_letter(self):
        # A value without a space is data, such as an id, a path or a word that the app echoes, and code shown as code keeps its own form.
        option = re.compile(r"""(?<![\w.])(text|title|hint|caption|message|prompt|label|subtitle|description|tooltip)\s*=\s*(['"])((?:\\.|(?!\2).)*)\2""")
        names = ("iOS", "iPhone", "iPad", "iPadOS", "macOS", "tvOS", "visionOS")
        wrong = []
        for name in self.tracked("samples/*.lua", "templates/*.lua", "engine/bench/*.lua"):
            for number, line in enumerate((self.root / name).read_text(encoding="utf-8").splitlines(), 1):
                if line.lstrip().startswith("--") or "'monospace'" in line:
                    continue
                for match in option.finditer(line):
                    value = match.group(3)
                    if re.search(r"bind\s*=\s*\{[^}]*$", line[: match.start()]) or " " not in value or "(" in value or value.startswith(names):
                        continue
                    if value[0].islower():
                        wrong.append(f"{name}:{number}: {value}")
        self.assertEqual(wrong, [], "These texts of the interface start with a lowercase letter.")

    def test_no_file_names_another_engine_or_the_authors_and_tools_of_third_party_art(self):
        # The project plan records the requests of the owner as they were made, and the bytes of binary files hold the metadata that tools write. The names are kept encoded, so this test names none of them itself.
        words = base64.b64decode("a2VubmV5CnJ1YmJlcmR1Y2sKcmFuZG9tbWluZApvcGVuZ2FtZWFydAphc2Vwcml0ZQp0ZXh0dXJlcGFja2VyCnBpeGVsbWF0b3IKZ2FtZXN5bnRoCnNvdW5kYm9vdGgKZmwgc3R1ZGlvCmxvZ2ljIGV4cHJlc3MKYWRtb2IKZmlyZWJhc2UKYnVubnltYXJrCmdvZG90CnVucmVhbCBlbmdpbmUKY29jb3MyZApkZWZvbGQKZ2FtZW1ha2VyCnBoYXNlcgpwaXhpanMKZ29vZ2xlIHNpZ24KZ29vZ2xlIGlkZW50aXR5CmVwaWMgb25saW5l").split(b"\n")
        names = re.compile(b"|".join(re.escape(word) for word in words), re.IGNORECASE)
        found = [name for name in self.tracked() if name != "PROJECT.md" and (self.root / name).is_file() and names.search((self.root / name).read_bytes())]
        self.assertEqual(found, [], "These files name another engine, a third-party SDK or the author or tool of third-party art.")


class DocsTest(unittest.TestCase):
    root = Path(__file__).resolve().parent.parent
    link = re.compile(r"(?<!!)\[([^\]]*)\]\(([^)\s]+)\)")
    header = "| Name | Description | Link |"

    def pages(self, *patterns: str) -> list[Path]:
        names = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "--", *(patterns or ("*.md",))], cwd=self.root, capture_output=True, text=True, check=True).stdout.splitlines()
        return [self.root / name for name in sorted(set(names)) if (self.root / name).is_file()]

    @staticmethod
    def prose(path: Path) -> Iterator[tuple[int, str]]:
        """Yields the numbered lines of a page outside its code blocks and the front matter that GitHub reads from an issue template."""
        inside = False
        lines = path.read_text(encoding="utf-8").splitlines()
        start = lines.index("---", 1) + 1 if lines and lines[0] == "---" else 0
        for number, line in enumerate(lines[start:], start + 1):
            if line.lstrip().startswith("```"):
                inside = not inside
                continue
            if not inside:
                yield number, line

    @classmethod
    def anchors(cls, path: Path) -> set[str]:
        """The anchors of the headings of a page, made the way GitHub makes them."""
        found, seen = set(), collections.Counter()
        for _, line in cls.prose(path):
            heading = re.match(r"^#{1,6} (.+)$", line)
            if heading:
                text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", heading.group(1)).strip().lower()
                slug = re.sub(r"[^\w\- ]", "", text).replace(" ", "-")
                found.add(slug if seen[slug] == 0 else f"{slug}-{seen[slug]}")
                seen[slug] += 1
        return found

    def links(self, path: Path) -> Iterator[tuple[int, str, str]]:
        """Yields the number, text and target of every link of a page outside code."""
        for number, line in self.prose(path):
            for match in self.link.finditer(re.sub(r"`[^`]*`(?!\])", "", line)):
                yield number, match.group(1), match.group(2)

    @classmethod
    def tables(cls, path: Path) -> Iterator[tuple[str, list[str]]]:
        """Yields every table of a page with the section it sits in, as its lines."""
        section, rows = "", []
        for _, line in [*cls.prose(path), (0, "")]:
            if line.startswith("|"):
                rows.append(line)
                continue
            if rows:
                yield section, rows
                rows = []
            if line.startswith("## "):
                section = line[3:]

    def indexes(self) -> list[Path]:
        return [self.root / "docs" / "README.md", *sorted((self.root / "docs").glob("*/README.md"))]

    def test_every_page_starts_with_a_title_that_starts_with_an_emoji(self):
        # The main page opens with the logo, the features of the plugin template are parts of the page of a new plugin, and an issue template opens with the front matter whose name GitHub shows.
        wrong = []
        for path in self.pages():
            name = path.relative_to(self.root).as_posix()
            if name == "README.md" or name.startswith(("templates/plugin-features/", ".github/ISSUE_TEMPLATE/")):
                continue
            title = path.read_text(encoding="utf-8").split("\n", 1)[0]
            if not title.startswith("# ") or unicodedata.category(title[2:3]) != "So":
                wrong.append(f"{name}: {title}")
        self.assertEqual(wrong, [], "These pages do not start with a title like \"# 🎨 Rendering\".")

    def test_every_code_block_names_its_language(self):
        wrong = []
        for path in self.pages():
            inside = False
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                if not line.lstrip().startswith("```"):
                    continue
                if not inside and line.strip() == "```":
                    wrong.append(f"{path.relative_to(self.root).as_posix()}:{number}")
                inside = not inside
        self.assertEqual(wrong, [], "These code blocks name no language, such as \"```lua\".")

    def test_every_paragraph_is_one_line(self):
        wrong = []
        for path in self.pages():
            previous = False
            for number, line in self.prose(path):
                text = bool(line.strip()) and not re.match(r"^\s*([|#>*+-]|\d+\.|<)", line)
                if text and previous:
                    wrong.append(f"{path.relative_to(self.root).as_posix()}:{number}")
                previous = text
        self.assertEqual(wrong, [], "These lines continue the paragraph above them. Write each paragraph on one line.")

    def test_relative_links_reach_their_pages_and_headings(self):
        anchors, wrong = {}, []
        for path in self.pages():
            for number, _, target in self.links(path):
                if re.match(r"^[a-z]+:", target):
                    continue
                file, _, anchor = target.partition("#")
                destination = (path.parent / file).resolve() if file else path.resolve()
                where = f"{path.relative_to(self.root).as_posix()}:{number}: {target}"
                if not destination.exists():
                    wrong.append(f"{where} names no file")
                elif anchor and destination.suffix == ".md":
                    if destination not in anchors:
                        anchors[destination] = self.anchors(destination)
                    if anchor not in anchors[destination]:
                        wrong.append(f"{where} names no heading")
        self.assertEqual(wrong, [], "These links lead nowhere.")

    def test_the_main_page_links_the_documentation_only_through_its_index(self):
        targets = [target for _, _, target in self.links(self.root / "README.md") if target.startswith(("docs/", "./docs/"))]
        self.assertEqual(targets, ["docs/README.md"], "The main page links the documentation once, through \"docs/README.md\".")

    def test_every_page_of_the_documentation_is_reached_from_the_index_through_its_block(self):
        docs = self.root / "docs"
        blocks = {(docs / target).resolve() for _, rows in self.tables(docs / "README.md") for row in rows for _, _, target in self.links_of(row)}
        self.assertEqual(blocks, {path.resolve() for path in docs.glob("*/README.md")}, "The tables of the index link the index of every block and nothing else.")
        reached = {(path.parent / target).resolve() for path in docs.glob("*/README.md") for _, rows in self.tables(path) for row in rows for _, _, target in self.links_of(row)}
        pages = {path.resolve() for path in docs.rglob("*.md") if path.name != "README.md"}
        self.assertEqual(sorted(path.relative_to(self.root).as_posix() for path in pages - reached), [], "The index of no block lists these pages.")
        self.assertEqual(sorted(path.relative_to(docs).as_posix() for path in docs.rglob("*.md") if path.parent.parent != docs and path.parent != docs), [], "Pages live in the folder of their block, one level under the docs.")

    def test_index_tables_list_name_description_and_link_in_alphabetical_order(self):
        wrong = []
        for path in self.indexes():
            name = path.relative_to(self.root).as_posix()
            for section, rows in self.tables(path):
                if rows[0] != self.header or len(rows) < 3:
                    wrong.append(f"{name}, {section}: the table needs the header \"{self.header}\" and rows")
                    continue
                names = []
                for row in rows[2:]:
                    cells = [cell.strip() for cell in row.strip("|").split(" | ")]
                    if len(cells) != 3 or not re.fullmatch(r"\[`[^`]+`\]\([^)\s]+\)", cells[2]):
                        wrong.append(f"{name}, {section}: {row}")
                        continue
                    names.append(re.sub(r"[^\w .]", "", cells[0].replace("`", "")).strip().casefold())
                if names != sorted(names):
                    wrong.append(f"{name}, {section}: the rows are out of alphabetical order")
        self.assertEqual(wrong, [], "These tables of the indexes break the form \"| Name | Description | Link |\" in alphabetical order.")

    def test_no_table_of_an_index_mixes_guides_with_the_lua_reference(self):
        wrong = []
        reference = (self.root / "docs" / "lua-api").resolve()
        for path in self.indexes():
            for section, rows in self.tables(path):
                kinds = {(path.parent / target).resolve().parent == reference for row in rows for _, _, target in self.links_of(row)}
                if len(kinds) > 1:
                    wrong.append(f"{path.relative_to(self.root).as_posix()}, {section}")
        self.assertEqual(wrong, [], "These tables list guides and reference pages together.")

    def links_of(self, row: str) -> Iterator[tuple[int, str, str]]:
        for match in self.link.finditer(row):
            yield 0, match.group(1), match.group(2)

    def test_no_long_sentence_repeats_in_another_page_of_the_documentation(self):
        # A subject lives in one page, and the other pages link to it. Reference pages share the sentences that describe the same rule of every module, such as how options are checked.
        seen, wrong = {}, []
        for path in [self.root / "README.md", *self.pages("docs/*.md")]:
            name = path.relative_to(self.root).as_posix()
            for _, line in self.prose(path):
                if line.startswith(("|", "#")):
                    continue
                for sentence in re.split(r"(?<=\.)\s+(?=[A-Z])", line.strip()):
                    first = seen.setdefault(sentence, name) if len(sentence) >= 100 else name
                    if first != name and not (first.startswith("docs/lua-api/") and name.startswith("docs/lua-api/")):
                        wrong.append(f"{first} and {name}: {sentence}")
        self.assertEqual(wrong, [], "These sentences repeat in two pages. Keep the subject in one page and link to it.")


class SpriteSheetTest(unittest.TestCase):
    @staticmethod
    def checker():
        path = Path(__file__).resolve().parent / "check-sprite-sheets.py"
        spec = importlib.util.spec_from_file_location("check_sprite_sheets", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_every_animated_sheet_of_the_samples_is_declared_and_holds_its_pivots(self):
        checker = self.checker()
        sheets = json.loads(checker.MANIFEST.read_text())["sheets"]
        self.assertEqual(checker.undeclared(sheets) + checker.check(sheets), [], "Run \"python3 tools/check-sprite-sheets.py\" and align or paint again the frames of these clips.")

    def test_a_frame_off_the_pivot_of_its_clip_drifts_and_aligned_frames_hold_it(self):
        checker = self.checker()
        image = checker.Image.blank(64, 32)
        frames = [checker.Frame(0, 0, 32, 32), checker.Frame(32, 0, 32, 32)]

        def square(left: int, top: int) -> None:
            for y in range(top, top + 8):
                for x in range(left, left + 8):
                    image.pixels[(y * 64 + x) * 4 + 3] = 255

        square(12, 12)
        square(32 + 18, 12)
        self.assertIn("6.0 pixels sideways", checker.drift(frames, image, "center"))
        image = checker.Image.blank(64, 32)
        square(12, 12)
        square(32 + 12, 12)
        self.assertIsNone(checker.drift(frames, image, "center"))


class ImageEdgeTest(unittest.TestCase):
    @staticmethod
    def checker():
        path = Path(__file__).resolve().parent / "check-image-edges.py"
        spec = importlib.util.spec_from_file_location("check_image_edges", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_every_image_of_the_samples_and_templates_has_clean_edges(self):
        self.assertEqual(self.checker().check(), [], "Run \"python3 tools/check-image-edges.py\" and clean or paint again these images.")

    def test_magenta_left_from_the_background_and_jagged_edges_are_found(self):
        checker = self.checker()
        image = checker.Image.blank(16, 16)
        image.pixels[(8 * 16 + 8) * 4 : (8 * 16 + 8) * 4 + 4] = bytes((250, 10, 245, 90))
        self.assertEqual(checker.leftovers(image), [8 * 16 + 8])

        smooth = checker.Image.blank(32, 32)
        stairs = checker.Image.blank(32, 32)
        for y in range(32):
            for x in range(32):
                inside = (x - 16) ** 2 + (y - 16) ** 2
                smooth.pixels[(y * 32 + x) * 4 + 3] = 255 if inside < 100 else (128 if inside < 144 else 0)
                stairs.pixels[(y * 32 + x) * 4 + 3] = 255 if x + y < 32 and x > 2 and y > 2 else 0
        self.assertEqual(checker.jagged(smooth), 0.0)
        self.assertGreater(checker.jagged(stairs), checker.JAGGED_SHARE)


class TerminalStream(io.StringIO):
    """A stream that answers like a terminal."""

    def isatty(self) -> bool:
        return True


class TerminalTest(unittest.TestCase):
    def test_without_color_reserved_expressions_keep_their_quotes_and_paths_and_urls_print_bare(self):
        message = 'Serving `build/apps/demo-70da5b67/web` at `http://127.0.0.1:8000/` with "--coep off"'
        self.assertEqual(haylen.Terminal.render(message, False), 'Serving build/apps/demo-70da5b67/web at http://127.0.0.1:8000/ with "--coep off"')

    def test_with_color_reserved_expressions_and_urls_take_their_styles_paths_stay_plain_and_the_line_keeps_its_own(self):
        rendered = haylen.Terminal.render('Run "python3 haylen.py run" in `apps/demo` and open `http://127.0.0.1:8000/`.', True, haylen.Terminal.SUCCESS)
        self.assertEqual(rendered, "\x1b[32mRun \x1b[38;5;73mpython3 haylen.py run\x1b[0m\x1b[32m in apps/demo and open \x1b[4mhttp://127.0.0.1:8000/\x1b[0m\x1b[32m.\x1b[0m")

    def test_an_empty_quote_stays_as_it_is(self):
        self.assertEqual(haylen.Terminal.render('The key "name" is "".', True), 'The key \x1b[38;5;73mname\x1b[0m is "".')

    def test_a_step_marks_its_start_and_rows_align_their_names(self):
        stdout = io.StringIO()
        terminal = haylen.Terminal(stdout, io.StringIO(), {"NO_COLOR": "1"})
        terminal.step('Building the app for "ios-simulator"')
        terminal.rows([("App", 'Demo ("com.haylen.demo")'), ("Configuration", '"Debug"')])
        self.assertEqual(stdout.getvalue().splitlines(), ['==> Building the app for "ios-simulator"', '  App            Demo ("com.haylen.demo")', '  Configuration  "Debug"'])

    @mock.patch.object(haylen.Terminal, "enable_escapes", return_value=True)
    def test_color_follows_the_terminal_and_the_environment(self, _):
        self.assertTrue(haylen.Terminal.shows_color(TerminalStream(), {}))
        self.assertFalse(haylen.Terminal.shows_color(io.StringIO(), {}))
        self.assertTrue(haylen.Terminal.shows_color(io.StringIO(), {"FORCE_COLOR": "1"}))
        self.assertFalse(haylen.Terminal.shows_color(TerminalStream(), {"NO_COLOR": "1"}))
        self.assertFalse(haylen.Terminal.shows_color(TerminalStream(), {"NO_COLOR": "1", "FORCE_COLOR": "1"}))
        self.assertFalse(haylen.Terminal.shows_color(TerminalStream(), {"TERM": "dumb"}))
        self.assertTrue(haylen.Terminal.shows_color(TerminalStream(), {"NO_COLOR": ""}))

    def test_the_serve_line_keeps_its_url_bare_without_color(self):
        stdout = io.StringIO()
        terminal = haylen.Terminal(stdout, io.StringIO(), {"NO_COLOR": "1"})
        terminal.success("Serving `build/apps/demo-70da5b67/web` at `http://127.0.0.1:8000/`")
        self.assertEqual(stdout.getvalue(), "Serving build/apps/demo-70da5b67/web at http://127.0.0.1:8000/\n")

    def test_an_error_is_one_block_with_the_output_of_the_tool_as_it_is(self):
        stderr = io.StringIO()
        terminal = haylen.Terminal(io.StringIO(), stderr, {})
        terminal.error('The shader `content/shaders/tint.glsl` does not compile as the "sprite" program.', "tint.glsl:3: error: 'tint' undeclared")
        self.assertEqual(stderr.getvalue(), "Error: The shader content/shaders/tint.glsl does not compile as the \"sprite\" program.\ntint.glsl:3: error: 'tint' undeclared\n")

    def test_a_command_prints_as_a_line_to_copy_and_dim(self):
        stdout = io.StringIO()
        haylen.Terminal(stdout, io.StringIO(), {"FORCE_COLOR": "1"}).command(["cmake", "-S", "my app", '-DNAME="value"'])
        expected = haylen.Terminal.command_line(["cmake", "-S", "my app", '-DNAME="value"'])
        self.assertEqual(stdout.getvalue(), f"\x1b[2m$ {expected}\x1b[0m\n")
        self.assertIn("my app", expected)

    def test_a_long_progress_line_loses_its_middle_and_keeps_its_start_and_end(self):
        stdout = io.StringIO()
        line = "\x1b[1;32m[ 42%]\x1b[0m Building CXX object engine/src/2d/graphics/Renderer.cpp.o"
        with mock.patch.object(haylen.shutil, "get_terminal_size", return_value=os.terminal_size((31, 24))):
            haylen.Terminal(stdout, io.StringIO(), {"FORCE_COLOR": "1"}).progress(line)
        self.assertEqual(stdout.getvalue(), "\r\x1b[2K\x1b[2m[ 42%] Building\u2026Renderer.cpp.o\x1b[0m")
        self.assertEqual(haylen.Terminal.shorten("Compiling", 9), "Compiling")
        self.assertEqual(haylen.Terminal.shorten("\u6f22\u5b57\u6f22\u5b57\u6f22\u5b57", 7), "\u6f22\u2026\u6f22\u5b57")

    def test_an_expected_error_stops_without_a_traceback(self):
        stderr = io.StringIO()
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), stderr, {})), mock.patch.object(sys, "argv", ["haylen.py", "run"]):
            with self.assertRaises(SystemExit) as stopped:
                haylen.main()
        self.assertEqual(stopped.exception.code, 1)
        self.assertTrue(stderr.getvalue().startswith("Error: The command needs the folder of an app"))
        self.assertNotIn("Traceback", stderr.getvalue())


class QuietCommandTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.scratch)
        self.stdout = io.StringIO()
        self.terminal = haylen.Terminal(self.stdout, io.StringIO(), {"NO_COLOR": "1"})
        self.terminal.log = self.scratch / "logs" / "run.log"
        patcher = mock.patch.object(haylen, "terminal", self.terminal)
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_a_tool_keeps_its_output_in_the_log_of_the_command(self):
        haylen.run([sys.executable, "-c", "print('compiled 3 files')"])
        self.assertEqual(self.stdout.getvalue(), "")
        self.assertIn("compiled 3 files", self.terminal.log.read_text())

    def test_a_failure_shows_the_last_lines_and_names_the_log(self):
        with self.assertRaises(haylen.BuildError) as raised:
            haylen.run([sys.executable, "-c", "import sys; print('\\n'.join(f'line {n}' for n in range(100))); sys.exit(2)"])
        self.assertIn("failed with exit code 2", str(raised.exception))
        self.assertIn(haylen.shown_path(self.terminal.log), str(raised.exception))
        self.assertTrue(raised.exception.details.endswith("line 99"))
        self.assertNotIn("line 39\n", raised.exception.details + "\n")

    def test_verbose_and_streamed_commands_print_their_output(self):
        self.terminal.verbose = True
        haylen.run([sys.executable, "-c", "print('compiled')"])
        self.assertFalse(self.terminal.log.exists())
        self.assertIn("$ ", self.stdout.getvalue())

    def test_the_output_of_a_tool_reads_as_utf_8_in_a_locale_of_another_encoding(self):
        # The tools write UTF-8, while a Python outside its UTF-8 mode decodes the output of a command in the encoding of the locale, such as the code page of Windows, unless it is told otherwise.
        # The tools write UTF-8, while a Python outside its UTF-8 mode decodes the output of a command in the encoding of the locale, such as the code page of Windows, unless it is told otherwise.
        script = textwrap.dedent("""
            import locale, sys
            try:
                locale.setlocale(locale.LC_CTYPE, "en_US.ISO8859-1")
            except locale.Error:
                sys.exit(3)
            sys.path.insert(0, sys.argv[1])
            import haylen
            print(ascii(haylen.capture([sys.executable, "-c", "import sys; sys.stdout.buffer.write(chr(233).encode())"])))
        """)
        child = subprocess.run([sys.executable, "-c", script, str(haylen.ROOT)], env={**os.environ, "PYTHONUTF8": "0"}, capture_output=True, text=True)
        if child.returncode == 3:
            self.skipTest("This machine has no locale of Latin-1.")
        self.assertEqual(child.stdout.strip(), ascii("é"), child.stderr)


class AppArgumentTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.scratch)
        previous = Path.cwd()
        os.chdir(self.scratch)
        self.addCleanup(os.chdir, previous)

    def test_run_without_an_app_explains_what_an_app_folder_is(self):
        self.assertIsNone(haylen.build_parser().parse_args(["run"]).app)
        for value in (None, ""):
            with self.assertRaisesRegex(haylen.BuildError, 'needs the folder of an app.*"app.json"'):
                haylen.resolve_app(value)

    def test_a_path_that_names_no_app_stops_with_the_reason(self):
        (self.scratch / "notes").mkdir()
        with self.assertRaisesRegex(haylen.BuildError, "The path `missing` does not exist"):
            haylen.resolve_app("missing")
        with self.assertRaisesRegex(haylen.BuildError, 'The path `notes` is not an app folder, because it holds no "app.json"'):
            haylen.resolve_app("notes")

    def test_an_app_resolves_from_the_current_folder_and_never_from_the_samples(self):
        (self.scratch / "game").mkdir()
        (self.scratch / "game" / "app.json").write_text("{}")
        self.assertEqual(haylen.resolve_app("game"), self.scratch / "game")
        self.assertEqual(haylen.resolve_app(str(self.scratch / "game")), self.scratch / "game")
        os.chdir(haylen.ROOT)
        with self.assertRaisesRegex(haylen.BuildError, "does not exist"):
            haylen.resolve_app("games/tiny-island")

    def test_the_player_of_this_machine_builds_with_the_engine_configuration(self):
        (self.scratch / "game").mkdir()
        (self.scratch / "game" / "app.json").write_text('{"name": "Game", "identifier": "com.example.game", "version": "1.0.0"}')
        builds = []

        def stop(options):
            builds.append(options)
            raise haylen.BuildError("Stop after the build.")

        for arguments, config in ((["run", "game"], "Release"), (["run", "game", "--config", "Release"], "Release"), (["run", "game", "--engine-config", "Debug"], "Debug")):
            with mock.patch.object(haylen, "compile_app_shaders"), mock.patch.object(haylen, "command_build", side_effect=stop), self.assertRaises(haylen.BuildError):
                haylen.command_run(haylen.build_parser().parse_args(arguments))
            self.assertEqual(builds[-1].config, config)


class ArtifactsTest(unittest.TestCase):
    def test_a_run_builds_only_the_apple_slice_it_needs_and_a_project_every_stale_slice(self):
        manifest = {"platforms": {"apple": {"config": "Release", "slices": {"ios-simulator": "current", "tvos": "old"}}}}
        builds = []
        with mock.patch.object(haylen, "read_manifest", return_value=manifest), mock.patch.object(haylen, "engine_sources_hash", return_value="current"), mock.patch.object(haylen, "build_artifacts", side_effect=lambda *arguments: builds.append(arguments)):
            haylen.ensure_artifacts("apple", "Release", 4, ["ios-simulator"])
            self.assertEqual(builds, [])
            haylen.ensure_artifacts("apple", "Release", 4, ["tvos"])
            self.assertEqual(builds[-1], ("apple", "Release", 4, ["tvos"]))
            haylen.ensure_artifacts("apple", "Release", 4)
            self.assertEqual(builds[-1], ("apple", "Release", 4, [name for name in haylen.APPLE_SLICES if name != "ios-simulator"]))
            haylen.ensure_artifacts("apple", "Debug", 4, ["ios-simulator"])
            self.assertEqual(builds[-1], ("apple", "Debug", 4, ["ios-simulator"]))
        self.assertEqual(haylen.APPLE_RUN_SLICES["ios"], ["ios", "ios-simulator"])
        self.assertEqual(haylen.APPLE_RUN_SLICES["tvos-simulator"], ["tvos", "tvos-simulator"])
        self.assertEqual(sorted(haylen.APPLE_RUN_SLICES), sorted(haylen.APPLE_RUNS))

    def test_the_engine_command_builds_the_apple_slices_it_names(self):
        builds = []
        with mock.patch.object(haylen, "build_artifacts", side_effect=lambda *arguments: builds.append(arguments)):
            haylen.command_engine(haylen.build_parser().parse_args(["engine", "--platform", "apple", "--slices", "tvos", "tvos-simulator", "--jobs", "4"]))
            haylen.command_engine(haylen.build_parser().parse_args(["engine", "--platform", "web", "--jobs", "4"]))
            with self.assertRaisesRegex(haylen.BuildError, "needs \"--platform apple\""):
                haylen.command_engine(haylen.build_parser().parse_args(["engine", "--platform", "android", "--slices", "tvos"]))
        self.assertEqual(builds, [("apple", "Release", 4, ["tvos", "tvos-simulator"]), ("web", "Release", 4, None)])

    def test_the_framework_joins_the_slices_built_now_and_the_current_ones_and_is_signed(self):
        manifest = {"platforms": {"apple": {"config": "Release", "slices": {"ios": "current", "tvos": "old"}}}}
        commands = []
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(haylen, "ENGINE_BUILDS_DIR", Path(folder)), mock.patch.object(haylen, "ARTIFACTS_DIR", Path(folder) / "artifacts"), mock.patch.object(haylen, "read_manifest", return_value=manifest), mock.patch.object(haylen, "write_manifest") as written, mock.patch.object(haylen, "engine_sources_hash", return_value="current"), mock.patch.object(haylen, "require_host"), mock.patch.object(haylen, "is_current_tree", return_value=True), mock.patch.object(haylen, "shutil") as files, mock.patch.object(haylen, "run", side_effect=lambda command, **_: commands.append([str(part) for part in command])), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            haylen.build_artifacts("apple", "Release", 4, ["ios-simulator"])
            expected = [str(haylen.apple_slice_library(name, "Release")) for name in ("ios", "ios-simulator")]
        created = next(command for command in commands if command[:2] == ["xcodebuild", "-create-xcframework"])
        self.assertEqual([created[index + 1] for index, part in enumerate(created) if part == "-library"], expected)
        output = created[-1]
        self.assertEqual(commands[-2:], [["codesign", "--sign", "-", "--timestamp=none", "--force", output], ["codesign", "--verify", "--strict", output]])
        self.assertEqual(written.call_args.args, ("apple", {"config": "Release", "slices": {"ios": "current", "ios-simulator": "current"}}))


class LauncherTest(unittest.TestCase):
    @unittest.skipIf(os.name == "nt", "The shell launcher runs on macOS and Linux.")
    def test_the_launcher_runs_the_tool_from_any_folder_and_through_a_link(self):
        with tempfile.TemporaryDirectory() as scratch:
            link = Path(scratch) / "haylen"
            link.symlink_to(haylen.ROOT / "haylen")
            for command in (haylen.ROOT / "haylen", link):
                result = subprocess.run([str(command), "--help"], cwd=scratch, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue(result.stdout.startswith("usage: haylen "))

    def test_the_version_option_prints_the_version_of_the_engine(self):
        result = subprocess.run([sys.executable, str(haylen.ROOT / "haylen.py"), "--version"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), f"Haylen {(haylen.ROOT / 'engine' / 'VERSION').read_text(encoding='utf-8').strip()}")

    def test_the_windows_launcher_hands_every_argument_to_the_tool(self):
        script = (haylen.ROOT / "haylen.cmd").read_text()
        self.assertIn('python "%~dp0haylen.py" %*', script)
        self.assertIn("exit /b %errorlevel%", script)


class PluginTemplateTest(unittest.TestCase):
    def test_a_new_plugin_has_a_native_part_for_every_platform(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch) / "camera-scanner"
            with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
                haylen.command_plugin_new(argparse.Namespace(folder=str(folder), id=None, features=[]))
            manifest = json.loads((folder / "plugin.json").read_text())
            self.assertEqual(manifest["platforms"], ["ios", "catalyst", "tvos", "visionos", "macos", "android", "web", "windows", "linux"])
            self.assertEqual(manifest["native"], {"cmake": "native", "platforms": ["macos", "windows", "linux"]})

            # The desktop library takes the id, the name that "native.load" finds, and its init function takes the id with underscores, since a C function is named with an identifier.
            source = (folder / "native" / "CameraScanner.c").read_text()
            self.assertIn("CAMERA_SCANNER_EXPORT int camera_scanner_haylen_init(const HaylenNativeApi* api)", source)
            self.assertIn('api->registerHandler("camera-scanner.echo"', source)
            self.assertIn("add_library(camera-scanner CameraScanner.c)", (folder / "native" / "CMakeLists.txt").read_text())
            self.assertIn("native.load('camera-scanner', {init = 'camera_scanner_haylen_init'})", (folder / "source" / "init.lua").read_text())
            self.assertFalse([path for path in folder.rglob("*") if "{{" in path.name or (path.is_file() and "{{" in path.read_text())])

    def test_a_plain_plugin_keeps_the_manifest_of_the_template_as_it_reads(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch) / "plain"
            with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
                haylen.command_plugin_new(argparse.Namespace(folder=str(folder), id=None, features=[]))
            template = (haylen.PLUGIN_TEMPLATE / "plugin.json").read_text().replace("{{ID}}", "plain").replace("{{TITLE}}", "Plain").replace("{{NAME}}", "Plain")
            self.assertEqual((folder / "plugin.json").read_text(), template)
            self.assertIn("The plugin has no parameters", (folder / "README.md").read_text())
            self.assertNotIn("\nimport ", (folder / "web" / "plain.js").read_text())

    def test_features_bring_their_code_to_every_native_part_and_merge_their_manifests(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch) / "device-kit"
            output = io.StringIO()
            with mock.patch.object(haylen, "terminal", haylen.Terminal(output, io.StringIO(), {})):
                haylen.command_plugin_new(argparse.Namespace(folder=str(folder), id=None, features=["camera,location", "banner", "microphone"]))
            self.assertIn('with "camera", "location", "banner", "microphone"', output.getvalue())
            self.assertFalse([path for path in folder.rglob("*") if "{{" in path.name or (path.is_file() and "{{" in path.read_text())])

            manifest = json.loads((folder / "plugin.json").read_text())
            self.assertEqual(manifest["apple"]["frameworks"], ["AVFoundation.framework", "CoreLocation.framework"])
            self.assertEqual(set(manifest["parameters"]), {"cameraUsage", "locationUsage", "microphoneUsage", "macDevices"})
            self.assertEqual(manifest["permissions"], ["camera", "location", "microphone"])
            self.assertEqual(manifest["parameters"]["macDevices"]["platforms"], ["macos"])
            self.assertEqual(set(manifest["apple"]["entitlements"].values()), {"${macDevices}"})

            # The shared parts come once, before the features that need them, the Android hooks hold the lines of the features, and the features register the permissions they own and ask for them through their context.
            kotlin = (folder / "android" / "src" / "main" / "kotlin" / "com" / "haylen" / "plugins" / "devicekit" / "DeviceKitPlugin.kt").read_text()
            imports = [line for line in kotlin.splitlines() if line.startswith("import ")]
            self.assertEqual(imports, sorted(imports))
            self.assertIn("    override fun onActivityDestroyed(activity: HaylenActivity) {\n        banner.activityDestroyed()\n    }", kotlin)
            self.assertIn("camera = DeviceKitCamera(context)", kotlin)
            camera = (folder / "android" / "src" / "main" / "kotlin" / "com" / "haylen" / "plugins" / "devicekit" / "DeviceKitCamera.kt").read_text()
            self.assertIn("context.registerPermission(CameraPermission())", camera)
            self.assertIn('private class CameraPermission : HaylenPermission("camera", "the camera", Manifest.permission.CAMERA)', camera)
            self.assertIn('context.requirePermission("camera", reply)', camera)
            self.assertIn("context.register(permission())", (folder / "apple" / "DeviceKitLocation.swift").read_text())
            self.assertIn('name: "microphone"', (folder / "web" / "microphone.js").read_text())
            self.assertIn('<uses-permission android:name="android.permission.CAMERA" />', (folder / "android" / "src" / "main" / "AndroidManifest.xml").read_text())

            swift = (folder / "apple" / "DeviceKitPlugin.swift").read_text()
            self.assertIn("    private let camera = DeviceKitCamera()\n    private let location = DeviceKitLocation()", swift)
            self.assertIn("        location.register(context)", swift)
            self.assertIn('import registerCamera from "./camera.js";', (folder / "web" / "device-kit.js").read_text())
            native = (folder / "native" / "DeviceKit.c").read_text()
            self.assertEqual(native.count("static void device_kit_unsupported("), 1)
            self.assertIn('api->registerHandler("device-kit.takePhoto", device_kit_unsupported, NULL, NULL);', native)
            self.assertIn("function plugin.onLocated(listener)", (folder / "source" / "init.lua").read_text())
            self.assertIn("| `cameraUsage` | string |", (folder / "README.md").read_text())

    def test_an_unknown_feature_is_refused_with_the_features_there_are(self):
        with tempfile.TemporaryDirectory() as scratch:
            with self.assertRaises(haylen.BuildError) as raised:
                haylen.command_plugin_new(argparse.Namespace(folder=str(Path(scratch) / "kit"), id=None, features=["radar"]))
            self.assertEqual(str(raised.exception), 'The plugin template has no feature "radar". The features are "banner", "camera", "location", "microphone".')

    def test_the_features_fill_only_the_blocks_of_the_skeleton(self):
        tokens = {match for path in haylen.PLUGIN_TEMPLATE.rglob("*") if path.is_file() for match in re.findall(r"\{\{BLOCK:([A-Z_]+)\}\}", path.read_text())}
        self.assertEqual(tokens, set(haylen.PLUGIN_BLOCKS))
        for folder in haylen.PLUGIN_FEATURES.iterdir():
            feature = json.loads((folder / "feature.json").read_text())
            self.assertLessEqual(set(feature.get("blocks", {})), set(haylen.PLUGIN_BLOCKS), folder.name)
            self.assertLessEqual(set(feature.get("androidHooks", {})), set(haylen.PLUGIN_ANDROID_HOOKS), folder.name)

    def test_the_format_command_leaves_the_files_of_the_template_with_tokens_alone(self):
        sources = haylen.format_sources()
        self.assertNotIn(haylen.PLUGIN_TEMPLATE / "native" / "{{NAME}}.c", sources)
        self.assertIn(haylen.ROOT / "templates" / "cpp" / "src" / "App.cpp", sources)


class PluginPermissionsTest(unittest.TestCase):
    @staticmethod
    def manifest(identifier: str, permissions: object) -> dict:
        return {"id": identifier, "name": identifier.title(), "version": "1.0.0", "description": "A plugin.", "platforms": ["android"], "permissions": permissions}

    def test_a_plugin_declares_its_own_permissions_in_camel_case(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch) / "lens"
            self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, self.manifest("lens", ["camera", "backgroundLocation"])), [])
            message = 'The key "permissions" must list the names of the permissions that the plugin owns in "camelCase", such as "camera".'
            for permissions in ("camera", ["front-camera"], ["Camera"], [3]):
                self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, self.manifest("lens", permissions)), [message], permissions)
            self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, self.manifest("lens", ["camera", "camera", "screens", "localNetwork"])), [
                'The key "permissions" names "camera" more than once.',
                'The key "permissions" names "screens", which is a permission of the engine.',
                'The key "permissions" names "localNetwork", which is a permission of the engine.',
            ])

    def test_one_plugin_of_an_app_owns_each_permission(self):
        with tempfile.TemporaryDirectory() as scratch:
            app = Path(scratch)
            for identifier in ("lens", "scanner"):
                (app / "plugins" / identifier).mkdir(parents=True)
                (app / "plugins" / identifier / "plugin.json").write_text(json.dumps(self.manifest(identifier, ["camera"])))
            plugins, _ = haylen.load_app_plugins(app, [haylen.PluginReference.parse("lens", {})], "android")
            self.assertEqual([plugin.permissions for plugin in plugins], [["camera"]])
            with self.assertRaises(haylen.BuildError) as raised:
                haylen.load_app_plugins(app, [haylen.PluginReference.parse(identifier, {}) for identifier in ("lens", "scanner")], "android")
            self.assertIn('The plugins "lens", "scanner" declare the permission "camera", which one plugin of an app owns.', str(raised.exception))


class AndroidGpuTestsTest(unittest.TestCase):
    def test_the_tests_build_alone_for_the_abi_as_the_library_of_the_test_app(self):
        with mock.patch.object(haylen, "ensure_shaderc", return_value=Path("/tools/haylen-shaderc")), mock.patch.object(haylen, "android_ndk", return_value=Path("/sdk/ndk")):
            command = [str(part) for part in haylen.android_gpu_tests_configure_command(Path("/build/android"), "x86_64", "Debug")]
        self.assertIn("-DANDROID_ABI=x86_64", command)
        self.assertIn("-DHAYLEN_BUILD_TESTS=ON", command)
        self.assertIn("-DHAYLEN_BUILD_PLAYER=OFF", command)
        self.assertFalse(any("GPU_TESTS_DIR" in part for part in command))

    def test_a_run_that_wrote_no_results_copies_nothing(self):
        # The command exec-out of adb ends with status 0 even when the command on the device fails, and hands its error text as the output.
        def device(command, **_):
            return subprocess.CompletedProcess(command, 1 if "test" in command else 0, b"" if "test" in command else b"tar: gpu-test-output: No such file or directory", b"")

        with mock.patch.object(haylen, "adb", return_value=Path("/sdk/adb")), mock.patch.object(haylen.subprocess, "run", side_effect=device), mock.patch.object(haylen.tarfile, "open") as opening:
            haylen.android_gpu_tests_copy_results("SERIAL", Path("/build/android/gpu-test-output"))
        opening.assert_not_called()

    def test_the_app_starts_with_the_variables_of_the_tests_alone_and_escapes_their_commas(self):
        with mock.patch.object(haylen, "adb", return_value=Path("/sdk/adb")):
            command = [str(part) for part in haylen.android_gpu_tests_launch_command("SERIAL", "vulkan", {"GTEST_FILTER": "GpuBlendTest.*:Gpu*Test.Draws*", "HAYLEN_PROBE": "a,b", "HOME": "/home/me"})]
        self.assertEqual(command[:10], ["/sdk/adb", "-s", "SERIAL", "shell", "am", "start", "-S", "-W", "-n", f"{haylen.ANDROID_GPU_TEST_APP}/android.app.NativeActivity"])
        self.assertEqual(command[10:12], ["--esa", "environment"])
        self.assertEqual(shlex.split(command[12]), ["GTEST_FILTER=GpuBlendTest.*:Gpu*Test.Draws*,HAYLEN_PROBE=a\\,b,HAYLEN_TEST_GRAPHICS_API=vulkan"])

    def test_the_app_starts_without_an_extra_when_no_variable_reaches_it(self):
        with mock.patch.object(haylen, "adb", return_value=Path("/sdk/adb")):
            command = haylen.android_gpu_tests_launch_command("SERIAL", None, {"HOME": "/home/me"})
        self.assertNotIn("--esa", command)

    def test_the_app_compiles_against_the_newest_platform_of_the_sdk(self):
        with tempfile.TemporaryDirectory() as scratch:
            for name in ("android-34", "android-37.0", "android-37.2", "android-Baklava"):
                (Path(scratch) / "platforms" / name).mkdir(parents=True)
                (Path(scratch) / "platforms" / name / "android.jar").write_bytes(b"")
            with mock.patch.object(haylen, "android_sdk", return_value=Path(scratch)):
                jar, api = haylen.android_platform_jar()
        self.assertEqual((jar.parent.name, api), ("android-37.2", 37))

    def test_a_virtual_device_builds_for_the_abi_of_its_configuration(self):
        with tempfile.TemporaryDirectory() as scratch:
            (Path(scratch) / "Phone.avd").mkdir()
            (Path(scratch) / "Phone.avd" / "config.ini").write_text("hw.gpu.enabled=yes\nabi.type=arm64-v8a\n", encoding="utf-8")
            with mock.patch.dict(os.environ, {"ANDROID_AVD_HOME": scratch}):
                self.assertEqual(haylen.android_virtual_device_abi("Phone"), "arm64-v8a")


class AppleGpuTestsTest(unittest.TestCase):
    def configure(self, platform_name: str) -> list[str]:
        with mock.patch.object(haylen, "ensure_shaderc", return_value=Path("/tools/haylen-shaderc")), mock.patch.object(haylen, "host_arch", return_value="arm64"):
            return [str(part) for part in haylen.apple_gpu_tests_configure_command(Path("/build/apple"), platform_name, "Debug")]

    def test_the_tests_build_alone_for_the_sdk_of_each_simulator_and_for_mac_catalyst(self):
        for platform_name, system, sdk in (("ios-simulator", "iOS", "iphonesimulator"), ("tvos-simulator", "tvOS", "appletvsimulator"), ("visionos-simulator", "visionOS", "xrsimulator")):
            command = self.configure(platform_name)
            self.assertIn(f"-DCMAKE_SYSTEM_NAME={system}", command)
            self.assertIn(f"-DCMAKE_OSX_SYSROOT={sdk}", command)
            self.assertIn("-DCMAKE_OSX_ARCHITECTURES=arm64", command)
            self.assertIn("-DHAYLEN_BUILD_TESTS=ON", command)
            self.assertIn("-DHAYLEN_BUILD_PLAYER=OFF", command)
        self.assertIn(f"-DCMAKE_TOOLCHAIN_FILE={haylen.ENGINE_DIR / 'cmake' / 'haylen-catalyst.toolchain.cmake'}", self.configure("catalyst"))

    def test_a_simulator_spawns_the_tests_and_hands_them_the_variables_of_the_tests_alone(self):
        command, env = haylen.apple_gpu_tests_command(Path("/build/haylen-gpu-tests"), "SIMULATOR-ID", "metal", {"GTEST_FILTER": "GpuBlendTest.*", "HAYLEN_TEST_GPU_LOW_POWER": "1", "HOME": "/Users/me"})
        self.assertEqual([str(part) for part in command], ["xcrun", "simctl", "spawn", "SIMULATOR-ID", "/build/haylen-gpu-tests"])
        self.assertEqual(env["SIMCTL_CHILD_HAYLEN_TEST_GRAPHICS_API"], "metal")
        self.assertEqual(env["SIMCTL_CHILD_GTEST_FILTER"], "GpuBlendTest.*")
        self.assertEqual(env["SIMCTL_CHILD_HAYLEN_TEST_GPU_LOW_POWER"], "1")
        self.assertNotIn("SIMCTL_CHILD_HOME", env)

    def test_mac_catalyst_runs_the_tests_on_this_mac(self):
        command, env = haylen.apple_gpu_tests_command(Path("/build/haylen-gpu-tests"), None, None, {"HOME": "/Users/me"})
        self.assertEqual(command, [Path("/build/haylen-gpu-tests")])
        self.assertNotIn("HAYLEN_TEST_GRAPHICS_API", env)

    def test_the_simulators_and_mac_catalyst_take_only_the_gpu_tests(self):
        for platform_name in haylen.APPLE_GPU_TEST_SLICES:
            arguments = haylen.build_parser().parse_args(["test", "--platform", platform_name])
            with self.assertRaisesRegex(haylen.BuildError, "only the GPU tests"):
                haylen.command_test(arguments)

    def test_a_device_builds_the_tests_as_an_app_that_xcode_signs_for_the_team(self):
        for platform_name, system, sdk in (("ios", "iOS", "iphoneos"), ("tvos", "tvOS", "appletvos"), ("visionos", "visionOS", "xros")):
            with mock.patch.object(haylen, "ensure_shaderc", return_value=Path("/tools/haylen-shaderc")), mock.patch.dict(os.environ, {"HAYLEN_APPLE_TEAM": "TEAM123456"}):
                command = [str(part) for part in haylen.apple_device_gpu_tests_configure_command(Path("/build/apple"), platform_name, "Debug")]
            self.assertEqual(command[command.index("-G") + 1], "Xcode")
            self.assertIn(f"-DCMAKE_SYSTEM_NAME={system}", command)
            self.assertIn(f"-DCMAKE_OSX_SYSROOT={sdk}", command)
            self.assertIn("-DCMAKE_OSX_ARCHITECTURES=arm64", command)
            self.assertIn("-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=TEAM123456", command)
            self.assertIn("-DHAYLEN_BUILD_TESTS=ON", command)

    def test_a_device_launches_the_app_and_hands_it_the_variables_of_the_tests_alone(self):
        command, env = haylen.apple_device_gpu_tests_command("iPhone", "metal", {"GTEST_FILTER": "GpuBlendTest.*", "HOME": "/Users/me"})
        self.assertEqual(command, ["xcrun", "devicectl", "device", "process", "launch", "--console", "--terminate-existing", "--device", "iPhone", haylen.APPLE_GPU_TEST_APP])
        self.assertEqual(env["DEVICECTL_CHILD_HAYLEN_TEST_GRAPHICS_API"], "metal")
        self.assertEqual(env["DEVICECTL_CHILD_GTEST_FILTER"], "GpuBlendTest.*")
        self.assertNotIn("DEVICECTL_CHILD_HOME", env)

    def test_the_results_of_a_device_say_whether_the_tests_failed_or_never_finished(self):
        with tempfile.TemporaryDirectory() as scratch:
            results = Path(scratch) / "results.xml"
            self.assertIn("quit before it wrote its results", haylen.gpu_test_app_problem(results))
            results.write_text('<testsuites tests="40" failures="2" errors="1"></testsuites>', encoding="utf-8")
            self.assertEqual(haylen.gpu_test_app_problem(results), "3 of the 40 GPU tests failed on the device.")
            results.write_text('<testsuites tests="40" failures="0" errors="0"></testsuites>', encoding="utf-8")
            self.assertIsNone(haylen.gpu_test_app_problem(results))

    def test_the_devices_take_only_the_gpu_tests_and_a_named_device(self):
        for platform_name in ("ios", "tvos", "visionos"):
            with self.assertRaisesRegex(haylen.BuildError, "only the GPU tests"):
                haylen.command_test(haylen.build_parser().parse_args(["test", "--platform", platform_name]))
            with mock.patch.object(haylen, "require_host"), self.assertRaisesRegex(haylen.BuildError, "Name the device"):
                haylen.command_test(haylen.build_parser().parse_args(["test", "--gpu", "--platform", platform_name]))

    def test_a_desktop_that_is_not_this_machine_runs_no_tests_of_this_machine(self):
        other = next(name for name in ("linux", "windows") if name != haylen.host_name())
        for extra in ([], ["--gpu"]):
            with mock.patch.object(haylen, "command_build") as build, mock.patch.object(haylen, "run") as run, self.assertRaisesRegex(haylen.BuildError, f'Builds for "{other}" need a "{other}" host'):
                haylen.command_test(haylen.build_parser().parse_args(["test", "--platform", other, *extra]))
            build.assert_not_called()
            run.assert_not_called()


class UiTestsTest(unittest.TestCase):
    def test_the_runner_runs_the_scheme_of_the_platform_on_the_simulator_and_names_the_app_to_it(self):
        project = (Path("/build/HaylenUITests.xcodeproj"), "visionos-simulator", "SIMULATOR-ID", Path("/build/derived"))
        common = ["-project", "/build/HaylenUITests.xcodeproj", "-scheme", "visionOS", "-destination", "id=SIMULATOR-ID", "-derivedDataPath", "/build/derived"]
        self.assertEqual([str(part) for part in haylen.ui_test_command("build-for-testing", *project)], ["xcodebuild", "build-for-testing", *common])
        self.assertEqual([str(part) for part in haylen.ui_test_command("test-without-building", *project)], ["xcodebuild", "test-without-building", *common, "-test-timeouts-enabled", "YES", "-maximum-test-execution-time-allowance", "600", "-collect-test-diagnostics", "never"])
        env = haylen.ui_test_environment({"HOME": "/Users/me"}, "com.haylen.tests", Path("/build/screenshots"))
        self.assertEqual(env["TEST_RUNNER_HAYLEN_UI_APP"], "com.haylen.tests")
        self.assertEqual(env["TEST_RUNNER_HAYLEN_UI_OUTPUT"], "/build/screenshots")
        self.assertEqual(env["HOME"], "/Users/me")

    def test_the_runner_builds_for_ios_tvos_and_visionos_with_the_tests_of_the_app(self):
        spec = (haylen.UI_TEST_PROJECT / "project.yml").read_text(encoding="utf-8")
        for platform_name in haylen.UI_TEST_PLATFORMS:
            self.assertIn(f"  {haylen.APPLE_RUNS[platform_name]['scheme']}:\n    type: bundle.ui-testing", spec)
        self.assertIn("path: ${HAYLEN_UI_TESTS}", spec)
        self.assertIn(f"PRODUCT_BUNDLE_IDENTIFIER: {haylen.UI_TEST_RUNNER.removesuffix('.xctrunner')}", spec)

    def test_the_ui_tests_take_a_simulator_of_ios_tvos_or_visionos_and_an_app_with_tests(self):
        with mock.patch.object(haylen, "require_host"):
            with self.assertRaisesRegex(haylen.BuildError, "neither"):
                haylen.command_test(haylen.build_parser().parse_args(["test", "--ui", "samples/tests", "--gpu", "--platform", "ios-simulator"]))
            with self.assertRaisesRegex(haylen.BuildError, '"ios-simulator", "tvos-simulator", "visionos-simulator"'):
                haylen.command_test(haylen.build_parser().parse_args(["test", "--ui", "samples/tests", "--platform", "catalyst"]))
            with tempfile.TemporaryDirectory() as scratch:
                (Path(scratch) / "app.json").write_text("{}", encoding="utf-8")
                with self.assertRaisesRegex(haylen.BuildError, "has no UI tests"):
                    haylen.command_test(haylen.build_parser().parse_args(["test", "--ui", scratch, "--platform", "ios-simulator"]))

    def test_the_ui_tests_build_the_app_with_every_option_of_run(self):
        built: list[argparse.Namespace] = []
        with tempfile.TemporaryDirectory() as scratch, mock.patch.object(haylen, "require_host"), mock.patch.object(haylen, "command_run", side_effect=lambda args: built.append(args)), mock.patch.object(haylen, "App", side_effect=StopIteration):
            (Path(scratch) / "app.json").write_text("{}", encoding="utf-8")
            (Path(scratch) / "ui-tests").mkdir()
            (Path(scratch) / "ui-tests" / "AppTests.swift").write_text("", encoding="utf-8")
            with self.assertRaises(StopIteration):
                haylen.command_test(haylen.build_parser().parse_args(["test", "--ui", scratch, "--platform", "ios-simulator", "--device", "SIMULATOR-ID", "--jobs", "4"]))
            expected = haylen.build_parser().parse_args(["run", str(Path(scratch).resolve()), "--platform", "ios-simulator", "--device", "SIMULATOR-ID", "--engine-config", "Debug", "--build-only", "--jobs", "4"])
        self.assertEqual(vars(built[0]), vars(expected))

    def test_the_unified_log_keeps_the_errors_it_relays(self):
        events = [json.dumps({"messageType": "Default", "eventMessage": "Started."}), json.dumps({"messageType": "Error", "eventMessage": "The view parted."}), "Filtering the log data."]
        errors: list[str] = []
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as stderr:
            haylen.relay_unified_log(mock.Mock(stdout=iter(line + "\n" for line in events)), errors)
        self.assertEqual(errors, ["The view parted."])
        self.assertEqual(stderr.getvalue(), "The view parted.\n")


class VisionOSTest(unittest.TestCase):
    def test_visionos_builds_its_own_slices_and_runs_as_an_apple_platform(self):
        device = [str(part) for part in haylen.apple_slice_options("xros", "arm64")]
        self.assertIn("-DCMAKE_SYSTEM_NAME=visionOS", device)
        self.assertIn("-DCMAKE_OSX_SYSROOT=xros", device)
        self.assertIn(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={haylen.APPLE_MINIMUM_VERSIONS['visionOS']}", device)
        self.assertIn("-DCMAKE_OSX_SYSROOT=xrsimulator", [str(part) for part in haylen.apple_slice_options("xros-simulator", "arm64")])
        self.assertEqual(haylen.APPLE_NATIVE_SLICES["visionos"], "xros")
        self.assertEqual(haylen.APPLE_NATIVE_SLICES["visionos-simulator"], "xros-simulator")
        self.assertEqual(haylen.APPLE_SLICE_NATIVE_PLATFORMS["xros"], "ios")
        self.assertEqual(haylen.RUN_TARGETS["visionos-simulator"].plugins, "visionos")
        self.assertEqual(haylen.APPLE_RUNS["visionos"]["template"], "HaylenVisionOS")
        self.assertIn("visionos", haylen.CPP_RUN_PLATFORMS, "The bundles of haylen_add_app have a visionOS variant.")
        self.assertIn("visionos-simulator", haylen.CPP_RUN_PLATFORMS)

    def test_the_visionos_target_embeds_the_native_libraries_of_ios(self):
        with tempfile.TemporaryDirectory() as scratch:
            generated = Path(scratch) / "haylen"
            library = haylen.NativeLibrary("native-test", {}, Path(scratch), ("ios",), False, ())
            app = mock.Mock(plugins=[], native=[library])
            frameworks = {"iOS": [], "macCatalyst": [], "tvOS": [], "visionOS": [], "macOS": []}
            with mock.patch.object(haylen, "apple_spec", return_value={"targets": {}}), mock.patch.object(haylen, "apple_frameworks", return_value=frameworks):
                haylen.write_apple_spec(app, Path(scratch), generated, {template: {} for template in haylen.APPLE_TEMPLATES}, False)
            text = (generated / "project.yml").read_text()
            templates = json.loads(text[text.index("{"):])["targetTemplates"]
        embeds = {template: [script["inputFileLists"] for script in entry.get("postBuildScripts", [])] for template, entry in templates.items()}
        self.assertEqual(embeds["HaylenVisionOS"], [["$(PROJECT_DIR)/haylen/native/visionos-$(PLATFORM_NAME).xcfilelist"]])
        self.assertEqual(embeds["HaylenIOS"], [["$(PROJECT_DIR)/haylen/native/ios-$(PLATFORM_NAME).xcfilelist"]])
        self.assertEqual(embeds["HaylenTVOS"], [])

    def test_the_frameworks_that_a_plugin_ships_join_the_targets_of_its_platforms(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch) / "vendor"
            (folder / "apple" / "Vendor.xcframework").mkdir(parents=True)
            (folder / "apple" / "Vendor.xcframework" / "Info.plist").write_text("<plist />")
            manifest = {"id": "vendor", "name": "Vendor", "version": "1.0.0", "description": "An SDK.", "platforms": ["ios", "tvos"], "apple": {"binaryFrameworks": [{"path": "apple/Vendor.xcframework", "embed": True}]}}
            self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, manifest), [])
            app = mock.Mock(plugins=[haylen.Plugin(folder, manifest, None)], plugin_values={"vendor": {}}, native=[])
            generated = Path(scratch) / "haylen"
            frameworks = {"iOS": [], "macCatalyst": [], "tvOS": [], "visionOS": [], "macOS": []}
            with mock.patch.object(haylen, "apple_spec", return_value={"targets": {}}), mock.patch.object(haylen, "apple_frameworks", return_value=frameworks):
                haylen.write_apple_spec(app, Path(scratch), generated, {template: {} for template in haylen.APPLE_TEMPLATES}, False)
            text = (generated / "project.yml").read_text()
            templates = json.loads(text[text.index("{"):])["targetTemplates"]
            self.assertTrue((generated / "plugins" / "vendor" / "frameworks" / "Vendor.xcframework" / "Info.plist").is_file())

            shipped = {"framework": "plugins/vendor/frameworks/Vendor.xcframework", "embed": True}
            self.assertIn({**shipped, "destinationFilters": ["iOS"]}, templates["HaylenIOS"]["dependencies"], "The plugin lists iOS without Mac Catalyst.")
            self.assertIn(shipped, templates["HaylenTVOS"]["dependencies"])
            self.assertNotIn(shipped, templates["HaylenMacOS"]["dependencies"])
            manifest["apple"]["binaryFrameworks"] = [{"path": "apple", "embed": "yes"}]
            self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, manifest), ['The key "apple.binaryFrameworks[0].path" must be an ".xcframework" or a ".framework" inside the plugin.', 'The key "apple.binaryFrameworks[0].embed" must be true for a dynamic framework, which the app embeds, and false for a static one.'])

    def test_an_extension_builds_only_for_the_platforms_it_lists(self):
        with tempfile.TemporaryDirectory() as scratch:
            folder = Path(scratch) / "shelf"
            (folder / "extension").mkdir(parents=True)
            extension = {"name": "Shelf", "type": "appExtension", "point": "com.apple.tv-top-shelf", "sources": "extension", "platforms": ["tvos"]}
            manifest = {"id": "shelf", "name": "Shelf", "version": "1.0.0", "description": "A top shelf.", "platforms": ["ios", "tvos"], "apple": {"extensions": [extension]}}
            self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, manifest), [])
            app = mock.Mock(plugins=[haylen.Plugin(folder, manifest, None)], plugin_values={"shelf": {}}, native=[], identifier="com.haylen.shelf", version="1.0.0")
            generated = Path(scratch) / "haylen"
            frameworks = {"iOS": [], "macCatalyst": [], "tvOS": [], "visionOS": [], "macOS": []}
            with mock.patch.object(haylen, "apple_spec", return_value={"targets": {}}), mock.patch.object(haylen, "apple_frameworks", return_value=frameworks):
                haylen.write_apple_spec(app, Path(scratch), generated, {template: {} for template in haylen.APPLE_TEMPLATES}, False)
            text = (generated / "project.yml").read_text()
            document = json.loads(text[text.index("{"):])
        self.assertEqual(list(document["targets"]), ["ShelfTVOS"])
        self.assertNotIn("ShelfIOS", json.dumps(document["targetTemplates"]["HaylenIOS"]))

    def test_a_missing_runtime_names_the_platform_that_xcodebuild_downloads(self):
        with mock.patch.object(haylen, "capture", return_value=json.dumps({"devices": {}})), self.assertRaisesRegex(haylen.BuildError, '"xcodebuild -downloadPlatform visionOS"'):
            haylen.apple_simulator("xrOS", None)


class CleanTest(unittest.TestCase):
    def test_clean_removes_the_build_folder_and_says_when_it_was_already_clean(self):
        with tempfile.TemporaryDirectory() as scratch:
            build = Path(scratch) / "build"
            (build / "macos-debug").mkdir(parents=True)
            for message in ("Cleaned the build folder.", "The build folder is already clean."):
                output = io.StringIO()
                with mock.patch.object(haylen, "BUILD_ROOT", build), mock.patch.object(haylen, "terminal", haylen.Terminal(output, io.StringIO(), {})):
                    haylen.command_clean(argparse.Namespace())
                self.assertFalse(build.exists())
                self.assertEqual(output.getvalue().strip(), message)

    @unittest.skipIf(os.name == "nt" or os.geteuid() == 0, "Only a POSIX user who is not root keeps the files of a read-only folder.")
    def test_a_build_folder_that_stays_is_an_error_and_not_a_clean(self):
        scratch = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, scratch)
        locked = Path(scratch) / "build" / "apps"
        locked.mkdir(parents=True)
        (locked / "app.log").write_text("")
        # A folder whose files cannot go, as a running app or an editor keeps them on Windows, holds the build folder in place.
        locked.chmod(0o500)
        self.addCleanup(locked.chmod, 0o700)
        output = io.StringIO()
        with mock.patch.object(haylen, "BUILD_ROOT", Path(scratch) / "build"), mock.patch.object(haylen, "terminal", haylen.Terminal(output, io.StringIO(), {})):
            with self.assertRaisesRegex(haylen.BuildError, "could not be removed"):
                haylen.command_clean(argparse.Namespace())
        self.assertEqual(output.getvalue(), "")


class CoverageTest(unittest.TestCase):
    def test_the_report_reads_the_tests_by_the_name_of_their_executable_on_this_machine(self):
        for system, executable in (("windows", "haylen-tests.exe"), ("linux", "haylen-tests")):
            commands = []

            # The run of the tests writes a profile, as the instrumented tests do.
            def record(command: list, **_) -> None:
                commands.append([str(part) for part in command])
                if command[0] == "ctest":
                    (Path(command[2]) / "coverage").mkdir(parents=True, exist_ok=True)
                    (Path(command[2]) / "coverage" / "haylen-1.profraw").write_bytes(b"")

            with tempfile.TemporaryDirectory() as scratch, mock.patch.object(haylen, "BUILD_ROOT", Path(scratch)), mock.patch.object(haylen, "host_name", return_value=system), mock.patch.object(haylen, "configure_command", return_value=(["cmake", "-B", "tree"], {})), mock.patch.object(haylen, "llvm_tool", side_effect=lambda name: name), mock.patch.object(haylen, "run", side_effect=record), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
                haylen.command_coverage(haylen.build_parser().parse_args(["coverage", "--jobs", "4"]))
                binary = str(Path(scratch) / "coverage" / "bin" / executable)
            self.assertEqual([command[2] for command in commands if command[0] == "llvm-cov"], [binary, binary])


class PythonVersionTest(unittest.TestCase):
    def test_an_older_python_stops_with_the_version_it_needs(self):
        script = f"import runpy, sys; sys.version_info = (3, 11, 9); runpy.run_path({str(haylen.ROOT / 'haylen.py')!r})"
        result = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Haylen needs Python 3.12 or newer", result.stderr)

    def test_every_f_string_keeps_its_quote_and_backslashes_out_of_its_fields_so_an_older_python_reads_the_file(self):
        # Before Python 3.12, the expression of a field may hold neither the quote of its f-string nor a backslash, even inside a string or an f-string of its own.
        wrong = []
        depth, quote, line = 0, "", 0
        with tokenize.open(haylen.ROOT / "haylen.py") as file:
            for token in tokenize.generate_tokens(file.readline):
                if token.type == tokenize.FSTRING_START and depth == 0:
                    depth, quote, line = 1, token.string.lstrip("rRfFbB"), token.start[0]
                    continue
                if depth == 0:
                    continue
                if token.type == tokenize.FSTRING_START:
                    depth += 1
                elif token.type == tokenize.FSTRING_END:
                    depth -= 1
                    if depth == 0:
                        continue
                elif token.type == tokenize.FSTRING_MIDDLE and depth == 1:
                    continue
                if quote in token.string or "\\" in token.string:
                    wrong.append(line)
        self.assertEqual(sorted(set(wrong)), [], "These lines hold an f-string whose fields use its own quote or a backslash, which only Python 3.12 reads.")


class RunCppTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)

    def test_cpp_apps_launch_on_apple_platforms_with_an_empty_command_line(self):
        (self.scratch / "ios-simulator-debug" / "bin" / "Debug-iphonesimulator" / "game.app").mkdir(parents=True)
        (self.scratch / "ios-simulator-debug" / "bin" / "game").mkdir(parents=True)
        (self.scratch / "ios-simulator-debug" / "bin" / "game" / "package.txt").write_text(str(self.scratch / "package"))
        args = haylen.build_parser().parse_args(["run", "project", "--platform", "ios-simulator"])
        simulator = {"udid": "id", "name": "Phone", "state": "Booted"}
        with mock.patch.object(haylen, "require_host"), mock.patch.object(haylen, "run"), mock.patch.object(haylen, "ensure_shaderc", return_value="shaderc"), mock.patch.object(haylen, "cpp_plugin_options", return_value=[]), mock.patch.object(haylen, "App"), mock.patch.object(haylen, "write_cpp_apple_parts", return_value=False), mock.patch.object(haylen, "apple_simulator", return_value=simulator), mock.patch.object(haylen, "launch_apple", autospec=True) as launch:
            haylen.run_cpp_apple(self.scratch / "project", "game", self.scratch, args)
        self.assertEqual(launch.call_args.args[3], simulator)
        self.assertEqual(launch.call_args.args[4].arguments, [])

    def test_the_apple_parts_of_plugins_reach_the_cmake_project_of_cpp_apps(self):
        plugin = self.scratch / "plugins" / "camera"
        (plugin / "apple").mkdir(parents=True)
        (plugin / "apple" / "CameraPlugin.swift").write_text("")
        (plugin / "apple" / "notes.txt").write_text("")
        (plugin / "camera.bundle").mkdir()
        (plugin / "Vendor.framework").mkdir()
        (plugin / "downloads").mkdir()
        (plugin / "downloads" / "Downloads.swift").write_text("")
        manifest = {
            "id": "camera", "version": "1.0.0", "platforms": ["ios"], "parameters": {},
            "apple": {
                "class": "CameraPlugin", "sources": "apple", "frameworks": ["AVFoundation.framework"], "resources": ["camera.bundle"],
                "infoPlist": {"NSCameraUsageDescription": "Shows the camera.", "CameraGroup": "group.$(HAYLEN_BUNDLE_IDENTIFIER)"},
                "entitlements": {"com.apple.security.application-groups": ["group.$(HAYLEN_BUNDLE_IDENTIFIER)"]},
                "privacy": {"NSPrivacyTracking": True},
                "binaryFrameworks": [{"path": "Vendor.framework", "embed": True}],
                "packages": {"Lens": {"url": "https://example.com/lens-kit.git", "exactVersion": "2.1.0", "products": ["Lens"]}},
                "buildScripts": [{"name": "Stamp", "script": "echo stamped"}],
                "extensions": [{"name": "Downloads", "type": "extensionKit", "point": "com.apple.background-asset-downloader-extension", "sources": "downloads", "minimumVersion": "26.0", "entitlements": {"com.apple.developer.team-identifier": "TEAM"}}, {"name": "Shelf", "type": "appExtension", "point": "com.apple.tv-top-shelf", "sources": "downloads", "platforms": ["tvos"]}],
            },
        }
        (self.scratch / "libcodec.a").write_bytes(b"")
        (self.scratch / "Effects.framework").mkdir()
        codec = haylen.NativeLibrary("codec", {"ios": self.scratch / "libcodec.a"}, None, ("ios",), True, ("codec_open",))
        effects = haylen.NativeLibrary("effects", {"ios": self.scratch / "Effects.framework"}, None, ("ios",), False, ())
        app = mock.Mock(plugins=[haylen.Plugin(plugin, manifest, None)], plugin_values={"camera": {}}, native=[codec, effects], identifier="com.haylen.camera", version="1.0.0")
        app.name = "Camera"
        build = self.scratch / "build"
        fragment = build / haylen.CPP_PLUGINS_FILE
        build.mkdir()
        products = build / "haylen-packages" / "derived" / "Build" / "Products"
        packages = products / "Debug-iphonesimulator"
        frameworks = packages / "PackageFrameworks"
        built = haylen.SwiftPackages(frameworks / "HaylenPackages.framework", [frameworks / "LensCore.framework", frameworks / "LensVideo.framework"], [packages / "Lens_Lens.bundle"], packages, [build / "maps" / "LensObjC.modulemap"], [build / "lens" / "include"], products)
        with mock.patch.object(haylen, "build_swift_packages", return_value=built) as swift, mock.patch.object(haylen, "apple_binary_is_dynamic", side_effect=lambda binary: binary.name == "LensVideo"):
            self.assertTrue(haylen.write_cpp_apple_parts(app, "ios-simulator", build, fragment, "Debug", 4))
        self.assertEqual(swift.call_args.args[:3], ({"Lens": {"url": "https://example.com/lens-kit.git", "exactVersion": "2.1.0", "products": ["Lens"]}}, "ios-simulator", "Debug"))
        text = fragment.read_text()
        copied = build / "haylen-plugins" / "camera"
        self.assertIn(f"set(HAYLEN_PLUGIN_SOURCES [==[{(copied / 'sources' / 'CameraPlugin.swift').as_posix()};{(build / 'haylen-plugins' / 'HaylenNativeSymbols.mm').as_posix()}]==])", text)
        self.assertIn('registerLinked("codec"', (build / "haylen-plugins" / "HaylenNativeSymbols.mm").read_text(), "The static native libraries register their symbols for haylen.native.")
        self.assertIn("set(HAYLEN_PLUGIN_FRAMEWORKS [==[AVFoundation]==])", text)
        self.assertIn(f"set(HAYLEN_PLUGIN_BUNDLES [==[{(copied / 'resources' / 'camera.bundle').as_posix()};{(packages / 'Lens_Lens.bundle').as_posix()}]==])", text)
        self.assertIn(f"set(HAYLEN_PLUGIN_LIBRARIES [==[{(plugin / 'Vendor.framework').as_posix()};{(self.scratch / 'libcodec.a').as_posix()};{(frameworks / 'HaylenPackages.framework').as_posix()};{(frameworks / 'LensCore.framework').as_posix()};{(frameworks / 'LensVideo.framework').as_posix()}]==])", text, "The app links the binary frameworks of the packages after their framework, which exports none of their symbols.")
        self.assertIn(f"set(HAYLEN_PLUGIN_EMBEDDED [==[{(plugin / 'Vendor.framework').as_posix()};{(self.scratch / 'Effects.framework').as_posix()};{(frameworks / 'HaylenPackages.framework').as_posix()};{(frameworks / 'LensVideo.framework').as_posix()}]==])", text, "The app embeds only the dynamic binary frameworks of the packages.")
        self.assertIn(f"set(HAYLEN_PLUGIN_MODULES [==[{packages.as_posix()}]==])", text)
        self.assertIn(f"set(HAYLEN_PLUGIN_MODULE_MAPS [==[{(build / 'maps' / 'LensObjC.modulemap').as_posix()}]==])", text, "The Swift sources import the C-family modules of the packages through their module maps.")
        self.assertIn(f"set(HAYLEN_PLUGIN_HEADERS [==[{(build / 'lens' / 'include').as_posix()}]==])", text)
        self.assertIn(f"set(HAYLEN_PLUGIN_PACKAGES_BUILD_DIR [==[{products.as_posix()}]==])", text, "The build scripts reach the checkouts of the packages from the products folder of their derived data.")
        self.assertIn(f"set(HAYLEN_PLUGIN_SCRIPTS [==[{(copied / 'scripts' / '0.sh').as_posix()}]==])", text)
        self.assertIn("echo stamped", (copied / "scripts" / "0.sh").read_text())
        self.assertIn("<string>group.com.haylen.camera</string>", text, "The Info.plist keys name the identity of the app as Xcode would.")
        self.assertEqual(haylen.read_plist(build / "haylen-plugins" / "App.entitlements"), {"com.apple.security.application-groups": ["group.com.haylen.camera"]})
        self.assertTrue(haylen.read_plist(build / "haylen-plugins" / "PrivacyInfo.xcprivacy")["NSPrivacyTracking"])
        self.assertIn("NSPrivacyAccessedAPITypes", haylen.read_plist(build / "haylen-plugins" / "PrivacyInfo.xcprivacy"), "The privacy manifest of the engine joins the declarations of the plugins.")
        self.assertIn("set(HAYLEN_PLUGIN_EXTENSIONS [==[Downloads]==])", text)
        self.assertIn("set(HAYLEN_EXTENSION_Downloads_TYPE extensionkit-extension)", text)
        self.assertIn("set(HAYLEN_EXTENSION_Downloads_IDENTIFIER com.haylen.camera.downloads)", text)
        self.assertIn("set(HAYLEN_EXTENSION_Downloads_MINIMUM_VERSION [==[26.0]==])", text)
        info = haylen.read_plist(copied / "extensions" / "Downloads" / "Info.plist")
        self.assertEqual(info["EXAppExtensionAttributes"], {"EXExtensionPointIdentifier": "com.apple.background-asset-downloader-extension"})
        self.assertEqual(info["CFBundleIdentifier"], "com.haylen.camera.downloads")
        with mock.patch.object(haylen, "build_swift_packages", return_value=built), mock.patch.object(haylen, "apple_binary_is_dynamic", side_effect=lambda binary: binary.name == "LensVideo"):
            self.assertFalse(haylen.write_cpp_apple_parts(app, "ios-simulator", build, fragment, "Debug", 4))

    def test_the_swift_packages_of_cpp_apps_join_one_dynamic_framework(self):
        haylen.write_swift_packages({"Lens": {"url": "https://example.com/team/lens-kit.git", "exactVersion": "2.1.0", "products": ["Lens", "LensUI"]}, "Maps": {"url": "git@example.com:team/Maps.git", "exactVersion": "1.0.0", "products": ["Maps"]}}, self.scratch)
        manifest = (self.scratch / "Package.swift").read_text()
        self.assertIn('.library(name: "HaylenPackages", type: .dynamic, targets: ["HaylenPackages"])', manifest)
        self.assertIn('.package(url: "https://example.com/team/lens-kit.git", exact: "2.1.0")', manifest)
        self.assertIn('.product(name: "LensUI", package: "lens-kit")', manifest)
        self.assertIn('.product(name: "Maps", package: "maps")', manifest)
        self.assertTrue((self.scratch / "Sources" / "HaylenPackages" / "HaylenPackages.swift").is_file())

    def test_swift_sources_of_cpp_apps_find_the_c_family_modules_that_the_packages_compiled(self):
        derived = self.scratch / "derived"
        built = derived / "Build" / "Products" / "Debug-iphonesimulator"
        generated = derived / "Build" / "Intermediates.noindex" / "GeneratedModuleMaps-iphonesimulator"
        checkouts = derived / "SourcePackages" / "checkouts"
        for name in ("Core.o", "Promises.o", "Utilities.o", "Utilities.swiftmodule", "HaylenPackages.o", "HaylenPackages.swiftmodule"):
            (built / name).mkdir(parents=True)
        for folder in ("kit/Sources/Core/Public/Core", "kit/Sources/Utilities", "kit/Tests/CoreTests", "promises/Sources/Promises/include", "promises/Sources/Unused/include"):
            (checkouts / folder).mkdir(parents=True)
        for package in ("kit", "promises"):
            (checkouts / package / "Package.swift").write_text("")
        (checkouts / "promises" / "Sources" / "Promises" / "include" / "module.modulemap").write_text("module Promises {}")
        generated.mkdir(parents=True)
        for name in ("Core", "Utilities", "HaylenPackages"):
            (generated / f"{name}.modulemap").write_text("")
        manifests = {
            "kit": {"targets": [{"name": "Core", "path": "Sources/Core", "publicHeadersPath": "Public"}, {"name": "Utilities"}, {"name": "CoreTests", "path": "Tests/CoreTests"}]},
            "promises": {"targets": [{"name": "Promises"}, {"name": "Unused"}]},
        }
        with mock.patch.object(haylen, "capture", side_effect=lambda command: json.dumps(manifests[Path(command[command.index("--package-path") + 1]).name])):
            module_maps, headers = haylen.swift_package_clang_modules(derived, built, generated, self.scratch / "manifests")
        self.assertEqual(module_maps, [generated / "Core.modulemap", checkouts / "promises" / "Sources" / "Promises" / "include" / "module.modulemap"], "A target keeps its own module map, and Swift targets and targets that the packages did not compile give none.")
        self.assertEqual(headers, [checkouts / "kit" / "Sources" / "Core" / "Public", checkouts / "promises" / "Sources" / "Promises" / "include"])

    def test_the_binary_frameworks_of_packages_are_static_or_dynamic_by_their_header(self):
        archive = self.scratch / "Archive"
        archive.write_bytes(b"!<arch>\n" + bytes(64))
        thin = self.scratch / "Thin"
        thin.write_bytes((0xFEEDFACF).to_bytes(4, "little") + bytes(8) + (6).to_bytes(4, "little") + bytes(16))
        universal = self.scratch / "Universal"
        universal.write_bytes((0xCAFEBABE).to_bytes(4, "big") + (1).to_bytes(4, "big") + bytes(8) + (64).to_bytes(4, "big") + bytes(44) + b"!<arch>\n" + bytes(8))
        self.assertFalse(haylen.apple_binary_is_dynamic(archive))
        self.assertTrue(haylen.apple_binary_is_dynamic(thin))
        self.assertFalse(haylen.apple_binary_is_dynamic(universal), "A universal file is what its first architecture is.")
        text = self.scratch / "Text"
        text.write_text("Not a library at all.")
        with self.assertRaisesRegex(haylen.BuildError, "is neither a static archive nor a Mach-O library"):
            haylen.apple_binary_is_dynamic(text)

    def test_plugins_that_ship_the_same_framework_stop_the_build_of_a_cpp_app(self):
        manifests = []
        for name in ("first", "second"):
            (self.scratch / name / "Vendor.framework").mkdir(parents=True)
            manifests.append(haylen.Plugin(self.scratch / name, {"id": name, "version": "1.0.0", "platforms": ["ios"], "parameters": {}, "apple": {"binaryFrameworks": [{"path": "Vendor.framework", "embed": False}]}}, None))
        app = mock.Mock(plugins=manifests, plugin_values={"first": {}, "second": {}}, native=[], identifier="com.haylen.vendor", version="1.0.0")
        with self.assertRaisesRegex(haylen.BuildError, 'The plugins "first" and "second" both ship the framework "Vendor.framework".'):
            haylen.write_cpp_apple_parts(app, "ios-simulator", self.scratch / "build", self.scratch / "build" / haylen.CPP_PLUGINS_FILE, "Debug", 4)

    def test_cpp_apps_of_windows_and_linux_find_the_native_libraries_next_to_their_executable(self):
        (self.scratch / "package").mkdir()
        (self.scratch / "package" / "app.json").write_text(json.dumps({"name": "Game", "identifier": "com.haylen.game", "version": "1.0.0"}))
        (self.scratch / "build" / "bin" / "main").mkdir(parents=True)
        (self.scratch / "build" / "bin" / "main" / "package.txt").write_text(str(self.scratch / "package"))
        executable = self.scratch / "build" / "bin" / "main" / "game"
        args = haylen.build_parser().parse_args(["run", str(self.scratch / "package")])
        patches = [mock.patch.object(haylen, "host_name", return_value="linux"), mock.patch.object(haylen, "run"), mock.patch.object(haylen, "ensure_shaderc", return_value="shaderc"), mock.patch.object(haylen, "cmake_app_executable", return_value=executable), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {}))]
        with contextlib.ExitStack() as stack, mock.patch.object(haylen, "prepare_host_native") as native:
            for patch in patches:
                stack.enter_context(patch)
            haylen.build_cpp_desktop(self.scratch / "package", "main", self.scratch / "build", args)
        self.assertEqual(native.call_args.args[0].folder, self.scratch / "package")
        self.assertEqual(native.call_args.args[1], executable.parent)

    def test_plugin_add_keeps_the_links_inside_the_frameworks_of_a_plugin(self):
        plugin = self.scratch / "vendor"
        framework = plugin / "frameworks" / "Vendor.framework"
        (framework / "Versions" / "A").mkdir(parents=True)
        (framework / "Versions" / "A" / "Vendor").write_bytes(b"code")
        (framework / "Versions" / "Current").symlink_to("A")
        (framework / "Vendor").symlink_to("Versions/Current/Vendor")
        (plugin / "plugin.json").write_text(json.dumps({"id": "vendor", "name": "Vendor", "version": "1.0.0", "description": "Ships a framework.", "platforms": ["macos"], "apple": {"binaryFrameworks": [{"path": "frameworks/Vendor.framework", "embed": True}]}}))
        app = self.scratch / "app"
        app.mkdir()
        (app / "app.json").write_text(json.dumps({"name": "Game", "identifier": "com.haylen.game", "version": "1.0.0"}))
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", str(plugin), "--app", str(app)]))
        copied = app / "plugins" / "vendor" / "frameworks" / "Vendor.framework"
        self.assertTrue((copied / "Vendor").is_symlink(), "A macOS framework whose links became copies is no bundle that codesign signs.")
        self.assertEqual((copied / "Versions" / "Current").readlink(), Path("A"))

    def test_cpp_apps_launch_on_android_with_an_empty_command_line(self):
        built = self.scratch / "android-arm64-v8a-debug" / "bin" / "game"
        built.mkdir(parents=True)
        (built / "package.txt").write_text(str(self.scratch / "package"))
        args = haylen.build_parser().parse_args(["run", "project", "--platform", "android"])
        patches = [
            mock.patch.object(haylen, "android_device", return_value="emulator-5554"),
            mock.patch.object(haylen, "capture", return_value="arm64-v8a"),
            mock.patch.object(haylen, "run"),
            mock.patch.object(haylen, "ensure_shaderc", return_value="shaderc"),
            mock.patch.object(haylen, "android_options", return_value=[]),
            mock.patch.object(haylen, "cpp_plugin_options", return_value=[]),
            mock.patch.object(haylen, "App"),
            mock.patch.object(haylen, "ensure_artifacts"),
            mock.patch.object(haylen, "project_root", return_value=self.scratch / "root"),
            mock.patch.object(haylen, "prepare_android"),
            mock.patch.object(haylen, "copy_into"),
            mock.patch.object(haylen, "build_android", return_value=self.scratch / "game.apk"),
            mock.patch.object(haylen, "check_android", return_value=[]),
            mock.patch.object(haylen, "launch_android", autospec=True),
        ]
        with contextlib.ExitStack() as stack:
            mocks = [stack.enter_context(patch) for patch in patches]
            haylen.run_cpp_android(self.scratch / "project", "game", self.scratch, args)
        self.assertEqual(mocks[-1].call_args.args[4], "emulator-5554")
        self.assertEqual(mocks[-1].call_args.args[5].arguments, [])


    def test_a_build_only_run_builds_the_app_of_this_machine_without_launching_it(self):
        (self.scratch / "project").mkdir()
        (self.scratch / "project" / "CMakeLists.txt").write_text("")
        (self.scratch / "project" / "app.json").write_text(json.dumps({"name": "Game", "identifier": "com.haylen.game", "version": "1.0.0"}))
        executable = self.scratch / "app"
        # Windows and Linux read the package of the built app to place its native libraries next to it.
        built = self.scratch / "builds" / f"{haylen.host_name()}-debug" / "bin" / "main"
        built.mkdir(parents=True)
        (built / "package.txt").write_text(str(self.scratch / "project"))
        for options, launched in (([], True), (["--build-only"], False)):
            args = haylen.build_parser().parse_args(["run", str(self.scratch / "project"), *options])
            # On macOS the configuration also gives the app the Apple parts of its plugins, which the test of those parts covers.
            configure = mock.patch.object(haylen, "configure_cpp_apple", side_effect=lambda target, directory, command, platform, given: haylen.run(command))
            with mock.patch.object(haylen, "run") as run, configure, mock.patch.object(haylen, "CPP_BUILDS_DIR", self.scratch), mock.patch.object(haylen, "build_folder_name", return_value="builds"), mock.patch.object(haylen, "prepare_host_native"), mock.patch.object(haylen.RunSession, "follow") as follow, mock.patch.object(haylen, "ensure_shaderc", return_value="shaderc"), mock.patch.object(haylen, "cmake_app_executable", return_value=executable), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
                haylen.run_cpp_app(self.scratch / "project", args)
            self.assertEqual([call.args[0] for call in follow.call_args_list], [[executable]] if launched else [])
            self.assertIn(f"-DHAYLEN_ENGINE_SOURCE={haylen.ENGINE_DIR.as_posix()}", run.call_args_list[0].args[0])
            self.assertIn("--target", run.call_args_list[1].args[0])
            self.assertEqual(run.call_args_list[1].args[0][run.call_args_list[1].args[0].index("--target") + 1], "main")


    def test_run_builds_a_cpp_app_from_its_cmake_project_on_the_platforms_it_supports(self):
        app = self.scratch / "game"
        app.mkdir()
        (app / "app.json").write_text("{}")
        (app / "CMakeLists.txt").write_text("")
        with mock.patch.object(haylen, "run_cpp_app") as built:
            haylen.command_run(haylen.build_parser().parse_args(["run", str(app)]))
        self.assertEqual(built.call_args.args[0], app.resolve())
        # A desktop builds the C++ apps of its own system alone.
        other = "linux" if haylen.host_name() != "linux" else "windows"
        with self.assertRaisesRegex(haylen.BuildError, f'A C\\+\\+ app runs on .* and not on "{other}".'):
            haylen.run_cpp_app(app, haylen.build_parser().parse_args(["run", str(app), "--platform", other]))


class PluginReferenceTest(unittest.TestCase):
    """The plugins of `app.json` come from the app folder, from a folder anywhere on disk or from a git repository at a ref, and every package and build holds them at `plugins/<id>/` the same way."""

    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.scratch)
        self.app = self.scratch / "app"
        (self.app / "source").mkdir(parents=True)
        (self.app / "source" / "main.lua").write_text("print(1)")
        quiet = haylen.Terminal(io.StringIO(), io.StringIO(), {})
        quiet.log = self.scratch / "haylen.log"
        for patch in (mock.patch.object(haylen, "PLUGIN_REPOSITORIES_DIR", self.scratch / "repositories"), mock.patch.object(haylen, "APPS_DIR", self.scratch / "apps"), mock.patch.object(haylen, "terminal", quiet)):
            patch.start()
            self.addCleanup(patch.stop)
        haylen.fetch_plugin_repository.cache_clear()
        self.addCleanup(haylen.fetch_plugin_repository.cache_clear)

    def write_plugin(self, folder, identifier, parameters=None):
        (folder / "source").mkdir(parents=True, exist_ok=True)
        (folder / "plugin.json").write_text(json.dumps({"id": identifier, "name": "Reference", "version": "1.0.0", "description": "Tests references.", "platforms": ["web", "linux"], "parameters": parameters or {}}))
        (folder / "source" / "init.lua").write_text(f"return '{identifier}'")
        (folder / "README.md").write_text("Never in the package.")
        return folder

    def write_app(self, plugins):
        (self.app / "app.json").write_text(json.dumps({"name": "References", "identifier": "com.haylen.references", "version": "1.0.0", "plugins": plugins}))

    def git(self, repository, *arguments):
        identity = ["-c", "user.name=Haylen", "-c", "user.email=tests@haylen.dev", "-c", "commit.gpgsign=false", "-c", "tag.gpgsign=false"]
        return subprocess.run(["git", "-C", str(repository), *identity, *arguments], check=True, capture_output=True, text=True).stdout.strip()

    def make_repository(self, identifier):
        repository = self.write_plugin(self.scratch / f"{identifier}-repository", identifier)
        self.git(repository, "init", "--quiet", "--initial-branch", "main")
        self.git(repository, "add", ".")
        self.git(repository, "commit", "--quiet", "-m", "First")
        self.git(repository, "tag", "1.0.0")
        return repository

    def failure(self, platform="web"):
        with self.assertRaises(haylen.BuildError) as raised:
            haylen.App(self.app, platform)
        return str(raised.exception)

    def test_plugins_come_from_the_app_a_folder_anywhere_and_a_git_repository_at_a_ref(self):
        self.write_plugin(self.app / "plugins" / "local", "local", {"greeting": {"type": "string", "default": "Hi", "description": "Greets."}})
        far = self.write_plugin(self.scratch / "elsewhere" / "far-plugin", "far")
        absolute = self.write_plugin(self.scratch / "absolute", "absolute")
        repository = self.make_repository("remote")
        self.write_app({"local": {"parameters": {"greeting": "Hello"}}, "far": {"path": "../elsewhere/far-plugin"}, "absolute": {"path": str(absolute)}, "remote": {"git": "../remote-repository", "ref": "1.0.0"}})

        app = haylen.App(self.app, "web")
        folders = {plugin.id: plugin.folder for plugin in app.plugins}
        self.assertEqual(folders["local"], self.app / "plugins" / "local")
        self.assertEqual(folders["far"], far)
        self.assertEqual(folders["absolute"], absolute)
        self.assertTrue(folders["remote"].is_relative_to(self.scratch / "repositories"), "A repository is fetched into the cache under build.")
        self.assertEqual(app.plugin_values["local"], {"greeting": "Hello"})
        files = app.layout.files()
        self.assertEqual(list(files), ["app.json", "plugins/absolute/plugin.json", "plugins/absolute/source/init.lua", "plugins/far/plugin.json", "plugins/far/source/init.lua", "plugins/local/plugin.json", "plugins/local/source/init.lua", "plugins/remote/plugin.json", "plugins/remote/source/init.lua", "source/main.lua"])
        self.assertEqual(files["plugins/far/source/init.lua"], far / "source" / "init.lua")
        self.assertEqual(files["plugins/remote/source/init.lua"].read_text(), "return 'remote'")

        # A branch follows the repository each time a command runs, and the checkout of a full commit never fetches again.
        (repository / "source" / "init.lua").write_text("return 'moved'")
        self.git(repository, "commit", "--quiet", "-am", "Moved")
        commit = self.git(repository, "rev-parse", "HEAD")
        self.write_app({"remote": {"git": str(repository), "ref": "main"}})
        haylen.fetch_plugin_repository.cache_clear()
        self.assertEqual(haylen.App(self.app, "web").layout.files()["plugins/remote/source/init.lua"].read_text(), "return 'moved'")
        self.write_app({"remote": {"git": str(repository), "ref": commit}})
        haylen.App(self.app, "web")
        haylen.fetch_plugin_repository.cache_clear()
        with mock.patch.object(haylen, "run", side_effect=AssertionError("The checkout of a commit fetched again.")):
            self.assertEqual(haylen.App(self.app, "web").layout.files()["plugins/remote/source/init.lua"].read_text(), "return 'moved'")

    def make_folders_repository(self):
        """A repository that holds two plugins in folders of its own, and a folder that leads outside it through a symbolic link."""
        repository = self.scratch / "plugins-repository"
        self.write_plugin(repository / "camera", "camera", {"lens": {"type": "string", "default": "wide", "description": "Picks the lens."}})
        self.write_plugin(repository / "share", "share")
        self.write_plugin(self.scratch / "outside", "outside")
        (repository / "outside").symlink_to(self.scratch / "outside")
        self.git(repository, "init", "--quiet", "--initial-branch", "main")
        self.git(repository, "add", ".")
        self.git(repository, "commit", "--quiet", "-m", "First")
        return repository

    def test_plugins_come_from_folders_of_a_git_repository(self):
        repository = self.make_folders_repository()
        self.write_app({"camera": {"git": "../plugins-repository", "ref": "main", "path": "camera"}, "share": {"git": str(repository), "path": "share/"}})

        app = haylen.App(self.app, "web")
        folders = {plugin.id: plugin.folder for plugin in app.plugins}
        checkout = folders["camera"].parent
        self.assertTrue(checkout.is_relative_to(self.scratch / "repositories"), "A repository is fetched into the cache under build.")
        self.assertEqual(folders, {"camera": checkout / "camera", "share": folders["share"].parent / "share"})
        self.assertEqual(app.plugin_values["camera"], {"lens": "wide"})
        files = app.layout.files()
        self.assertEqual(list(files), ["app.json", "plugins/camera/plugin.json", "plugins/camera/source/init.lua", "plugins/share/plugin.json", "plugins/share/source/init.lua", "source/main.lua"])
        self.assertEqual(files["plugins/share/source/init.lua"].read_text(), "return 'share'")

        # The development server pushes the plugins of the folders of a repository under their ids, and C++ apps take the same folders.
        server = haylen.DevelopmentServer(haylen.PackageLayout.of(self.app), {})
        self.assertEqual(sorted(server.layout.plugins), ["camera", "share"])
        build = self.scratch / "build"
        haylen.cpp_plugin_options(self.app, build)
        self.assertIn(f"set(HAYLEN_PLUGIN_FOLDER_camera [[{(checkout / 'camera').as_posix()}]])", (build / haylen.CPP_PLUGIN_FOLDERS_FILE).read_text())

        output = io.StringIO()
        listing = haylen.Terminal(output, io.StringIO(), {})
        listing.log = self.scratch / "haylen.log"
        with mock.patch.object(haylen, "terminal", listing):
            haylen.command_plugin_list(haylen.build_parser().parse_args(["plugin", "list", "--app", str(self.app)]))
        self.assertIn('From the folder "camera" of ../plugins-repository at "main".', output.getvalue())

        # A folder that the repository lacks has no plugin, and one that leads outside the repository through a symbolic link is refused.
        self.write_app({"camera": {"git": str(repository), "path": "lens"}, "outside": {"git": str(repository), "path": "outside"}})
        message = self.failure()
        self.assertIn(f'The key "plugins.camera" names the folder "lens" of `{repository}` at "HEAD", which holds no "plugin.json" at its root.', message)
        self.assertIn('The key "plugins.outside.path" names the folder "outside" of a git repository, which leads outside the repository. Name a folder inside it, relative to its root, such as "camera".', message)

    def test_plugin_add_takes_a_folder_of_a_repository_as_a_copy_or_a_reference(self):
        repository = self.make_folders_repository()
        self.write_app({})
        haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", "file://" + str(repository), "--ref", "main", "--path", "camera", "--app", str(self.app)]))
        haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", "file://" + str(repository), "--path", "share", "--reference", "--app", str(self.app)]))
        listed = json.loads((self.app / "app.json").read_text())["plugins"]
        self.assertEqual(listed["share"], {"git": "file://" + str(repository), "path": "share"})
        self.assertEqual((self.app / "plugins" / "camera" / "source" / "init.lua").read_text(), "return 'camera'")
        self.assertFalse((self.app / "plugins" / "share").exists())

        for path, sentence in (
            ("outside", 'The option "--path" names the folder "outside" of a git repository, which leads outside the repository.'),
            ("../camera", 'The option "--path" names the folder "../camera" of a git repository, which leads outside the repository.'),
            ("lens", f'The folder "lens" of the repository `file://{repository}` is not a plugin, because it holds no "plugin.json".'),
        ):
            with self.subTest(path=path), self.assertRaises(haylen.BuildError) as raised:
                haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", "file://" + str(repository), "--path", path, "--reference", "--app", str(self.app)]))
            self.assertIn(sentence, str(raised.exception))
        with self.assertRaisesRegex(haylen.BuildError, 'holds no "plugin.json" at its root. Name the plugin folder inside it with "--path", such as "--path camera".'):
            haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", "file://" + str(repository), "--app", str(self.app)]))
        with self.assertRaisesRegex(haylen.BuildError, 'The option "--path" names the plugin folder inside a git repository, and a plugin folder is no repository.'):
            haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", str(repository / "camera"), "--path", "camera", "--app", str(self.app)]))

    def test_every_problem_of_the_entries_names_its_key(self):
        with self.assertRaises(haylen.BuildError) as raised:
            haylen.read_plugin_references(self.app, {"Camera": {}, "ads": [], "maps": {"path": "../maps", "git": "https://example.com/maps.git"}, "charts": {"path": "/charts", "git": "https://example.com/charts.git"}, "share": {"ref": "main"}, "cloud": {"greeting": "Hi", "path": 3, "parameters": []}})
        message = str(raised.exception)
        for sentence in (
            'The key "plugins.Camera" must be the id of a plugin in "dash-case", such as "camera-scanner".',
            'The key "plugins.ads" must be an object with the optional keys "path", "git", "ref", "parameters", such as {"path": "../plugins/ads", "parameters": {}}.',
            'The key "plugins.maps.path" names the folder "../maps" of a git repository, which leads outside the repository. Name a folder inside it, relative to its root, such as "camera".',
            'The key "plugins.charts.path" names the folder "/charts" of a git repository, which leads outside the repository.',
            'The key "plugins.share.ref" picks a branch, tag or commit of a git repository, so it needs "git".',
            'The key "plugins.cloud.greeting" is not a key of a plugin entry, which takes "path", "git", "ref", "parameters".',
            'The key "plugins.cloud.path" must be a text.',
            'The key "plugins.cloud.parameters" must be an object of parameter values.',
        ):
            self.assertIn(sentence, message)
        with self.assertRaisesRegex(haylen.BuildError, 'The key "plugins" of .* must map plugin ids to their entries'):
            haylen.read_plugin_references(self.app, ["ads"])

    def test_a_plugin_that_cannot_be_resolved_stops_the_build_with_its_reason(self):
        self.write_plugin(self.scratch / "misnamed", "other")
        self.write_plugin(self.app / "plugins" / "copied", "copied")
        self.write_plugin(self.scratch / "copied", "copied")
        repository = self.make_repository("remote")
        self.write_app({"missing": {}, "absent": {"path": "../absent"}, "misnamed": {"path": "../misnamed"}, "copied": {"path": "../copied"}, "remote": {"git": str(repository), "ref": "2.0.0"}})
        message = self.failure()
        self.assertIn('The key "plugins.missing" names no plugin of the app, because', message)
        self.assertIn('The key "plugins.absent" names `', message)
        self.assertIn('which holds no "plugin.json" at its root.', message)
        self.assertIn('whose "plugin.json" has the id "other". List the plugin under the id of its "plugin.json".', message)
        self.assertIn('The plugin "copied" comes from `', message)
        self.assertIn('Delete that folder, or leave "path" and "git" out of "plugins.copied" to use it.', message)
        self.assertIn(f'The plugin repository `{repository}` cannot be fetched at "2.0.0". Check the address in "git" and the branch, tag or commit in "ref".', message)

        # A path that names the copy in the app folder is that copy, not a second one.
        self.write_app({"copied": {"path": "plugins/copied"}})
        self.assertEqual(haylen.App(self.app, "web").plugins[0].folder, self.app / "plugins" / "copied")

    def test_the_values_of_parameters_come_from_parameters(self):
        self.write_plugin(self.app / "plugins" / "ads", "ads", {"appId": {"type": "string", "required": True, "description": "Names the app."}})
        self.write_app({"ads": {"parameters": {"appId": "", "testMode": True}}})
        message = self.failure()
        self.assertIn('The key "plugins.ads.parameters.testMode" is no parameter of the plugin "ads".', message)
        self.assertIn('The key "plugins.ads.parameters.appId" needs a value to build for "web". Names the app.', message)

    def test_the_package_holds_every_plugin_at_its_id_wherever_it_comes_from(self):
        far = self.write_plugin(self.scratch / "far-plugin", "far")
        self.write_app({"far": {"path": "../far-plugin"}})
        app = haylen.App(self.app, "web")
        archive = self.scratch / "app.zip"
        haylen.package_folder(app.layout, archive)
        with zipfile.ZipFile(archive) as opened:
            self.assertEqual(sorted(opened.namelist()), ["app.json", "plugins/far/plugin.json", "plugins/far/source/init.lua", "source/main.lua"])
        copied = self.scratch / "copy"
        self.assertEqual(haylen.copy_package(app, copied), ["app.json", "plugins/far/plugin.json", "plugins/far/source/init.lua", "source/main.lua"])
        self.assertEqual((copied / "plugins" / "far" / "source" / "init.lua").read_text(), "return 'far'")

        # The staged package follows the app and its plugins, and loses what they lost.
        staged = haylen.stage_package(app)
        self.assertEqual(staged, app.build_folder / "package")
        (far / "source" / "extra.lua").write_text("return 1")
        haylen.stage_package(app)
        self.assertTrue((staged / "plugins" / "far" / "source" / "extra.lua").is_file())
        (far / "source" / "extra.lua").unlink()
        (far / "source" / "init.lua").write_text("return 'edited'")
        os.utime(far / "source" / "init.lua", ns=(0, 5_000_000_000))
        haylen.stage_package(app)
        self.assertFalse((staged / "plugins" / "far" / "source" / "extra.lua").exists())
        self.assertEqual((staged / "plugins" / "far" / "source" / "init.lua").read_text(), "return 'edited'")
        self.assertEqual(sorted(path.relative_to(staged).as_posix() for path in staged.rglob("*") if path.is_file()), ["app.json", "plugins/far/plugin.json", "plugins/far/source/init.lua", "source/main.lua"])

    def test_the_development_server_watches_referenced_folders_and_pushes_their_files_under_their_ids(self):
        far = self.write_plugin(self.scratch / "far-plugin", "far")
        self.write_app({})
        server = haylen.DevelopmentServer(haylen.PackageLayout.of(self.app), {})
        self.write_app({"far": {"path": "../far-plugin"}})
        os.utime(self.app / "app.json", ns=(0, 5_000_000_000))
        changed, removed = server.scan()
        self.assertEqual((changed, removed), (["app.json", "plugins/far/plugin.json", "plugins/far/source/init.lua"], []))

        (far / "source" / "init.lua").write_text("return 'saved'")
        os.utime(far / "source" / "init.lua", ns=(0, 9_000_000_000))
        changed, _ = server.scan()
        self.assertEqual(changed, ["plugins/far/source/init.lua"])
        message, contents = server.batch(changed, [])
        self.assertEqual((message["files"], contents), ([{"path": "plugins/far/source/init.lua", "size": 14}], [b"return 'saved'"]))

        # The debugger shows the files of a plugin where they lie, and the app names them by their package paths.
        relay = haylen.DebugRelay(server.layout, 0)
        self.addCleanup(relay.close)
        self.assertEqual(relay.to_package(str(far / "source" / "init.lua")), "plugins/far/source/init.lua")
        self.assertEqual(relay.to_editor("plugins/far/source/init.lua"), str(far / "source" / "init.lua"))
        self.assertEqual(relay.to_editor("source/main.lua"), str(self.app / "source" / "main.lua"))

    def test_the_player_of_this_machine_runs_the_staged_package(self):
        self.write_plugin(self.scratch / "far-plugin", "far")
        self.write_app({"far": {"path": "../far-plugin"}})
        session = mock.Mock(arguments=["--dev-server", "ws://127.0.0.1:8000"])
        packages = []

        @contextlib.contextmanager
        def started(app, package, args, device=None):
            packages.append(package)
            yield session

        patches = [mock.patch.object(haylen, "compile_app_shaders"), mock.patch.object(haylen, "command_build"), mock.patch.object(haylen, "prepare_host_native", return_value=False), mock.patch.object(haylen, "run_session", started), mock.patch.object(haylen, "run_rows", return_value=[])]
        with contextlib.ExitStack() as stack:
            for patch in patches:
                stack.enter_context(patch)
            haylen.command_run(haylen.build_parser().parse_args(["run", str(self.app)]))
        staged = haylen.App(self.app, haylen.host_name()).build_folder / "package"
        self.assertEqual(packages, [staged])
        self.assertEqual(session.follow.call_args.args[0][-1], staged)
        self.assertTrue((staged / "plugins" / "far" / "plugin.json").is_file())

    def test_plugin_add_references_a_folder_or_a_repository_and_plugin_list_names_the_source(self):
        far = self.write_plugin(self.scratch / "far-plugin", "far", {"greeting": {"type": "string", "default": "Hi", "description": "Greets."}})
        self.write_plugin(self.app / "plugins" / "far", "far")
        repository = self.make_repository("remote")
        self.write_app({"far": {"parameters": {"greeting": "Hello"}}})
        haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", str(far), "--reference", "--app", str(self.app)]))
        haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", "file://" + str(repository), "--ref", "1.0.0", "--reference", "--app", str(self.app)]))
        listed = json.loads((self.app / "app.json").read_text())["plugins"]
        self.assertEqual(listed["far"], {"path": "../far-plugin", "parameters": {"greeting": "Hello"}}, "A reference keeps the values that the app gives.")
        self.assertEqual(listed["remote"], {"git": "file://" + str(repository), "ref": "1.0.0"})
        self.assertFalse((self.app / "plugins").exists(), "A reference deletes the copy that a player would take.")

        output = io.StringIO()
        listing = haylen.Terminal(output, io.StringIO(), {})
        listing.log = self.scratch / "haylen.log"
        with mock.patch.object(haylen, "terminal", listing):
            haylen.command_plugin_list(haylen.build_parser().parse_args(["plugin", "list", "--app", str(self.app)]))
        self.assertIn(f"From {far}.", output.getvalue())
        self.assertIn(f'From file://{repository} at "1.0.0".', output.getvalue())

        # A copy takes the place of a reference again.
        haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", str(far), "--app", str(self.app)]))
        self.assertEqual(json.loads((self.app / "app.json").read_text())["plugins"]["far"], {"parameters": {"greeting": "Hello"}})
        self.assertTrue((self.app / "plugins" / "far" / "plugin.json").is_file())

    def test_plugin_add_writes_no_default_and_every_project_takes_the_defaults_of_the_plugin(self):
        parameters = {"lens": {"type": "string", "default": "wide", "description": "Picks the lens."}, "testMode": {"type": "boolean", "default": False, "description": "Tests."}, "appId": {"type": "string", "required": True, "description": "Names the app."}}
        far = self.write_plugin(self.scratch / "far-plugin", "far", parameters)
        manifest = json.loads((far / "plugin.json").read_text())
        manifest["platforms"] = ["ios", "android", "web"]
        manifest["apple"] = {"infoPlist": {"FarLens": "${lens}", "FarTestMode": "${testMode}", "FarAppId": "${appId}"}}
        manifest["android"] = {"module": "android", "placeholders": {"farLens": "${lens}"}}
        manifest["web"] = {"module": "web/far.js"}
        (far / "plugin.json").write_text(json.dumps(manifest))
        for path in ("android/build.gradle.kts", "android/src/main/AndroidManifest.xml", "web/far.js"):
            (far / path).parent.mkdir(parents=True, exist_ok=True)
            (far / path).write_text("")
        self.write_app({})

        # Only the values of the app and the empty text of a required parameter reach "app.json", so a newer version of the plugin brings its own defaults.
        haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", str(far), "--reference", "--app", str(self.app)]))
        self.assertEqual(json.loads((self.app / "app.json").read_text())["plugins"]["far"], {"path": "../far-plugin", "parameters": {"appId": ""}})
        self.write_app({"far": {"path": "../far-plugin", "parameters": {"appId": "app-1234"}}})
        manifest["parameters"]["lens"]["default"] = "tele"
        (far / "plugin.json").write_text(json.dumps(manifest))
        values = {"lens": "tele", "testMode": False, "appId": "app-1234"}

        # The projects of Lua and C++ apps take the same values: the Info.plist keys of Apple platforms, the placeholders and the plugin list of Android, and the config of the web.
        apple = haylen.App(self.app, "ios")
        self.assertEqual(apple.plugin_values["far"], values)
        self.assertEqual(haylen.apple_plugin_keys(apple, ("ios",), "infoPlist", {}, {}, '"Info.plist"'), {"FarLens": "tele", "FarTestMode": False, "FarAppId": "app-1234"})
        android = haylen.App(self.app, "android")
        root = self.scratch / "android"
        self.assertEqual(haylen.prepare_android_plugins(android, root)["placeholder.farLens"], "tele")
        haylen.write_android_plugins(android, root / "assets")
        self.assertEqual(json.loads((root / "assets" / haylen.ANDROID_PLUGINS_FILE).read_text())["plugins"][0]["parameters"], values)
        self.assertEqual(haylen.write_web_plugins(haylen.App(self.app, "web"), self.scratch / "site")[0]["parameters"], values)

    def test_plugin_add_references_a_folder_by_its_short_path_through_symbolic_links(self):
        self.write_plugin(self.scratch / "real" / "camera", "camera")
        (self.scratch / "real" / "apps").mkdir()
        shutil.move(self.app, self.scratch / "real" / "apps" / "app")
        (self.scratch / "linked").symlink_to(self.scratch / "real")
        self.app.symlink_to(self.scratch / "real" / "apps" / "app")
        self.write_app({})

        # The app and the plugin are named through other links than the ones they lie behind, as "/tmp" and "/private/tmp" of macOS.
        haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", str(self.scratch / "linked" / "camera"), "--reference", "--app", str(self.app)]))
        self.assertEqual(json.loads((self.app / "app.json").read_text())["plugins"]["camera"], {"path": "../../camera"})
        self.assertEqual(haylen.App(self.app, "web").plugins[0].folder, self.scratch / "real" / "camera")

        # A folder that shares only the root of the file system with the app stays absolute, since a path through the root breaks once the app moves.
        far = Path("/haylen-plugins/camera").resolve()
        self.assertEqual(haylen.app_relative(Path("/haylen-apps/app"), far), far.as_posix())
        self.assertEqual(haylen.app_relative(Path("/haylen-apps/app"), Path("/haylen-apps/plugins/camera")), "../plugins/camera")

    def test_plugin_add_refuses_an_entry_whose_values_lie_outside_its_parameters(self):
        far = self.write_plugin(self.scratch / "far-plugin", "far", {"prefetchPacks": {"type": "array", "default": [], "description": "Packs."}})
        self.write_app({"far": {"prefetchPacks": ["intro"]}})
        written = (self.app / "app.json").read_text()
        for options in (["--reference"], []):
            with self.subTest(options=options):
                with self.assertRaisesRegex(haylen.BuildError, 'The key "plugins.far.prefetchPacks" is not a key of a plugin entry'):
                    haylen.command_plugin_add(haylen.build_parser().parse_args(["plugin", "add", str(far), *options, "--app", str(self.app)]))
                self.assertEqual((self.app / "app.json").read_text(), written, "The values of the app stay where they are.")
                self.assertFalse((self.app / "plugins").exists())

    def test_cpp_apps_take_the_folders_of_their_plugins_from_haylen_py(self):
        far = self.write_plugin(self.scratch / "far-plugin", "far")
        self.write_plugin(self.app / "plugins" / "local", "local")
        self.write_app({"local": {}, "far": {"path": "../far-plugin"}})
        build = self.scratch / "build"
        options = haylen.cpp_plugin_options(self.app, build)
        folders = build / haylen.CPP_PLUGIN_FOLDERS_FILE
        self.assertEqual(options, [f"-DHAYLEN_PLUGIN_FOLDERS_FILE={folders.as_posix()}"])
        self.assertIn(f"set(HAYLEN_PLUGIN_FOLDER_far [[{far.as_posix()}]])", folders.read_text())
        if shutil.which("cmake") is None:
            self.skipTest('The command "cmake" runs the part of "haylen_add_app" that reads the folders.')

        # The CMake of haylen_add_app places each plugin at its id, and without the folders of haylen.py it refuses a reference.
        script = self.scratch / "entries.cmake"
        script.write_text(f'include("{(haylen.ENGINE_DIR / "cmake" / "haylen-app.cmake").as_posix()}")\nhaylen_read_package_entries("${{PACKAGE}}" entries paths)\nforeach(entry path IN ZIP_LISTS entries paths)\n  message("${{entry}}=${{path}}")\nendforeach()\n')
        listed = subprocess.run(["cmake", f"-DPACKAGE={self.app.as_posix()}", f"-DHAYLEN_PLUGIN_FOLDERS_FILE={folders.as_posix()}", "-P", str(script)], capture_output=True, text=True, check=True).stderr.splitlines()
        self.assertEqual(listed, [f"app.json={(self.app / 'app.json').as_posix()}", f"source={(self.app / 'source').as_posix()}", f"content={(self.app / 'content').as_posix()}", f"plugins/far/plugin.json={(far / 'plugin.json').as_posix()}", f"plugins/far/source={(far / 'source').as_posix()}", f"plugins/local/plugin.json={(self.app / 'plugins' / 'local' / 'plugin.json').as_posix()}", f"plugins/local/source={(self.app / 'plugins' / 'local' / 'source').as_posix()}"])
        refused = subprocess.run(["cmake", f"-DPACKAGE={self.app.as_posix()}", "-P", str(script)], capture_output=True, text=True)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn('which haylen.py resolves. Build the app with "haylen run"', " ".join(refused.stderr.split()), "CMake wraps the lines of its errors.")

        # The desktop builds link every entry from where it lies, so a saved file of a referenced plugin shows without a build.
        entries = [line.split("=", 1) for line in listed]
        linked = self.scratch / "linked"
        subprocess.run(["cmake", f"-DDESTINATION={linked.as_posix()}", f"-DENTRIES={'|'.join(entry for entry, _ in entries)}", f"-DPATHS={'|'.join(path for _, path in entries)}", "-P", str(haylen.ENGINE_DIR / "cmake" / "haylen-link-content.cmake")], capture_output=True, check=True)
        self.assertEqual((linked / "plugins" / "far" / "source").resolve(), far / "source")
        self.assertEqual((linked / "plugins" / "far" / "plugin.json").read_text(), (far / "plugin.json").read_text())
        self.assertFalse((linked / "content").exists(), "An entry that the app lacks stays out.")


class CommandNamesTest(unittest.TestCase):
    def test_every_command_that_the_workflows_and_the_docs_run_exists(self):
        commands = {action.dest: action for action in haylen.build_parser()._subparsers._group_actions}["command"].choices
        texts = [path.read_text(encoding="utf-8") for path in [*(haylen.ROOT / ".github" / "workflows").glob("*.yml"), *(haylen.ROOT / "docs").rglob("*.md")]]
        unknown = set()
        for text in texts:
            for match in re.finditer(r"(?:python3? \S*haylen\.py(?: --verbose)?|^haylen) ([a-z][a-z-]*)(?: ([a-z][a-z-]*))?", text, re.MULTILINE):
                command, action = match[1], match[2]
                if command not in commands:
                    unknown.add(command)
                    continue
                groups = [choice for choice in commands[command]._actions if isinstance(choice, argparse._SubParsersAction)]
                if groups and action not in groups[0].choices:
                    unknown.add(f"{command} {action}")
        self.assertEqual(sorted(unknown), [], "These commands of the workflows and the docs do not exist.")


class DevicesTest(unittest.TestCase):
    SIMULATORS = {"devices": {
        "com.apple.CoreSimulator.SimRuntime.iOS-27-2": [{"name": "iPhone 17e", "udid": "PHONE", "state": "Booted"}],
        "com.apple.CoreSimulator.SimRuntime.xrOS-27-0": [{"name": "Apple Vision Pro", "udid": "VISION", "state": "Shutdown"}],
        "com.apple.CoreSimulator.SimRuntime.watchOS-27-0": [{"name": "Apple Watch", "udid": "WATCH", "state": "Shutdown"}],
    }}
    PHYSICAL = {"result": {"devices": [
        {"properties": {"hardware": {"reality": "physical", "platform": "iOS", "udid": "00008110"}, "software": {"osVersionNumber": {"stringValue": "27.0.1"}}, "state": {"name": "Phone of Ana"}, "connection": {"state": "connected"}}},
        {"properties": {"hardware": {"reality": "physical", "platform": "tvOS", "udid": "0766"}, "software": {"osVersionNumber": {"stringValue": "26.6"}}, "state": {"name": "Living Room"}, "connection": {"state": "unavailable"}}},
        {"properties": {"hardware": {"reality": "simulated", "platform": "iOS", "udid": "PHONE"}, "software": {"osVersionNumber": {"stringValue": "27.2"}}, "state": {"name": "iPhone 17e"}, "connection": {"state": "disconnected"}}},
    ]}}

    def answer(self, command: list) -> str:
        parts = [str(part) for part in command]
        if parts[:3] == ["xcrun", "simctl", "list"]:
            return json.dumps(self.SIMULATORS)
        if parts[:2] == ["xcrun", "devicectl"]:
            Path(parts[parts.index("--json-output") + 1]).write_text(json.dumps(self.PHYSICAL))
            return ""
        if parts[1:] == ["devices"]:
            return "List of devices attached\nemulator-5554\tdevice\nR58M\tdevice\nZX1G\tunauthorized\n\n"
        if parts[-3:] == ["emu", "avd", "name"]:
            return "Pixel_10a\nOK\n"
        if parts[-1] == "ro.product.model":
            return "Pixel 9\n"
        if parts[-1] == "ro.build.version.release":
            return "16\n"
        if parts[-1] == "-list-avds":
            return "Pixel_10a\nTablet\n"
        raise AssertionError(f"Unexpected command {parts}")

    def test_every_platform_lists_its_devices_with_the_identifier_of_device(self):
        with mock.patch.object(haylen, "capture", side_effect=self.answer), mock.patch.object(haylen, "host_name", return_value="macos"), mock.patch.object(haylen, "find_android_sdk", return_value=Path("sdk")):
            devices, idle = haylen.machine_devices()
        listed = [(device.name, device.identifier, device.platforms, device.system, device.kind) for device in devices[1:]]
        self.assertEqual(devices[0].platforms, "macos, catalyst")
        self.assertEqual(listed, [
            ("iPhone 17e", "PHONE", "ios-simulator", "iOS 27.2", "Simulator, booted"),
            ("Apple Vision Pro", "VISION", "visionos-simulator", "visionOS 27.0", "Simulator"),
            ("Phone of Ana", "00008110", "ios", "iOS 27.0.1", "Device, connected"),
            ("Living Room", "0766", "tvos", "tvOS 26.6", "Device, unavailable"),
            ("Pixel_10a", "emulator-5554", "android", "Android 16", "Emulator, running"),
            ("Pixel 9", "R58M", "android", "Android 16", "Device"),
            ("ZX1G", "ZX1G", "android", "", "Device, unauthorized"),
        ])
        self.assertEqual(idle, ["Tablet"])

    def test_a_machine_without_the_tools_of_a_platform_lists_only_what_it_has(self):
        with mock.patch.object(haylen, "capture", side_effect=AssertionError("No tool runs.")), mock.patch.object(haylen, "host_name", return_value="linux"), mock.patch.object(haylen.host_platform, "freedesktop_os_release", return_value={"PRETTY_NAME": "Ubuntu 26.04"}), mock.patch.object(haylen, "find_android_sdk", return_value=None):
            devices, idle = haylen.machine_devices()
        self.assertEqual([(device.platforms, device.system) for device in devices], [("linux", "Ubuntu 26.04")])
        self.assertEqual(idle, [])

    def test_a_device_that_is_not_there_points_to_the_list_of_devices(self):
        with mock.patch.object(haylen, "capture", return_value="List of devices attached\nemulator-5554\tdevice\n"), mock.patch.object(haylen, "find_android_sdk", return_value=Path("sdk")):
            with self.assertRaisesRegex(haylen.BuildError, 'The device "R58M" is not a connected Android device. Use the command "haylen devices"'):
                haylen.android_device("R58M")
        with mock.patch.object(haylen, "capture", return_value=json.dumps(self.SIMULATORS)):
            with self.assertRaisesRegex(haylen.BuildError, 'No available iOS simulator has the name or identifier "iPad". Use the command "haylen devices"'):
                haylen.apple_simulator("iOS", "iPad")


class ConsumerArgumentTest(unittest.TestCase):
    def test_the_consumer_check_builds_the_listed_ways_or_every_way(self):
        parser = haylen.build_parser()
        self.assertEqual(haylen.consumer_modes(parser.parse_args(["sdk"])), [])
        self.assertEqual(haylen.consumer_modes(parser.parse_args(["sdk", "--check-consumers"])), ["subdirectory", "cpm", "package"])
        self.assertEqual(haylen.consumer_modes(parser.parse_args(["sdk", "--check-consumers", "package"])), ["package"])

    def test_the_consumer_check_runs_only_for_this_machine(self):
        with self.assertRaisesRegex(haylen.BuildError, '"--check-consumers" builds consumer projects for this machine'):
            haylen.consumer_modes(haylen.build_parser().parse_args(["sdk", "--platform", "web", "--check-consumers"]))


class RunTest(unittest.TestCase):
    def test_a_streamed_command_leaves_the_input_of_the_terminal_to_the_developer_console(self):
        with mock.patch.object(haylen.subprocess, "run") as started, mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            haylen.run(["xcrun", "devicectl", "device", "process", "launch", "--console"], stream=True)
        self.assertIs(started.call_args.kwargs["stdin"], haylen.subprocess.DEVNULL)


class AppShadersTest(unittest.TestCase):
    def test_a_new_pin_of_the_shader_compiler_compiles_the_shaders_of_an_app_again(self):
        with tempfile.TemporaryDirectory() as scratch:
            app = Path(scratch) / "app"
            (app / "content" / "shaders").mkdir(parents=True)
            source = app / "content" / "shaders" / "glow.glsl"
            source.write_text("@program glow haylen_vs glow_fs\n", encoding="utf-8")
            compiled = source.with_suffix(".shader")
            compiled.write_text("{}", encoding="utf-8")
            pin = Path(scratch) / "haylen-shader-compiler.json"
            pin.write_text("{}", encoding="utf-8")
            newest = max(path.stat().st_mtime for path in haylen.SHADER_LIBRARY_DIR.rglob("*.glsl"))
            os.utime(source, (newest, newest))
            os.utime(compiled, (newest + 10, newest + 10))
            os.utime(pin, (newest + 20, newest + 20))
            with mock.patch.object(haylen, "SHADER_COMPILER_PIN", pin), mock.patch.object(haylen, "compile_shader") as compile_shader:
                haylen.compile_app_shaders(app)
            compile_shader.assert_called_once_with(source, compiled)

    def test_a_shader_file_that_goes_away_while_the_watcher_reads_it_keeps_the_watcher_running(self):
        with tempfile.TemporaryDirectory() as scratch:
            app = Path(scratch) / "app"
            shaders = app / "content" / "shaders"
            shaders.mkdir(parents=True)
            source = shaders / "glow.glsl"
            source.write_text("@program glow haylen_vs glow_fs\n", encoding="utf-8")
            # An editor deletes its scratch file between the listing of the folder and the read or the time of the file.
            vanished = shaders / "glow.glsl~.glsl"
            listing = Path.rglob

            def listed(folder: Path, pattern: str) -> list[Path]:
                found = list(listing(folder, pattern))
                return [*found, vanished] if folder == shaders else found

            with mock.patch.object(haylen.Path, "rglob", listed):
                self.assertEqual(haylen.shader_sources(app), [source])
                self.assertGreaterEqual(haylen.newest_shader_source(app), source.stat().st_mtime)
            with mock.patch.object(haylen, "ensure_shaderc") as compiler:
                haylen.compile_shader(vanished, vanished.with_suffix(".shader"))
            compiler.assert_not_called()


class ShaderCompilerTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        haylen.ensure_shaderc.cache_clear()
        self.addCleanup(haylen.ensure_shaderc.cache_clear)

    def test_another_shader_compiler_or_python_configures_the_build_tree_again(self):
        compiler = self.scratch / "tools" / "haylen-shaderc-new" / "haylen-shaderc"
        tree = self.scratch / "build" / "macos-debug"
        tree.mkdir(parents=True)
        arguments = argparse.Namespace(platform="macos", config="Debug", sanitizers=None)
        (tree / haylen.CONFIGURED_MARKER).write_text("")
        with mock.patch.object(haylen, "BUILD_ROOT", self.scratch / "build"), mock.patch.object(haylen, "ensure_shaderc", return_value=compiler), mock.patch.object(haylen, "command_configure") as configure:
            (tree / "CMakeCache.txt").write_text(f"HAYLEN_SHADERC:FILEPATH={(self.scratch / 'tools' / 'haylen-shaderc').as_posix()}\nPython3_EXECUTABLE:FILEPATH={haylen.python_executable()}\n")
            haylen.ensure_configured(arguments)
            self.assertEqual(configure.call_count, 1)
            (tree / "CMakeCache.txt").write_text(f"HAYLEN_SHADERC:FILEPATH={compiler.as_posix()}\nPython3_EXECUTABLE:FILEPATH={haylen.python_executable()}\n")
            haylen.ensure_configured(arguments)
            self.assertEqual(configure.call_count, 1)

            # A tree that another Python configured, or one whose cache misses a value that the tool reads, is configured again instead of stopping.
            (tree / "CMakeCache.txt").write_text(f"HAYLEN_SHADERC:FILEPATH={compiler.as_posix()}\nPython3_EXECUTABLE:FILEPATH=/usr/bin/python3.11\n")
            haylen.ensure_configured(arguments)
            self.assertEqual(configure.call_count, 2)
            (tree / "CMakeCache.txt").write_text(f"HAYLEN_SHADERC:FILEPATH={compiler.as_posix()}\n")
            haylen.ensure_configured(arguments)
            self.assertEqual(configure.call_count, 3)

    def test_a_new_pin_of_the_compiler_configures_the_trees_of_the_apple_slices_again(self):
        compiler = self.scratch / "tools" / "haylen-shaderc-new" / "haylen-shaderc"
        tree = self.scratch / "engine" / "apple-ios-arm64-release"
        tree.mkdir(parents=True)
        (tree / haylen.CONFIGURED_MARKER).write_text("")
        (tree / "CMakeCache.txt").write_text(f"HAYLEN_SHADERC:FILEPATH={(self.scratch / 'tools' / 'haylen-shaderc-old' / 'haylen-shaderc').as_posix()}\n")
        configured = []
        with mock.patch.object(haylen, "ENGINE_BUILDS_DIR", self.scratch / "engine"), mock.patch.object(haylen, "ARTIFACTS_DIR", self.scratch / "artifacts"), mock.patch.object(haylen, "ensure_shaderc", return_value=compiler), mock.patch.object(haylen, "require_host"), mock.patch.object(haylen, "shutil"), mock.patch.object(haylen, "run"), mock.patch.object(haylen, "configure_tree", side_effect=lambda command, directory, *_: configured.append(directory)):
            haylen.build_apple_artifacts("Release", 4, ["ios"], [])
            self.assertEqual(configured, [tree])
            (tree / "CMakeCache.txt").write_text(f"HAYLEN_SHADERC:FILEPATH={compiler.as_posix()}\n")
            haylen.build_apple_artifacts("Release", 4, ["ios"], [])
        self.assertEqual(configured, [tree])

    def test_a_tree_whose_configure_failed_configures_again(self):
        compiler = self.scratch / "tools" / "haylen-shaderc"
        tree = self.scratch / "build" / "macos-debug"
        arguments = argparse.Namespace(platform="macos", config="Debug", sanitizers=None)

        # A configure that fails leaves the cache of CMake without the files of its generator and without the record of a configured tree.
        with mock.patch.object(haylen, "run", side_effect=haylen.BuildError("The configure failed.")):
            with self.assertRaises(haylen.BuildError):
                haylen.configure_tree(["cmake", "-B", tree], tree)
        tree.mkdir(parents=True)
        (tree / "CMakeCache.txt").write_text(f"HAYLEN_SHADERC:FILEPATH={compiler.as_posix()}\nPython3_EXECUTABLE:FILEPATH={haylen.python_executable()}\n")
        self.assertFalse(haylen.is_configured(tree))
        with mock.patch.object(haylen, "BUILD_ROOT", self.scratch / "build"), mock.patch.object(haylen, "ensure_shaderc", return_value=compiler), mock.patch.object(haylen, "command_configure") as configure:
            haylen.ensure_configured(arguments)
        self.assertEqual(configure.call_count, 1)

        # A configure that succeeds records the tree, which the next command builds without configuring it again.
        with mock.patch.object(haylen, "run") as run:
            haylen.configure_tree(["cmake", "-B", tree], tree)
        run.assert_called_once()
        self.assertTrue(haylen.is_configured(tree))
        with mock.patch.object(haylen, "BUILD_ROOT", self.scratch / "build"), mock.patch.object(haylen, "ensure_shaderc", return_value=compiler), mock.patch.object(haylen, "command_configure") as configure:
            haylen.ensure_configured(arguments)
        configure.assert_not_called()

        # A configure that starts again and fails takes the record away.
        with mock.patch.object(haylen, "run", side_effect=haylen.BuildError("The configure failed.")):
            with self.assertRaises(haylen.BuildError):
                haylen.configure_tree(["cmake", "-B", tree], tree)
        self.assertFalse(haylen.is_configured(tree))

    def test_the_variable_names_a_build_of_the_compiler_to_use_instead_of_the_pinned_one(self):
        compiler = self.scratch / "haylen-shaderc"
        compiler.write_text("compiler")
        with mock.patch.dict(os.environ, {haylen.SHADER_COMPILER_VARIABLE: str(compiler)}), mock.patch.object(haylen, "run") as run:
            self.assertEqual(haylen.ensure_shaderc(), compiler.resolve())
        run.assert_not_called()
        haylen.ensure_shaderc.cache_clear()
        with mock.patch.dict(os.environ, {haylen.SHADER_COMPILER_VARIABLE: str(self.scratch / "missing")}):
            with self.assertRaisesRegex(haylen.BuildError, haylen.SHADER_COMPILER_VARIABLE):
                haylen.ensure_shaderc()

    def archive(self, name: str, files: dict[str, str]) -> tuple[str, str]:
        """Writes an archive like the ones of the releases, a zip or a gzipped tar after its name, and returns its URL and SHA-256."""
        target = self.scratch / "server" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if name.endswith(".zip"):
            with zipfile.ZipFile(target, "w") as package:
                for path, text in files.items():
                    package.writestr(path, text)
        else:
            with tarfile.open(target, "w:gz") as package:
                for path, text in files.items():
                    info = tarfile.TarInfo(path)
                    info.size = len(text.encode())
                    info.mode = 0o755
                    package.addfile(info, io.BytesIO(text.encode()))
        return target.as_uri(), hashlib.sha256(target.read_bytes()).hexdigest()

    def provide(self, pins: dict, environment: dict[str, str] | None = None) -> tuple[subprocess.CompletedProcess, Path]:
        """Runs the script of the engine with the pins as a machine would, and returns the run and the compiler it named."""
        (self.scratch / "pins.json").write_text(json.dumps(pins))
        result = self.scratch / "result"
        result.unlink(missing_ok=True)
        command = ["cmake", f"-DHAYLEN_SHADERC_CACHE={(self.scratch / 'cache').as_posix()}", f"-DHAYLEN_SHADERC_RESULT={result.as_posix()}", f"-DHAYLEN_SHADERC_PINS={(self.scratch / 'pins.json').as_posix()}", "-P", haylen.SHADER_COMPILER_SCRIPT]
        run = subprocess.run(command, capture_output=True, text=True, env={**os.environ, **(environment or {})})
        return run, Path(result.read_text()) if result.exists() else None

    def host(self) -> str:
        return f"{haylen.host_name()}-{haylen.host_arch()}"

    def test_the_release_binary_of_the_machine_downloads_once_into_a_folder_of_its_commit(self):
        executable = haylen.executable_name("haylen-shaderc")
        name = "haylen-shaderc-9.9.9-host.zip" if haylen.host_name() == "windows" else "haylen-shaderc-9.9.9-host.tar.gz"
        url, digest = self.archive(name, {executable: "compiler", "LICENSE": "license"})
        pins = {"version": "9.9.9", "commit": "abcdef0123456789", "source": {"url": "file:///missing.tar.gz", "sha256": "0" * 64}, "binaries": {self.host(): {"url": url, "sha256": digest}}}
        run, compiler = self.provide(pins)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(compiler, self.scratch / "cache" / "haylen-shaderc-abcdef0" / executable)
        self.assertEqual(compiler.read_text(), "compiler")
        self.assertTrue(os.access(compiler, os.X_OK))
        self.assertEqual([path.name for path in compiler.parent.iterdir()], [executable])

        (self.scratch / "server").rename(self.scratch / "gone")
        run, again = self.provide(pins)
        self.assertEqual((run.returncode, again), (0, compiler))

        # The release keeps its version when it is rebuilt at another commit, so the new pin downloads the new binary.
        url, digest = self.archive(name, {executable: "rebuilt"})
        run, rebuilt = self.provide({**pins, "commit": "1234567890abcdef", "binaries": {self.host(): {"url": url, "sha256": digest}}})
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(rebuilt, self.scratch / "cache" / "haylen-shaderc-1234567" / executable)
        self.assertEqual(rebuilt.read_text(), "rebuilt")

    def test_a_download_that_differs_from_its_digest_fails_and_keeps_nothing(self):
        url, digest = self.archive("haylen-shaderc-9.9.9-host.tar.gz", {"haylen-shaderc": "compiler"})
        pins = {"version": "9.9.9", "commit": "abcdef0123456789", "source": {"url": url, "sha256": digest}, "binaries": {self.host(): {"url": url, "sha256": "0" * 64}}}
        run, compiler = self.provide(pins)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("haylen-shaderc-9.9.9-host.tar.gz", run.stderr)
        self.assertIsNone(compiler)
        self.assertEqual(list((self.scratch / "cache" / "haylen-shaderc-abcdef0").iterdir()), [])

    def test_a_machine_without_a_binary_builds_the_commit_without_the_toolchain_of_a_cross_build(self):
        executable = haylen.executable_name("haylen-shaderc")
        project = f"""cmake_minimum_required(VERSION 3.28)
project(compiler LANGUAGES NONE)
if(HAYLEN_SHADERC_BUILD_TESTS)
  message(FATAL_ERROR "The tests of the compiler are on.")
endif()
add_custom_target(haylen-shaderc ALL COMMAND ${{CMAKE_COMMAND}} -E copy ${{CMAKE_CURRENT_SOURCE_DIR}}/compiler ${{CMAKE_BINARY_DIR}}/bin/{executable})
"""
        url, digest = self.archive("abcdef0123456789.tar.gz", {"haylen-shader-compiler-abcdef0123456789/CMakeLists.txt": project, "haylen-shader-compiler-abcdef0123456789/compiler": "built"})
        pins = {"version": "9.9.9", "commit": "abcdef0123456789", "source": {"url": url, "sha256": digest}, "binaries": {"other-host": {"url": "file:///missing.tar.gz", "sha256": "0" * 64}}}
        run, compiler = self.provide(pins, {"CMAKE_TOOLCHAIN_FILE": str(self.scratch / "missing-toolchain.cmake")})
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertEqual(compiler, self.scratch / "cache" / "haylen-shaderc-abcdef0" / executable)
        self.assertEqual(compiler.read_text(), "built")
        self.assertEqual([path.name for path in compiler.parent.iterdir()], [executable])

    def test_haylen_py_takes_the_compiler_that_the_script_of_the_engine_provides(self):
        script = self.scratch / "cmake" / "haylen-shader-compiler.cmake"
        script.parent.mkdir()
        shutil.copy2(haylen.SHADER_COMPILER_SCRIPT, script)
        executable = haylen.executable_name("haylen-shaderc")
        url, digest = self.archive("haylen-shaderc-9.9.9-host.zip", {executable: "compiler"})
        (script.parent / "haylen-shader-compiler.json").write_text(json.dumps({"version": "9.9.9", "commit": "abcdef0123456789", "binaries": {self.host(): {"url": url, "sha256": digest}}}))
        environment = {key: value for key, value in os.environ.items() if key != haylen.SHADER_COMPILER_VARIABLE}
        with mock.patch.dict(os.environ, environment, clear=True), mock.patch.object(haylen, "SHADER_COMPILER_SCRIPT", script), mock.patch.object(haylen, "TOOLS_DIR", self.scratch / "tools"), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            self.assertEqual(haylen.ensure_shaderc(), self.scratch / "tools" / "haylen-shaderc-abcdef0" / executable)

    def test_the_pins_name_the_archives_of_their_release_and_the_source_of_their_commit(self):
        pins = json.loads((haylen.SHADER_COMPILER_SCRIPT.parent / "haylen-shader-compiler.json").read_text())
        version, commit = pins["version"], pins["commit"]
        self.assertTrue(pins["source"]["url"].endswith(f"/archive/{commit}.tar.gz"))
        for host, binary in pins["binaries"].items():
            self.assertIn(f"/releases/download/v{version}/haylen-shaderc-{version}-", binary["url"], host)
            self.assertRegex(binary["sha256"], "^[0-9a-f]{64}$", host)
        self.assertEqual(pins["binaries"]["macos-arm64"], pins["binaries"]["macos-x64"])


class XcodeGenTest(unittest.TestCase):
    def test_a_new_pin_downloads_its_own_release(self):
        scratch = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, scratch)
        older = scratch / "xcodegen-1.0.0" / "xcodegen" / "bin" / "xcodegen"
        older.parent.mkdir(parents=True)
        older.write_text("older")

        def download(url: str, archive: Path, sha256: str | None = None) -> None:
            self.assertIn(f"/{haylen.XCODEGEN_VERSION}/", url)
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("xcodegen/bin/xcodegen", "pinned")

        with mock.patch.object(haylen, "TOOLS_DIR", scratch), mock.patch.object(haylen, "require_host"), mock.patch.object(haylen, "download", side_effect=download):
            installed = haylen.ensure_xcodegen()
        self.assertEqual(installed, scratch / f"xcodegen-{haylen.XCODEGEN_VERSION}" / "xcodegen" / "bin" / "xcodegen")
        self.assertEqual(installed.read_text(), "pinned")


class GradleTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.executable = "gradle.bat" if haylen.host_name() == "windows" else "gradle"
        self.downloads: list[tuple[str, str | None]] = []

    def download(self, url: str, archive: Path, sha256: str | None = None) -> None:
        self.downloads.append((url, sha256))
        with zipfile.ZipFile(archive, "w") as package:
            package.writestr(f"gradle-{haylen.GRADLE_VERSION}/bin/{self.executable}", "pinned")
            package.writestr(f"gradle-{haylen.GRADLE_VERSION}/lib/gradle.jar", "library")

    def test_the_pinned_distribution_moves_into_place_only_once_it_is_unpacked(self):
        folder = self.scratch / f"gradle-{haylen.GRADLE_VERSION}"
        with mock.patch.object(haylen, "TOOLS_DIR", self.scratch), mock.patch.object(haylen, "download", side_effect=self.download):
            # An unpack that stops halfway leaves no folder whose executable seems installed.
            with mock.patch.object(haylen.zipfile.ZipFile, "extractall", side_effect=OSError("No space left on device")), self.assertRaises(OSError):
                haylen.ensure_gradle()
            self.assertEqual(list(self.scratch.iterdir()), [])

            installed = haylen.ensure_gradle()
        self.assertEqual(installed, folder / "bin" / self.executable)
        self.assertEqual(installed.read_text(), "pinned")
        self.assertTrue(os.access(installed, os.X_OK))
        self.assertEqual((folder / "lib" / "gradle.jar").read_text(), "library")
        self.assertEqual(self.downloads, [(f"https://services.gradle.org/distributions/gradle-{haylen.GRADLE_VERSION}-bin.zip", haylen.GRADLE_SHA256)] * 2)
        self.assertEqual([path.name for path in self.scratch.iterdir()], [folder.name])

    def test_a_folder_that_an_interrupted_unpack_left_is_replaced(self):
        folder = self.scratch / f"gradle-{haylen.GRADLE_VERSION}"
        (folder / "lib").mkdir(parents=True)
        (folder / "lib" / "partial.jar").write_text("partial")
        with mock.patch.object(haylen, "TOOLS_DIR", self.scratch), mock.patch.object(haylen, "download", side_effect=self.download):
            installed = haylen.ensure_gradle()
        self.assertEqual(installed.read_text(), "pinned")
        self.assertFalse((folder / "lib" / "partial.jar").exists())
        self.assertEqual([path.name for path in self.scratch.iterdir()], [folder.name])


class DownloadTest(unittest.TestCase):
    def test_a_download_that_the_server_refuses_fails_with_its_reason_and_keeps_nothing(self):
        scratch = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, scratch)
        target = scratch / "tools" / "archive.zip"
        url = (scratch / "missing.zip").as_uri()
        with mock.patch.object(haylen, "terminal"), self.assertRaises(haylen.BuildError) as raised:
            haylen.download(url, target)
        self.assertIn(f"The download of `{url}` failed with", str(raised.exception))
        self.assertFalse(target.exists())


class AppImageToolsTest(unittest.TestCase):
    def test_a_new_pin_of_patchelf_downloads_its_own_release(self):
        scratch = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, scratch)
        folder = scratch / f"appimage-{haylen.APPIMAGETOOL_VERSION}"
        for path in (folder / "appimagetool" / "AppRun", folder / f"runtime-{haylen.APPIMAGE_RUNTIME_VERSION}-x86_64", folder / "patchelf-0.0.1" / "bin" / "patchelf"):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("older")

        def download(url: str, target: Path, sha256: str | None = None) -> None:
            target.parent.mkdir(parents=True, exist_ok=True)
            if "patchelf" not in url:
                target.write_text("pinned")
                return
            self.assertIn(f"/{haylen.PATCHELF_VERSION}/", url)
            binary = scratch / "patchelf"
            binary.write_text("pinned")
            with tarfile.open(target, "w:gz") as package:
                package.add(binary, "./bin/patchelf")

        def extract(command: list, cwd: Path) -> str:
            (cwd / "squashfs-root").mkdir()
            (cwd / "squashfs-root" / "AppRun").write_text("pinned")
            return ""

        with mock.patch.object(haylen, "TOOLS_DIR", scratch), mock.patch.object(haylen, "host_arch", return_value="x64"), mock.patch.object(haylen, "require_host"), mock.patch.object(haylen, "download", side_effect=download), mock.patch.object(haylen, "capture", side_effect=extract):
            tools = haylen.ensure_appimage_tools()
        self.assertEqual(tools["patchelf"], folder / f"patchelf-{haylen.PATCHELF_VERSION}" / "bin" / "patchelf")
        self.assertEqual(tools["patchelf"].read_text(), "pinned")


class EmscriptenTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.emscripten = self.scratch / "emsdk" / "upstream" / "emscripten"
        self.emscripten.mkdir(parents=True)
        (self.emscripten / ("emcmake.bat" if haylen.host_name() == "windows" else "emcmake")).write_text("")

    def installed(self, version: str) -> list[list[str]]:
        (self.emscripten / "emscripten-version.txt").write_text(f'"{version}"\n')
        with mock.patch.object(haylen, "TOOLS_DIR", self.scratch), mock.patch.object(haylen, "run") as run, mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            haylen.ensure_emsdk()
        return [[str(part) for part in call.args[0]] for call in run.call_args_list]

    def test_the_pinned_version_is_used_as_it_is(self):
        self.assertEqual(self.installed(haylen.EMSDK_VERSION), [])

    def test_another_version_takes_the_newest_releases_and_installs_the_pinned_one(self):
        commands = self.installed("1.0.0")
        self.assertEqual(commands[0][:2], ["git", "fetch"])
        self.assertEqual([command[1:] for command in commands[2:]], [["install", haylen.EMSDK_VERSION], ["activate", haylen.EMSDK_VERSION]])

    def test_a_web_tree_of_another_version_builds_again_from_nothing(self):
        tree = self.scratch / "build" / "web-release"
        tree.mkdir(parents=True)
        (tree / "CMakeCache.txt").write_text("HAYLEN_SHADERC:FILEPATH=haylen-shaderc\n")
        (tree / "object.o").write_bytes(b"object")
        (tree / haylen.EMSCRIPTEN_MARKER).write_text("1.0.0")
        self.assertTrue(haylen.is_stale_web_tree(tree))
        with mock.patch.object(haylen, "run", side_effect=lambda command, env: tree.mkdir()) as run:
            haylen.configure_web_tree(["emcmake", "cmake"], tree)
        run.assert_called_once()
        self.assertFalse((tree / "object.o").exists())
        (tree / "CMakeCache.txt").write_text("HAYLEN_SHADERC:FILEPATH=haylen-shaderc\n")
        self.assertFalse(haylen.is_stale_web_tree(tree))

    def test_a_web_app_links_again_when_the_scripts_of_the_runtime_change(self):
        if shutil.which("cmake") is None or shutil.which("ninja") is None:
            self.skipTest('The commands "cmake" and "ninja" configure the project that links the runtime.')
        source = self.scratch / "project"
        source.mkdir()
        (source / "main.c").write_text("int main(void) { return 0; }\n")
        # The project stands for a web app on any machine, so it leaves the Apple branch of the runtime aside, and it only configures, since only Emscripten links it.
        (source / "CMakeLists.txt").write_text(f"""cmake_minimum_required(VERSION 3.28)
project(probe LANGUAGES C)
set(APPLE FALSE)
set(HAYLEN_PLATFORM web)
set(HAYLEN_ENGINE_DIR "{(self.scratch / 'engine').as_posix()}")
set(HAYLEN_VARN_DATAVIEW_SCRIPT "{(self.scratch / 'varn-dataview.js').as_posix()}")
include("{(haylen.ENGINE_DIR / 'cmake' / 'haylen-app.cmake').as_posix()}")
add_library(runtime INTERFACE)
haylen_link_runtime_platform(runtime INTERFACE)
add_executable(app main.c)
target_link_libraries(app PRIVATE runtime)
""")
        subprocess.run(["cmake", "-G", "Ninja", "-S", str(source), "-B", str(self.scratch / "build")], capture_output=True, check=True)
        link = next(line for line in (self.scratch / "build" / "build.ninja").read_text().splitlines() if line.startswith("build app"))
        self.assertIn((self.scratch / "engine" / "platform" / "web" / "haylen-runtime.js").as_posix(), link.split(" | ", 1)[-1])
        self.assertIn((self.scratch / "varn-dataview.js").as_posix(), link.split(" | ", 1)[-1])

    def test_a_web_app_links_again_when_a_file_of_its_package_changes(self):
        if shutil.which("cmake") is None or shutil.which("ninja") is None:
            self.skipTest('The commands "cmake" and "ninja" configure the project of the app.')
        source = self.scratch / "project"
        (source / "source").mkdir(parents=True)
        (source / "app.json").write_text('{"name": "Probe", "identifier": "com.haylen.probe", "version": "1.0.0"}')
        (source / "source" / "main.lua").write_text("print('Hello.')\n")
        (source / "main.cpp").write_text("int main() { return 0; }\n")
        player = self.scratch / "engine" / "src" / "platform" / "LuaPlayer.cpp"
        player.parent.mkdir(parents=True)
        player.write_text("")
        # The project stands for a C++ web app on any machine and only configures, since only Emscripten links it.
        (source / "CMakeLists.txt").write_text(f"""cmake_minimum_required(VERSION 3.28)
project(probe LANGUAGES C CXX)
set(APPLE FALSE)
set(EMSCRIPTEN TRUE)
set(HAYLEN_ENGINE_DIR "{(self.scratch / 'engine').as_posix()}")
include("{(haylen.ENGINE_DIR / 'cmake' / 'haylen-app.cmake').as_posix()}")
add_library(runtime INTERFACE)
add_library(haylen::runtime ALIAS runtime)
haylen_add_app(main CPP SOURCES main.cpp PACKAGE "${{CMAKE_CURRENT_SOURCE_DIR}}")
""")
        subprocess.run(["cmake", "-G", "Ninja", "-S", str(source), "-B", str(self.scratch / "build")], capture_output=True, check=True)
        link = next(line for line in (self.scratch / "build" / "build.ninja").read_text().splitlines() if line.startswith("build bin/main/main.html"))
        self.assertIn((source / "source" / "main.lua").as_posix(), link.split(" | ", 1)[-1])
        self.assertIn((source / "app.json").as_posix(), link.split(" | ", 1)[-1])


class AndroidKeyTest(unittest.TestCase):
    def setUp(self):
        self.app = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.app)
        (self.app / "app.json").write_text("{}")

    def arguments(self, *extra: str):
        return haylen.build_parser().parse_args(["distribution", "android-key", str(self.app), *extra])

    def test_the_upload_key_of_release_builds_is_the_default(self):
        args = self.arguments()
        self.assertEqual((args.kind, args.alias, args.password, args.dname, args.force), ("release", "upload", "upload", "CN=Upload, OU=Upload, O=Upload, L=Upload, ST=Upload, C=BR", False))
        self.assertEqual(self.arguments("--debug").kind, "debug")
        self.assertEqual(self.arguments("--release", "--alias", "store", "--password", "secret1").alias, "store")

    def test_release_and_debug_exclude_each_other(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.arguments("--debug", "--release")

    def test_a_short_password_stops_before_anything_changes(self):
        with self.assertRaisesRegex(haylen.BuildError, "at least 6 characters"):
            haylen.command_distribution_android_key(self.arguments("--password", "short"))
        self.assertFalse((self.app / "platform").exists())

    def test_an_existing_key_stays_unless_forced(self):
        keystore = self.app / "platform" / "android" / "keystore" / "release.jks"
        keystore.parent.mkdir(parents=True)
        keystore.write_bytes(b"key")
        with self.assertRaisesRegex(haylen.BuildError, 'already has a release key.*"--force"'):
            haylen.command_distribution_android_key(self.arguments())
        self.assertEqual(keystore.read_bytes(), b"key")

    def test_the_properties_name_the_keystore_next_to_them_and_escape_the_values(self):
        properties = haylen.android_key_properties("release", "upload", "pa=ss✓")
        lines = properties.splitlines()
        self.assertTrue(lines[0].startswith("# "))
        self.assertEqual(lines[1:], ["storeFile=release.jks", "storePassword=pa=ss\\u2713", "keyAlias=upload", "keyPassword=pa=ss\\u2713"])


class TemplateLayersTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.scratch)
        self.output = io.StringIO()
        quiet = mock.patch.object(haylen, "terminal", haylen.Terminal(self.output, io.StringIO(), {}))
        quiet.start()
        self.addCleanup(quiet.stop)

    def use_layers(self, files: dict[str, str]) -> None:
        templates = self.scratch / "templates"
        for name, text in files.items():
            (templates / name).parent.mkdir(parents=True, exist_ok=True)
            (templates / name).write_text(text)
        layers = mock.patch.multiple(haylen, TEMPLATES_DIR=templates, COMMON_TEMPLATE=templates / "common")
        layers.start()
        self.addCleanup(layers.stop)

    def new_app(self, name: str, *options: str) -> Path:
        haylen.command_new(haylen.build_parser().parse_args(["new", str(self.scratch / name), *options]))
        return self.scratch / name

    def test_the_common_layer_comes_first_and_the_file_of_a_kind_replaces_the_common_one(self):
        self.use_layers({"common/notes.txt": "common", "common/content/logo.png": "logo", "lua/notes.txt": "lua", "lua/source/main.lua": "lua", "cpp/src/App.cpp": "cpp"})
        files = haylen.layered_files(haylen.template_layers("lua"))
        self.assertEqual(list(files), ["content/logo.png", "notes.txt", "source/main.lua"])
        self.assertEqual({name: path.read_text() for name, path in files.items()}, {"content/logo.png": "logo", "notes.txt": "lua", "source/main.lua": "lua"})
        self.assertEqual({name: path.read_text() for name, path in haylen.layered_files(haylen.template_layers("cpp")).items()}, {"content/logo.png": "logo", "notes.txt": "common", "src/App.cpp": "cpp"})

    def test_a_platform_project_takes_the_layers_of_the_kind_of_its_app(self):
        self.use_layers({"common/platform/web/app.js": "common", "common/platform/android/App.kt": "common", "lua/platform/web/index.html": "lua", "lua/platform/apple/project.yml": "lua", "cpp/platform/web/app.js": "cpp"})
        self.assertEqual(haylen.platform_templates("lua"), ["android", "apple", "web"])
        self.assertEqual(haylen.platform_templates("cpp"), ["android", "web"])
        self.assertEqual(haylen.platform_templates(), ["android", "apple", "web"])
        lua, cpp = self.scratch / "lua-app", self.scratch / "cpp-app"
        for app in (lua, cpp):
            app.mkdir()
            (app / "app.json").write_text("{}")
        (cpp / "CMakeLists.txt").write_text("")
        for app in (lua, cpp):
            haylen.command_platform_add(haylen.build_parser().parse_args(["platform", "add", str(app), "web"]))
        self.assertEqual({name: path.read_text() for name, path in haylen.project_files(lua / "platform" / "web").items()}, {"app.js": "common", "index.html": "lua"})
        self.assertEqual({name: path.read_text() for name, path in haylen.project_files(cpp / "platform" / "web").items()}, {"app.js": "cpp"})
        with self.assertRaisesRegex(haylen.BuildError, 'The C\\+\\+ apps have no "apple" template. Their platform templates are "android", "web".'):
            haylen.command_platform_add(haylen.build_parser().parse_args(["platform", "add", str(cpp), "apple"]))

        # An app without a project of its own builds a copy of the layers, made again whenever a layer changes.
        info = mock.Mock(folder=cpp, build_folder=self.scratch / "build")
        root = haylen.project_root(info, "android")
        self.assertEqual((root / "App.kt").read_text(), "common")
        (self.scratch / "templates" / "cpp" / "platform" / "android").mkdir(parents=True)
        (self.scratch / "templates" / "cpp" / "platform" / "android" / "App.kt").write_text("cpp")
        self.assertEqual((haylen.project_root(info, "android") / "App.kt").read_text(), "cpp")

    def test_each_kind_of_app_starts_from_its_layers_over_the_common_one(self):
        for kind in haylen.APP_KINDS:
            app = self.new_app(f"{kind}-app", "--kind", kind)
            expected = haylen.layered_files(haylen.template_layers(kind))
            self.assertEqual(sorted(haylen.project_files(app)), sorted(expected))
            self.assertEqual(haylen.app_kind(app), kind)
            self.assertTrue(all((app / name).read_bytes() == path.read_bytes() for name, path in expected.items() if name != "app.json"))
        self.assertEqual(haylen.platform_templates("lua"), ["android", "apple", "linux", "web", "windows"])
        self.assertEqual(haylen.platform_templates("cpp"), ["android", "web"])
        self.assertTrue((self.scratch / "lua-app" / "source" / "main.lua").is_file())
        self.assertFalse((self.scratch / "cpp-app" / "source").exists())
        self.assertTrue((self.scratch / "cpp-app" / "src" / "App.cpp").is_file())
        self.assertTrue((self.scratch / "cpp-app" / "platform" / "android" / "settings.gradle.kts").is_file())
        self.assertIn(f'Run it with "haylen run {self.scratch / "cpp-app"}', self.output.getvalue())
        self.assertEqual(json.loads((self.scratch / "cpp-app" / "app.json").read_text())["name"], "Cpp App")

    def test_the_commands_of_platform_projects_send_a_cpp_app_to_run(self):
        app = self.new_app("game", "--kind", "cpp")
        for arguments in (["prepare", str(app), "--platform", "web"], ["distribution", "installer", str(app), "--platform", "linux"], ["platform", "xcodegen", str(app)]):
            with self.assertRaisesRegex(haylen.BuildError, 'is a C\\+\\+ app, whose CMake project builds it for every platform. Run it with "haylen run '):
                haylen.build_parser().parse_args(arguments).handler(haylen.build_parser().parse_args(arguments))

    def test_a_cpp_app_stages_the_steam_depots_of_the_executable_its_cmake_project_builds(self):
        app = self.new_app("game", "--kind", "cpp")
        build = self.scratch / "cpp-build"
        (build / "bin" / "main" / "app").mkdir(parents=True)
        (build / "bin" / "main" / "main").write_bytes(b"executable")
        (build / "bin" / "main" / "app" / "app.hmanifest").write_bytes(b"manifest")
        (build / "haylen-release" / "main" / "packs" / "chapter-2").mkdir(parents=True)
        (build / "haylen-release" / "main" / "packs" / "chapter-2" / "shard.hpak").write_bytes(b"shard")
        released = mock.Mock(content_packs=["chapter-2"], version="1.0.0", build_folder=self.scratch / "app-build")
        released.name = "Game"
        arguments = ["distribution", "steam", str(app), "--platform", haylen.host_name(), "--app-id", "7", "--depot", f"{haylen.host_name()}=8", "--depot", "chapter-2=9"]
        parsed = haylen.build_parser().parse_args(arguments)
        executable = build / "bin" / "main" / "main"
        with mock.patch.object(haylen, "build_cpp_desktop", return_value=executable) as built, mock.patch.object(haylen, "cpp_release_app", return_value=released), mock.patch.object(haylen, "CPP_BUILDS_DIR", self.scratch), mock.patch.object(haylen, "build_folder_name", return_value=""), mock.patch.object(haylen, "apple_bundle_of", return_value=None):
            (self.scratch / f"{haylen.host_name()}-release").symlink_to(build)
            parsed.handler(parsed)
        self.assertEqual(built.call_args.args[1], "main")
        steam = released.build_folder / "steam"
        self.assertEqual((steam / "content" / haylen.host_name() / "main").read_bytes(), b"executable")
        self.assertTrue((steam / "content" / haylen.host_name() / "app" / "app.hmanifest").is_file())
        self.assertEqual((steam / "content" / "chapter-2" / "packs" / "shard.hpak").read_bytes(), b"shard", "The shards of a pack install next to the executable.")
        self.assertIn('"7"', (steam / "scripts" / "app_build.vdf").read_text())
        other = "windows" if haylen.host_name() != "windows" else "linux"
        with self.assertRaisesRegex(haylen.BuildError, "builds its Steam depots on the platform they ship to"):
            parsed = haylen.build_parser().parse_args(["distribution", "steam", str(app), "--platform", other, "--app-id", "7", "--depot", f"{other}=8"])
            parsed.handler(parsed)

    def test_no_file_of_a_kind_copies_the_common_file_it_replaces(self):
        common = haylen.project_files(haylen.COMMON_TEMPLATE)
        for kind in haylen.APP_KINDS:
            for name, path in haylen.project_files(haylen.TEMPLATES_DIR / kind).items():
                self.assertFalse(name in common and common[name].read_bytes() == path.read_bytes(), f"The file {name} of the {kind} template repeats the common one.")

    def test_platform_diff_compares_every_file_of_the_project_with_the_layered_template(self):
        app = self.new_app("game")
        handler = app / "platform" / "android" / "app" / "src" / "main" / "kotlin" / "com" / "haylen" / "app" / "App.kt"
        handler.write_text(handler.read_text() + "// A change of the developer.\n")
        (app / "platform" / "android" / "haylen").mkdir()
        (app / "platform" / "android" / "haylen" / "haylen.properties").write_text("generated")
        haylen.command_platform_diff(haylen.build_parser().parse_args(["platform", "diff", str(app), "android"]))
        self.assertIn("+// A change of the developer.", self.output.getvalue())
        self.assertIn('differs from the "android" template in 1 files.', self.output.getvalue())


class DesktopProjectTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.scratch)
        self.app = self.scratch / "Sky Garden"
        self.app.mkdir()
        (self.app / "app.json").write_text(json.dumps({"name": "Sky Garden", "identifier": "com.example.skygarden", "version": "1.2"}))

    def test_the_publisher_follows_the_name_and_the_executable_follows_the_folder(self):
        info = haylen.App(self.app, "linux")
        self.assertEqual((info.description, info.company, info.copyright, info.website), ("Sky Garden", "Sky Garden", "Copyright \u00a9 Sky Garden", ""))
        self.assertEqual(info.executable, "sky-garden")
        (self.app / "app.json").write_text(json.dumps({"name": "Sky Garden", "identifier": "com.example.skygarden", "version": "1.2", "company": "Harbor Games", "website": "https://harbor.example"}))
        info = haylen.App(self.app, "linux")
        self.assertEqual((info.company, info.copyright, info.website), ("Harbor Games", "Copyright \u00a9 Harbor Games", "https://harbor.example"))

    def test_a_linux_executable_takes_no_name_of_the_folders_next_to_it(self):
        for name in ("app", "Lib"):
            folder = self.scratch / name
            folder.mkdir()
            (folder / "app.json").write_text(json.dumps({"name": "Sky Garden", "identifier": "com.example.skygarden", "version": "1.2"}))
            arguments = argparse.Namespace(platform="linux", config="Release", engine_config="Release", jobs=1)
            with mock.patch.object(haylen, "host_name", return_value="linux"), self.assertRaisesRegex(haylen.BuildError, f'gives the executable "{name.lower()}", which is the name of a folder next to the executable of a Linux app'):
                haylen.prepare_desktop(haylen.App(folder, "linux"), arguments)

    def test_the_desktop_platforms_build_the_projects_of_their_templates(self):
        self.assertEqual({name: target.template for name, target in haylen.RUN_TARGETS.items() if name in ("windows", "linux")}, {"windows": "windows", "linux": "linux"})
        self.assertTrue({"windows", "linux"} <= set(haylen.platform_templates("lua")))

    def test_the_installed_app_stays_apart_from_the_copy_of_the_template(self):
        info = haylen.App(self.app, "linux")
        for platform in ("windows", "linux"):
            self.assertFalse(haylen.desktop_folder(info, platform).is_relative_to(info.build_folder / platform))

    def test_a_project_that_replaces_the_copy_of_the_template_builds_in_a_new_tree(self):
        info = mock.Mock(build_folder=self.scratch / "build")
        build = self.scratch / "build" / "cmake" / "linux-debug"
        build.mkdir(parents=True)
        (build / "CMakeCache.txt").write_text(f"CMAKE_HOME_DIRECTORY:INTERNAL={(self.scratch / 'build' / 'linux').as_posix()}\n")
        (build / "stale.o").write_bytes(b"object")
        arguments = haylen.argparse.Namespace(platform="linux", config="Debug", jobs=1)
        with mock.patch.object(haylen, "run"), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            haylen.build_desktop(info, self.scratch / "build" / "linux", arguments)
            self.assertTrue((build / "stale.o").exists())
            haylen.build_desktop(info, self.app / "platform" / "linux", arguments)
        self.assertFalse(build.exists())

    def test_values_reach_cmake_and_resource_scripts_as_they_are(self):
        self.assertEqual(haylen.cmake_text("Sky Garden"), "[[Sky Garden]]")
        self.assertEqual(haylen.cmake_text('A "quoted" ${name}; with ]] inside'), '[=[A "quoted" ${name}; with ]] inside]=]')
        self.assertEqual(haylen.rc_text('The "Garden" \\ Sky'), '"The ""Garden"" \\\\ Sky"')
        cmake = haylen.desktop_cmake("windows", "Release", {"HAYLEN_APP_NAME": "Sky Garden"})
        self.assertIn("set(HAYLEN_APP_NAME [[Sky Garden]])", cmake)
        self.assertIn("MSVC_RUNTIME_LIBRARY MultiThreadedDLL", cmake)
        self.assertIn("MSVC_RUNTIME_LIBRARY MultiThreadedDebugDLL", haylen.desktop_cmake("windows", "Debug", {}))
        self.assertIn('INSTALL_RPATH "$ORIGIN;$ORIGIN/lib"', haylen.desktop_cmake("linux", "Release", {}))

    def test_the_content_reports_reach_the_terminal(self):
        # What verify, inspect and diff find is what the developer reads, so it prints instead of staying in the log of the command.
        arguments = haylen.argparse.Namespace(app=str(self.app), platform="linux", engine_config="Release", jobs=1, chunks=False, earlier=str(self.scratch / "a"), later=str(self.scratch / "b"))
        for command in (haylen.command_content_verify, haylen.command_content_inspect, haylen.command_content_diff):
            with mock.patch.object(haylen, "content_tool", return_value=Path("haylen-content")), mock.patch.object(haylen, "ensure_keys", return_value=Path("keys")), mock.patch.object(haylen, "content_release", return_value=Path("release")), mock.patch.object(haylen, "run") as run:
                command(arguments)
            self.assertTrue(run.call_args.kwargs.get("stream"), command.__name__)

    def test_r8_keeps_every_java_method_that_the_engine_calls(self):
        # The engine resolves its Java methods by name when the library loads, so a method that R8 removes from a minified app stops the app before it starts.
        bridge = (haylen.ENGINE_DIR / "src" / "platform" / "android" / "JavaBridge.cpp").read_text(encoding="utf-8")
        rules = (haylen.ENGINE_DIR / "platform" / "android" / "haylen" / "consumer-rules.pro").read_text(encoding="utf-8")
        classes = dict(re.findall(r'(\w+Class) = findClass\(\*env, "dev/haylen/(\w+)"\)', bridge))
        types = {"V": "void", "Z": "boolean", "B": "byte", "I": "int", "J": "long", "F": "float", "D": "double"}

        def java(signature: str) -> list[str]:
            found, index = [], 0
            while index < len(signature):
                dimensions = len(signature[index:]) - len(signature[index:].lstrip("["))
                index += dimensions
                found.append(types[signature[index]] + "[]" * dimensions)
                index += 1
            return found

        methods = re.findall(r'findMethod\(\*env, (\w+Class), "(\w+)", "\(([^)]*)\)(.+?)"\)', bridge)
        self.assertGreater(len(methods), 20)
        for owner, name, arguments, result in methods:
            block = re.search(r"-keep class dev\.haylen\." + classes[owner] + r" \{(.*?)\}", rules, re.S)
            self.assertIsNotNone(block, f"The rules of R8 keep no class dev.haylen.{classes[owner]}.")
            self.assertIn(f"static {java(result)[0]} {name}({', '.join(java(arguments))});", block.group(1), f"The rules of R8 keep no method {name} of dev.haylen.{classes[owner]}.")

    def test_release_binaries_keep_their_symbols_apart(self):
        linux = haylen.desktop_cmake("linux", "Release", {})
        self.assertIn('"$<$<CONFIG:Release>:LINKER:--gc-sections>"', linux)
        self.assertIn("--only-keep-debug", linux)
        self.assertIn('"--add-gnu-debuglink=$<TARGET_FILE:${target}>.debug"', linux)
        windows = haylen.desktop_cmake("windows", "Release", {})
        for option in ("/DEBUG", "/OPT:REF", "/OPT:ICF"):
            self.assertIn(f'"$<$<CONFIG:Release>:{option}>"', windows)
        generated = self.scratch / "generated"
        haylen.write_apple_xcconfig(haylen.App(self.app, "ios"), generated, [])
        xcconfig = (generated / "Haylen.xcconfig").read_text()
        for setting in ("DEBUG_INFORMATION_FORMAT[config=Release] = dwarf-with-dsym", "DEPLOYMENT_POSTPROCESSING[config=Release] = YES", "STRIP_STYLE[config=Release] = all"):
            self.assertIn(setting, xcconfig)

    def test_apple_targets_build_only_the_architectures_of_the_engine_slices(self):
        generated = self.scratch / "generated"
        haylen.write_apple_xcconfig(haylen.App(self.app, "visionos"), generated, [])
        xcconfig = (generated / "Haylen.xcconfig").read_text()
        for sdk, architectures in (("iphoneos", "arm64"), ("iphonesimulator", "arm64 x86_64"), ("appletvos", "arm64"), ("appletvsimulator", "arm64 x86_64"), ("xros", "arm64"), ("xrsimulator", "arm64")):
            self.assertIn(f"ARCHS[sdk={sdk}*] = {architectures}\n", xcconfig)
        self.assertIn("HAYLEN_ENGINE_ARCHS_MACCATALYST_NO = arm64 x86_64\n", xcconfig)
        self.assertIn("HAYLEN_ENGINE_ARCHS_MACCATALYST_YES = arm64 x86_64\n", xcconfig)
        self.assertIn("ARCHS[sdk=macosx*] = $(HAYLEN_ENGINE_ARCHS_MACCATALYST_$(IS_MACCATALYST))\n", xcconfig)

    def test_a_release_site_minifies_its_scripts_and_leaves_the_runtimes(self):
        emsdk = self.scratch / "emsdk"
        node = emsdk / "node" / "24.0.0_64bit" / "bin" / ("node.exe" if haylen.host_name() == "windows" else "node")
        node.parent.mkdir(parents=True)
        node.write_text("")
        site = self.scratch / "site"
        for name in ("loader.js", "app.js", "haylen.js", "haylen-content-store.js", "plugins/banner/banner.js"):
            (site / name).parent.mkdir(parents=True, exist_ok=True)
            (site / name).write_text("const value = 1;\n")
        with mock.patch.object(haylen, "emsdk_root", return_value=emsdk), mock.patch.object(haylen, "run") as run:
            haylen.minify_web_scripts(site)
        commands = {Path(call.args[0][2]).relative_to(site).as_posix(): call.args[0] for call in run.call_args_list}
        self.assertEqual(sorted(commands), ["app.js", "haylen-content-store.js", "loader.js", "plugins/banner/banner.js"])
        self.assertIn("--module", commands["plugins/banner/banner.js"])
        self.assertNotIn("--module", commands["loader.js"])
        self.assertEqual(commands["app.js"][0], node)

    def test_the_version_resource_takes_the_values_of_app_json(self):
        generated = self.scratch / "generated"
        haylen.write_windows_resources(haylen.App(self.app, "windows"), generated)
        header = (generated / "HaylenApp.h").read_text()
        self.assertIn('#define HAYLEN_APP_EXECUTABLE "sky-garden.exe"', header)
        self.assertIn('#define HAYLEN_APP_COPYRIGHT "Copyright \u00a9 Sky Garden"', header)
        self.assertIn("#define HAYLEN_APP_VERSION_NUMBERS 1, 2, 0, 1", header)
        self.assertIn('#define HAYLEN_APP_VERSION "1.2"', header)

        # The fourth number is the build number, and the resource holds numbers of 16 bits alone.
        (self.app / "app.json").write_text(json.dumps({"name": "Sky Garden", "identifier": "com.example.skygarden", "version": "1.2", "build": 37}))
        haylen.write_windows_resources(haylen.App(self.app, "windows"), generated)
        self.assertIn("#define HAYLEN_APP_VERSION_NUMBERS 1, 2, 0, 37", (generated / "HaylenApp.h").read_text())
        (self.app / "app.json").write_text(json.dumps({"name": "Sky Garden", "identifier": "com.example.skygarden", "version": "1.2", "build": 65536}))
        with self.assertRaisesRegex(haylen.BuildError, 'The version resource of Windows holds numbers up to 65535, and the "version" "1.2" or the "build" "65536" of'):
            haylen.write_windows_resources(haylen.App(self.app, "windows"), generated)

    def test_development_builds_stop_a_release_of_the_development_package(self):
        self.assertIn("#if defined(NDEBUG)\n#error", haylen.development_bootstrap("defined(NDEBUG)"))
        self.assertIn("#if !DEBUG\n#error", haylen.development_bootstrap("!DEBUG"))

    def test_the_desktop_entry_of_the_developer_wins_and_gains_what_it_lacks(self):
        warnings: list[str] = []
        own = "# A comment.\n[Desktop Entry]\nType=Application\nName=Garden\n\n[Desktop Action New]\nName=New\n"
        completed = haylen.complete_desktop_entry(own, {"Name": "Sky Garden", "Comment": "Tab\tand\nline", "Exec": "sky-garden"}, "app.desktop", warnings)
        self.assertEqual(completed, "# Written by haylen.py from app.json and the desktop entry of the project.\n# A comment.\n[Desktop Entry]\nType=Application\nName=Garden\nComment=Tab\\tand\\nline\nExec=sky-garden\n\n[Desktop Action New]\nName=New\n")
        self.assertEqual(warnings, ['The key "Name" of `app.desktop` keeps its value "Garden", while "app.json" gives "Sky Garden".'])
        with self.assertRaisesRegex(haylen.BuildError, 'has no "\\[Desktop Entry\\]" group'):
            haylen.complete_desktop_entry("[Desktop Action New]\n", {}, "app.desktop", [])

    def test_the_metainfo_of_the_developer_wins_and_gains_the_elements_of_app_json(self):
        info = haylen.App(self.app, "linux")
        warnings: list[str] = []
        own = '<?xml version="1.0"?>\n<component type="desktop-application"><name>Garden</name><project_license>MIT</project_license></component>'
        completed = haylen.complete_metainfo(own, haylen.appstream_elements(info), "app.metainfo.xml", warnings)
        component = haylen.xml.etree.ElementTree.fromstring(completed.split("\n", 2)[2])
        self.assertEqual([child.tag for child in component][:5], ["id", "summary", "description", "developer", "launchable"])
        self.assertEqual(component.findtext("name"), "Garden")
        self.assertEqual(component.find("developer").get("id"), "com.example")
        self.assertIsNone(component.find("url"))
        self.assertEqual(component.find("launchable").text, "com.example.skygarden.desktop")
        self.assertEqual(warnings, ['The element "name" of `app.metainfo.xml` keeps its value "Garden", while "app.json" gives "Sky Garden".'])
        with self.assertRaisesRegex(haylen.BuildError, "is not valid XML"):
            haylen.complete_metainfo("<component>", [], "app.metainfo.xml", [])

    @staticmethod
    def executable(manifest: bytes) -> bytes:
        """Builds the smallest PE file whose resource section holds one application manifest."""
        resources = bytearray(0x58)
        resources[14:16] = (1).to_bytes(2, "little")
        resources[16:24] = (24).to_bytes(4, "little") + (0x80000000 | 0x18).to_bytes(4, "little")
        resources[0x18 + 14 : 0x18 + 16] = (1).to_bytes(2, "little")
        resources[0x18 + 16 : 0x18 + 24] = (1).to_bytes(4, "little") + (0x80000000 | 0x30).to_bytes(4, "little")
        resources[0x30 + 14 : 0x30 + 16] = (1).to_bytes(2, "little")
        resources[0x30 + 16 : 0x30 + 24] = (0x409).to_bytes(4, "little") + (0x48).to_bytes(4, "little")
        resources[0x48:0x50] = (0x1058).to_bytes(4, "little") + len(manifest).to_bytes(4, "little")
        resources += manifest
        header = bytearray(0x200)
        header[0:2] = b"MZ"
        header[0x3C:0x40] = (0x40).to_bytes(4, "little")
        header[0x40:0x44] = b"PE\0\0"
        header[0x46:0x48] = (1).to_bytes(2, "little")
        header[0x54:0x56] = (240).to_bytes(2, "little")
        header[0x58:0x5A] = (0x20B).to_bytes(2, "little")
        header[0x58 + 128 : 0x58 + 136] = (0x1000).to_bytes(4, "little") + len(resources).to_bytes(4, "little")
        section = 0x58 + 240
        header[section : section + 8] = b".rsrc\0\0\0"
        header[section + 8 : section + 24] = len(resources).to_bytes(4, "little") + (0x1000).to_bytes(4, "little") + len(resources).to_bytes(4, "little") + (0x200).to_bytes(4, "little")
        return bytes(header + resources)

    def test_the_manifest_of_a_windows_executable_selects_what_the_engine_needs(self):
        manifest = haylen.layered_files(haylen.platform_layers("lua", "windows"))["resources/app.manifest"].read_bytes()
        self.assertEqual(haylen.windows_resources(self.executable(manifest), haylen.WINDOWS_MANIFEST_RESOURCE), [manifest])
        self.assertEqual(haylen.windows_resources(b"not an executable", haylen.WINDOWS_MANIFEST_RESOURCE), [])

        project = self.scratch / "project"
        (project / "haylen" / "native").mkdir(parents=True)
        (project / "haylen" / "native" / "glue.dll").write_bytes(b"glue")
        app = mock.Mock(folder=self.app, build_folder=self.scratch / "build", executable="sky-garden", identifier="com.example.skygarden")
        folder = self.scratch / "build" / "installed" / "windows"
        (folder / "app").mkdir(parents=True)
        (folder / "app" / "app.json").write_text("{}")
        (folder / "glue.dll").write_bytes(b"glue")
        arguments = haylen.argparse.Namespace(platform="windows", config="Debug")
        (folder / "sky-garden.exe").write_bytes(self.executable(manifest))
        self.assertEqual(haylen.check_desktop(app, project, arguments), [])

        (folder / "sky-garden.exe").write_bytes(self.executable(b'<assembly><application><windowsSettings><activeCodePage>Legacy</activeCodePage></windowsSettings></application></assembly>'))
        (folder / "glue.dll").unlink()
        messages = [requirement.message for requirement in haylen.check_desktop(app, project, arguments)]
        self.assertEqual(messages, [
            'The app needs the native library "glue.dll" next to its executable, which the built app lacks.',
            "The application manifest of the built executable lacks the Common Controls of version 6, which give the task dialogs and the controls that native libraries show the current look of Windows.",
            "The application manifest of the built executable lacks UTF-8 as the code page of the process, which the text of Haylen needs.",
        ])

    @unittest.skipIf(os.name == "nt", "The Linux layout links the executable into its bin folder.")
    def test_a_linux_app_needs_its_desktop_entry_named_after_the_identifier(self):
        project = self.scratch / "project"
        app = mock.Mock(folder=self.app, build_folder=self.scratch / "build", executable="sky-garden", identifier="com.example.skygarden")
        folder = self.scratch / "build" / "installed" / "linux"
        (folder / "lib" / "sky-garden" / "app").mkdir(parents=True)
        (folder / "lib" / "sky-garden" / "app" / "app.json").write_text("{}")
        (folder / "lib" / "sky-garden" / "sky-garden").write_bytes(b"code")
        (folder / "bin").mkdir()
        (folder / "bin" / "sky-garden").symlink_to("../lib/sky-garden/sky-garden")
        arguments = haylen.argparse.Namespace(platform="linux", config="Debug")
        self.assertEqual([requirement.message for requirement in haylen.check_desktop(app, project, arguments)], ['Haylen needs the desktop entry "share/applications/com.example.skygarden.desktop" in the built app, which it lacks, since Linux desktops find the name and the icon of the window of the app by the identifier.'])
        (folder / "share" / "applications").mkdir(parents=True)
        (folder / "share" / "applications" / "com.example.skygarden.desktop").write_text("[Desktop Entry]\n")
        self.assertEqual(haylen.check_desktop(app, project, arguments), [])


class ContentKeysTest(unittest.TestCase):
    def test_keys_live_outside_every_project_unless_continuous_integration_names_their_folder(self):
        environment = {name: value for name, value in os.environ.items() if name != haylen.KEYS_VARIABLE}
        with mock.patch.dict(os.environ, environment, clear=True):
            folder = haylen.keys_folder("com.example.game")
            self.assertEqual(folder, haylen.user_config_folder() / "keys" / "com.example.game")
            self.assertFalse(folder.is_relative_to(haylen.ROOT))
        with mock.patch.dict(os.environ, {haylen.KEYS_VARIABLE: "/secrets/keys"}):
            self.assertEqual(haylen.keys_folder("com.example.game"), Path("/secrets/keys/com.example.game"))

    def test_continuous_integration_receives_keys_and_never_creates_them(self):
        with tempfile.TemporaryDirectory() as scratch, mock.patch.dict(os.environ, {haylen.KEYS_VARIABLE: scratch}):
            app = mock.Mock(identifier="com.example.game")
            with self.assertRaisesRegex(haylen.BuildError, 'holds no keys of "com.example.game"'):
                haylen.ensure_keys(app, Path("haylen-content"))
            (Path(scratch) / "com.example.game").mkdir()
            (Path(scratch) / "com.example.game" / "keys.json").write_text("{}")
            self.assertEqual(haylen.ensure_keys(app, Path("haylen-content")), Path(scratch) / "com.example.game")

    def test_every_platform_with_a_project_has_a_content_profile(self):
        self.assertEqual(set(haylen.CONTENT_PROFILES), set(haylen.RUN_TARGETS))
        self.assertEqual({haylen.CONTENT_PROFILES[name] for name in haylen.APPLE_RUNS}, {"apple"})


class ReleaseInspectionTest(unittest.TestCase):
    def setUp(self):
        self.app = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.app)
        (self.app / "app.json").write_text(json.dumps({"name": "Game", "identifier": "com.example.game", "version": "1.0.0", "splash": {"logo": "images/logo.png"}}))
        (self.app / "source").mkdir()
        (self.app / "source" / "main.lua").write_text("print('the main module of the game')")
        (self.app / "content" / "images").mkdir(parents=True)
        (self.app / "content" / "images" / "hero.png").write_bytes(b"a hero image that only the package holds")
        (self.app / "content" / "images" / "logo.png").write_bytes(b"the splash logo that platforms show")
        self.info = haylen.App(self.app, "linux")
        self.secrets = {'the content key "1234"': bytes(range(32))}

    @staticmethod
    def entry(path: str, data: bytes) -> tuple[str, int, object]:
        return (path, len(data), lambda: data)

    def problems(self, *entries: tuple[str, int, object]) -> list[str]:
        return haylen.release_problems(self.info, list(entries), "app", self.secrets)

    def test_a_protected_release_passes(self):
        logo = (self.app / "content" / "images" / "logo.png").read_bytes()
        self.assertEqual(self.problems(self.entry("app/app.hmanifest", b"manifest"), self.entry("app/content.hmanifest", b"manifest"), self.entry("app/" + "ab" * 32 + ".hpak", b"\x00" * 64), self.entry("game", b"code of the app"), self.entry("res/splash.png", logo)), [])

    def test_raw_files_lua_text_indexes_and_symbols_fail(self):
        hero = (self.app / "content" / "images" / "hero.png").read_bytes()
        problems = self.problems(self.entry("app/app.hmanifest", b"manifest"), self.entry("app/source/main.lua", b"print('x')"), self.entry("assets/renamed.bin", hero), self.entry("app/haylen-package-index.json", b"[]"), self.entry("game.pdb", b"symbols of the game"))
        self.assertIn('The release holds the Lua text "app/source/main.lua".', problems)
        self.assertIn('The release holds "app/source/main.lua", while its folder "app" holds only manifests and shards.', problems)
        self.assertIn('The release holds "assets/renamed.bin", which is the file "content/images/hero.png" of the package as it is.', problems)
        self.assertIn('The release holds the index of a development package, "app/haylen-package-index.json".', problems)
        self.assertIn('The release holds the symbols "game.pdb", which stay with the developer.', problems)

    def test_keys_in_any_form_and_a_missing_release_fail(self):
        key = bytes(range(32))
        for form in (key, key.hex().encode(), key.hex().upper().encode(), haylen.base64.b64encode(key)):
            problems = self.problems(self.entry("game", b"code " + form + b" code"))
            self.assertIn('The release holds the content key "1234" of the app in "game".', problems)
            self.assertIn('The release has no protected release in "app".', problems)

    def test_a_release_larger_than_its_store_accepts_warns(self):
        release = self.app / "release"
        release.mkdir()
        (release / "app.hmanifest").write_bytes(b"manifest")
        errors = io.StringIO()
        with mock.patch.dict(haylen.STORE_RELEASE_LIMITS, {"apple": (4, "the limit of the test")}), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), errors, {})):
            haylen.warn_store_size(release, "apple")
            haylen.warn_store_size(release, "linux")
        self.assertEqual(errors.getvalue().count("more than the limit of the test"), 1)

    def test_a_run_stops_on_what_a_release_never_ships_and_goes_on_with_other_requirements(self):
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            with self.assertRaisesRegex(haylen.BuildError, "holds the 1 files above that a release never ships"):
                haylen.stop_on_release_problems([haylen.Requirement('The release holds the Lua text "app/main.lua".', haylen.RELEASE_ADVICE), haylen.Requirement("A plugin needs a key.", "Add it.")])
            haylen.stop_on_release_problems([haylen.Requirement("A plugin needs a key.", "Add it.")])


class AndroidPluginsTest(unittest.TestCase):
    def test_the_plugins_file_lists_the_android_plugins_in_load_order_with_their_values(self):
        assets = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, assets)
        accounts = haylen.Plugin(assets, {"id": "accounts", "version": "2.0.0", "android": {"module": "android"}}, None)
        sounds = haylen.Plugin(assets, {"id": "sounds", "version": "1.0.0", "requires": ["accounts"]}, None)
        store = haylen.Plugin(assets, {"id": "store", "version": "1.1.0", "requires": ["accounts"], "android": {"module": "android"}}, None)
        app = mock.Mock(plugins=[accounts, sounds, store], plugin_values={"accounts": {"server": "accounts.example.com"}, "sounds": {}, "store": {"sandbox": True}})
        haylen.write_android_plugins(app, assets)
        written = json.loads((assets / haylen.ANDROID_PLUGINS_FILE).read_text())
        self.assertEqual(written, {"plugins": [{"id": "accounts", "version": "2.0.0", "parameters": {"server": "accounts.example.com"}}, {"id": "store", "version": "1.1.0", "parameters": {"sandbox": True}}]})


    def test_a_plugin_that_delivers_packs_turns_the_named_packs_into_asset_packs_of_google_play(self):
        scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, scratch)
        packs = scratch / "release.packs"
        for pack, shards in (("chapter-2", 2), ("intro.music", 1), ("2nd", 1)):
            for index in range(shards):
                (packs / pack).mkdir(parents=True, exist_ok=True)
                (packs / pack / f"{pack}-{index}.hpak").write_bytes(b"s" * 10)
        generated = scratch / "project" / haylen.GENERATED_FOLDER
        (generated / "asset-packs" / "haylencontent1").mkdir(parents=True)
        (generated / "asset-packs" / "haylencontent1" / "build.gradle.kts").write_text(haylen.ANDROID_ASSET_PACK_MODULE.format(name="haylencontent1", delivery="install-time", words="with the app"))
        delivery = haylen.Plugin(scratch, {"id": "delivery", "version": "1.0.0", "android": {"module": "android", "assetPacks": {"fastFollow": "${fastFollowPacks}"}}}, None)
        app = mock.Mock(plugins=[delivery], plugin_values={"delivery": {"fastFollowPacks": ["intro.music"]}}, content_packs=["2nd", "chapter-2", "intro.music"])
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            names = haylen.write_android_store_packs(app, generated, packs)
        self.assertEqual(names, ["pack_2nd", "chapter_2", "intro_music"])
        self.assertIn('deliveryType.set("fast-follow")', (generated / "asset-packs" / "intro_music" / "build.gradle.kts").read_text())
        self.assertIn('deliveryType.set("on-demand")', (generated / "asset-packs" / "chapter_2" / "build.gradle.kts").read_text())
        self.assertEqual(haylen.android_store_packs(scratch / "project"), {"pack_2nd": {"2nd-0.hpak"}, "chapter_2": {"chapter-2-0.hpak", "chapter-2-1.hpak"}, "intro_music": {"intro.music-0.hpak"}}, "The install-time packs are not among them, and the shards sit at the root of the assets of each pack.")

        without = mock.Mock(plugins=[], plugin_values={}, content_packs=app.content_packs)
        self.assertEqual(haylen.write_android_store_packs(without, scratch / "other", packs), [], "Without a plugin that delivers them, the packs stay for the content server.")
        app.plugin_values["delivery"]["fastFollowPacks"] = ["chapter-3"]
        with self.assertRaisesRegex(haylen.BuildError, 'the packs "chapter-3", which "content.delivery"'):
            haylen.write_android_store_packs(app, scratch / "unknown", packs)
        app.plugin_values["delivery"]["fastFollowPacks"] = []
        with mock.patch.object(haylen, "ANDROID_STORE_PACK_LIMIT", 15), self.assertRaisesRegex(haylen.BuildError, 'The pack "chapter-2" holds 20 bytes'):
            haylen.write_android_store_packs(app, scratch / "large", packs)
        (packs / "chapter.2").mkdir()
        with self.assertRaisesRegex(haylen.BuildError, 'The packs "chapter-2" and "chapter.2" both become the asset pack "chapter_2"'):
            haylen.write_android_store_packs(app, scratch / "collision", packs)
        shutil.rmtree(packs / "chapter.2")
        (packs / "haylencontent1").mkdir()
        app.content_packs.append("haylencontent1")
        with self.assertRaisesRegex(haylen.BuildError, 'which the release already takes for its content shards'):
            haylen.write_android_store_packs(app, generated, packs)

    def test_the_asset_packs_of_a_plugin_list_the_packs_that_deliver_right_after_the_install(self):
        root = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, root)
        scratch = root / "delivery"
        (scratch / "android" / "src" / "main").mkdir(parents=True)
        (scratch / "android" / "build.gradle.kts").write_text("")
        (scratch / "android" / "src" / "main" / "AndroidManifest.xml").write_text("<manifest />")
        manifest = {"id": "delivery", "name": "Delivery", "version": "1.0.0", "description": "Delivers packs.", "platforms": ["android"], "parameters": {"fastFollowPacks": {"type": "array", "default": [], "description": "Packs."}}, "android": {"module": "android", "assetPacks": {"fastFollow": "${fastFollowPacks}"}}}
        self.assertEqual(haylen.PluginManifestCheck.problems_of(scratch, manifest), [])
        manifest["android"]["assetPacks"] = {"fastFollow": "music", "delivery": "on-demand"}
        self.assertEqual(haylen.PluginManifestCheck.problems_of(scratch, manifest), ['The key "android.assetPacks.delivery" is not a key of "plugin.json".', 'The key "android.assetPacks.fastFollow" must list the packs that Google Play delivers right after the install, or name an array parameter that lists them.'])

    def test_the_maven_repositories_of_plugins_reach_the_android_project_once_and_in_order(self):
        root = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, root)
        plugins = []
        for identifier, repositories in (("ads", ["https://maven.ads.example/releases", "https://maven.shared.example"]), ("maps", ["https://maven.shared.example"])):
            folder = root / identifier
            (folder / "android" / "src" / "main").mkdir(parents=True)
            (folder / "android" / "build.gradle.kts").write_text("")
            (folder / "android" / "src" / "main" / "AndroidManifest.xml").write_text("<manifest />")
            manifest = {"id": identifier, "name": identifier, "version": "1.0.0", "description": "An SDK.", "platforms": ["android"], "android": {"module": "android", "repositories": repositories}}
            self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, manifest), [])
            plugins.append(haylen.Plugin(folder, manifest, None))
        app = mock.Mock(plugins=plugins, plugin_values={"ads": {}, "maps": {}})
        properties = haylen.prepare_android_plugins(app, root / "project")
        self.assertEqual(properties["repositories"], "https://maven.ads.example/releases,https://maven.shared.example")

        manifest["android"]["repositories"] = ["http://maven.example", "https://a,b"]
        self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, manifest), ['The key "android.repositories" must list the https addresses of the Maven repositories that the module and its Gradle plugins come from, such as "https://maven.example.com/releases".'])
        for name in ("settings.gradle.kts", "build.gradle.kts"):
            self.assertIn('haylen.getProperty("repositories")', (haylen.ROOT / "templates" / "common" / "platform" / "android" / name).read_text(encoding="utf-8"), f'The file "{name}" of the template reads the repositories of the plugins.')

    def test_bundletool_signs_with_the_upload_key_that_the_gradle_scripts_read(self):
        scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, scratch)
        self.assertEqual(haylen.read_java_properties(f"# A comment.\nkeyPassword={haylen.java_property('pä\\ss😀 ')}\n"), {"keyPassword": "pä\\ss😀 "}, "A value keeps its trailing spaces, as Java reads it.")
        variables = {"HAYLEN_RELEASE_STORE_FILE": "keys/upload.jks", "HAYLEN_RELEASE_STORE_PASSWORD": "store", "HAYLEN_RELEASE_KEY_ALIAS": "upload", "HAYLEN_RELEASE_KEY_PASSWORD": "key"}
        with mock.patch.dict(os.environ, variables):
            self.assertEqual(haylen.android_release_key(scratch), {"storeFile": str(scratch / "keys" / "upload.jks"), "storePassword": "store", "keyAlias": "upload", "keyPassword": "key"})
        files = haylen.android_key_files(scratch, "release")
        files["properties"].parent.mkdir()
        files["properties"].write_text(haylen.android_key_properties("release", "upload", "secret"))
        self.assertEqual(haylen.android_release_key(scratch), {"storeFile": str(files["keystore"]), "storePassword": "secret", "keyAlias": "upload", "keyPassword": "secret"})
        files["properties"].unlink()
        home = scratch / "gradle-home"
        home.mkdir()
        (scratch / "gradle.properties").write_text("haylen.release.storeFile=keys/project.jks\nhaylen.release.storePassword=project\nhaylen.release.keyAlias=upload\nhaylen.release.keyPassword=project\n")
        (home / "gradle.properties").write_text("haylen.release.storePassword=home\n")
        with mock.patch.dict(os.environ, {**{name: "" for name in variables}, "GRADLE_USER_HOME": str(home)}):
            self.assertEqual(haylen.android_release_key(scratch), {"storeFile": str(scratch / "keys" / "project.jks"), "storePassword": "home", "keyAlias": "upload", "keyPassword": "project"}, "The Gradle properties give the key, those of the Gradle home first.")
        (scratch / "gradle.properties").unlink()
        with mock.patch.dict(os.environ, {**{name: "" for name in variables}, "GRADLE_USER_HOME": str(scratch / "empty")}), self.assertRaisesRegex(haylen.BuildError, "needs the upload key of the app"):
            haylen.android_release_key(scratch)

    def test_an_app_extension_of_a_plugin_builds_with_the_identity_of_the_app(self):
        scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, scratch)
        extension = {"name": "Downloads", "type": "extensionKit", "point": "com.apple.background-asset-downloader-extension", "sources": "apple-extension", "minimumVersion": "26.0", "frameworks": ["BackgroundAssets.framework"], "entitlements": {"com.apple.security.application-groups": ["group.$(HAYLEN_BUNDLE_IDENTIFIER)"]}}
        target = haylen.apple_extension_target(mock.Mock(), scratch, "DownloadsTVOS", "assets", "HaylenTVOS", extension)
        self.assertEqual(target["type"], "extensionkit-extension")
        self.assertEqual(target["platform"], "tvOS")
        self.assertEqual(target["deploymentTarget"], "26.0")
        self.assertEqual(target["sources"], [{"path": "plugins/assets/extensions/Downloads", "name": "Downloads", "group": "plugins"}])
        self.assertEqual(target["dependencies"], [{"sdk": "BackgroundAssets.framework"}])
        self.assertEqual(target["settings"]["base"]["PRODUCT_BUNDLE_IDENTIFIER"], "$(HAYLEN_BUNDLE_IDENTIFIER).downloads")
        info = plistlib.loads((scratch / "extensions" / "DownloadsTVOS" / "Info.plist").read_bytes())
        self.assertEqual(info["EXAppExtensionAttributes"], {"EXExtensionPointIdentifier": "com.apple.background-asset-downloader-extension"})
        self.assertEqual((info["CFBundleShortVersionString"], info["CFBundleVersion"]), ("$(MARKETING_VERSION)", "$(CURRENT_PROJECT_VERSION)"), "The extension takes the version of the app it is embedded in.")
        self.assertEqual(plistlib.loads((scratch / "extensions" / "DownloadsTVOS" / "DownloadsTVOS.entitlements").read_bytes()), extension["entitlements"])

    def test_the_extensions_and_asset_packs_of_an_apple_part_are_checked(self):
        root = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, root)
        folder = root / "assets"
        (folder / "apple").mkdir(parents=True)
        (folder / "apple-extension").mkdir()
        extension = {"name": "Downloads", "type": "extensionKit", "point": "com.apple.background-asset-downloader-extension", "sources": "apple-extension", "minimumVersion": "26.0"}
        manifest = {"id": "assets", "name": "Assets", "version": "1.0.0", "description": "Delivers packs.", "platforms": ["ios"], "parameters": {"prefetchPacks": {"type": "array", "default": [], "description": "Packs."}}, "apple": {"class": "AssetsPlugin", "sources": "apple", "assetPacks": {"prefetch": "${prefetchPacks}"}, "extensions": [extension]}}
        self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, manifest), [])
        manifest["apple"]["extensions"] = [{**extension, "name": "downloads", "type": "widget", "platforms": ["tvos"], "minimumVersion": "latest"}, {"name": "Other"}]
        manifest["apple"]["assetPacks"] = {"prefetch": 3}
        self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, manifest), [
            'The key "apple.extensions[0].name" must be a name of letters and digits that starts with a capital letter, such as "Downloads".',
            'The key "apple.extensions[0].type" must be "extensionKit", "appExtension".',
            'The key "apple.extensions[0].platforms" must list the platforms of the plugin that the extension builds for, among "ios".',
            'The key "apple.extensions[0].minimumVersion" must be the oldest system version that the extension runs on, such as "26.0".',
            'The key "apple.extensions[1].point" is missing.',
            'The key "apple.extensions[1].sources" is missing.',
            'The key "apple.extensions[1].type" is missing.',
            'The key "apple.assetPacks.prefetch" must list the packs that the system downloads with the install, or name an array parameter that lists them.',
        ])

    def test_a_plugin_that_delivers_packs_from_the_app_store_packs_each_named_pack_with_its_download_policy(self):
        scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, scratch)
        for pack in ("chapter-2", "intro.music"):
            (scratch / "release" / "apple.packs" / pack).mkdir(parents=True)
            (scratch / "release" / "apple.packs" / pack / f"{pack}.hpak").write_bytes(b"s")
        assets = haylen.Plugin(scratch, {"id": "assets", "version": "1.0.0", "platforms": ["ios", "tvos", "visionos"], "apple": {"assetPacks": {"prefetch": "${prefetchPacks}"}}}, None)
        app = mock.Mock(plugins=[assets], plugin_values={"assets": {"prefetchPacks": ["intro.music"]}}, content_packs=["chapter-2", "intro.music"], build_folder=scratch)
        manifests = {}
        def package(command, **_):
            manifests[Path(command[3]).stem] = json.loads(Path(command[3]).read_text())
            Path(command[-1]).write_bytes(b"archive")
        with mock.patch.object(haylen, "run", side_effect=package), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            archives = haylen.write_apple_asset_packs(app, scratch / "release" / "apple.packs")
        self.assertEqual([path.name for path in archives], ["chapter-2.aar", "intro.music.aar"])
        self.assertEqual(manifests["chapter-2"]["downloadPolicy"], {"onDemand": {}})
        self.assertEqual(manifests["intro.music"]["downloadPolicy"], {"prefetch": {"installationEventTypes": ["firstInstallation", "subsequentUpdate"]}})
        self.assertEqual(manifests["intro.music"]["fileSelectors"], [{"directorySource": "intro.music", "directoryDestination": "intro.music"}])
        self.assertEqual(manifests["intro.music"]["platforms"], ["iOS", "tvOS", "visionOS"])
        self.assertEqual((haylen.apple_asset_packs_folder(app) / manifests["intro.music"]["sourceRoot"]).resolve(), scratch / "release" / "apple.packs")
        self.assertEqual(sorted(path.name for path in haylen.apple_asset_packs_folder(app).iterdir()), ["chapter-2.aar", "intro.music.aar"], "The manifests of the tool leave once the archives exist.")
        app.plugin_values["assets"]["prefetchPacks"] = ["chapter-3"]
        with self.assertRaisesRegex(haylen.BuildError, 'the packs "chapter-3", which "content.delivery"'):
            haylen.write_apple_asset_packs(app, scratch / "release" / "apple.packs")

    def test_an_android_project_without_the_files_of_its_plugins_stops_before_gradle(self):
        scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, scratch)
        (scratch / "cloud").mkdir()
        (scratch / "cloud" / "services.json").write_text("{}")
        cloud = haylen.Plugin(scratch / "cloud", {"id": "cloud", "version": "1.0.0", "android": {"module": "android", "files": [{"from": "services.json", "to": "app/services.json"}]}}, None)
        app = mock.Mock(plugins=[cloud], plugin_values={"cloud": {}}, folder=scratch / "game", build_folder=scratch / "build")
        project = scratch / "project"
        arguments = haylen.argparse.Namespace(config="Debug", jobs=1)
        with mock.patch.object(haylen, "run") as run, mock.patch.object(haylen, "ensure_gradle", return_value=Path("gradle")), mock.patch.object(haylen, "android_product", return_value=Path("app.apk")), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            with self.assertRaises(haylen.BuildError) as stopped:
                haylen.build_android(app, project, arguments)
            self.assertIn('needs "app/services.json"', str(stopped.exception))
            run.assert_not_called()
            (project / "app").mkdir(parents=True)
            (project / "app" / "services.json").write_text("{}")
            haylen.build_android(app, project, arguments)
            run.assert_called_once()

    def test_a_release_too_large_for_the_base_module_moves_its_content_shards_into_install_time_asset_packs(self):
        scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, scratch)
        generated = scratch / "project" / haylen.GENERATED_FOLDER
        release = generated / "assets" / "app"
        release.mkdir(parents=True)
        for name in ("app.hmanifest", "content.hmanifest"):
            (release / name).write_bytes(b"m" * 10)
        for index in range(6):
            (release / f"{index:064x}.hpak").write_bytes(b"s" * 100)
        kept = {"app.hmanifest", "content.hmanifest", f"{0:064x}.hpak"}
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})), mock.patch.object(haylen, "ANDROID_ASSET_PACK_LIMIT", 250):
            packs = haylen.write_android_asset_packs(generated, kept)
        self.assertEqual(packs, ["haylencontent1", "haylencontent2", "haylencontent3"])
        self.assertEqual({path.name for path in release.iterdir()}, kept, "The manifests and the shards of the app domain stay in the base module.")
        self.assertEqual([len(list((generated / "asset-packs" / pack / "src" / "main" / "assets" / "app").iterdir())) for pack in packs], [2, 2, 1])
        self.assertIn('deliveryType.set("install-time")', (generated / "asset-packs" / "haylencontent1" / "build.gradle.kts").read_text())
        self.assertEqual(haylen.android_release_shards(scratch / "project"), {f"{index:064x}.hpak" for index in range(6)})

    def test_android_releases_build_shards_that_fit_an_asset_pack_in_lua_and_cpp_apps(self):
        target = haylen.CONTENT_SHARD_TARGETS["android"]
        self.assertLessEqual(target * 2 + 5 * 1024**2, haylen.ANDROID_ASSET_PACK_LIMIT, "A shard holds at most twice its target and one record, so it fits a pack.")
        self.assertIn(f"set(shard_target {target})", (haylen.ROOT / "engine" / "cmake" / "haylen-app.cmake").read_text(encoding="utf-8"))

    def test_a_build_only_run_builds_and_checks_every_abi_without_a_device(self):
        args = haylen.build_parser().parse_args(["run", "game", "--platform", "android", "--config", "Release", "--build-only"])
        with mock.patch.object(haylen, "android_device") as device, mock.patch.object(haylen, "build_android", return_value=Path("app.apk")) as build, mock.patch.object(haylen, "check_android", return_value=[]) as check, mock.patch.object(haylen, "launch_android") as launch:
            haylen.run_android(mock.Mock(), Path("project"), args)
        device.assert_not_called()
        self.assertIsNone(build.call_args.args[3])
        check.assert_called_once()
        launch.assert_not_called()

    def test_a_debug_build_for_a_device_packs_only_its_abi(self):
        app = mock.Mock(plugins=[], plugin_values={}, build_folder=Path("build"))
        for config, abi, packed in (("Debug", "x86_64", True), ("Debug", None, False), ("Release", "x86_64", False)):
            arguments = haylen.argparse.Namespace(config=config, jobs=1)
            with mock.patch.object(haylen, "run") as run, mock.patch.object(haylen, "ensure_gradle", return_value=Path("gradle")), mock.patch.object(haylen, "android_product", return_value=Path("app.apk")), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
                haylen.build_android(app, Path("project"), arguments, abi)
            self.assertEqual("-Phaylen.abi=x86_64" in run.call_args.args[0], packed, (config, abi))


class CheckTest(unittest.TestCase):
    """The check of a web site or a Windows or Linux folder looks in the build folder of the app."""

    def test_checks_the_site_in_the_build_folder_of_the_app(self):
        with tempfile.TemporaryDirectory() as scratch:
            app = Path(scratch).resolve() / "game"
            app.mkdir()
            (app / "app.json").write_text('{"name": "Game", "identifier": "com.example.game", "version": "1.0.0"}')
            checked = []
            target = haylen.RUN_TARGETS["web"]
            with mock.patch.dict(haylen.RUN_TARGETS, {"web": dataclasses.replace(target, check=lambda info, root, args: checked.append(root) or [])}):
                haylen.command_check(mock.Mock(app=str(app), platform="web", config="Release"))
            self.assertEqual(checked, [haylen.App(app, "web").build_folder / "web"])

    def test_checks_the_apple_bundles_that_cpp_apps_build_with_ninja_and_xcode(self):
        with tempfile.TemporaryDirectory() as scratch:
            app = Path(scratch).resolve() / "game"
            app.mkdir()
            (app / "app.json").write_text('{"name": "Game", "identifier": "com.example.game", "version": "1.0.0"}')
            (app / "CMakeLists.txt").write_text("")
            builds = Path(scratch).resolve() / "cpp"
            layouts = {"macos": ("macos-debug/bin/main/main.app/Contents", "Contents/MacOS/main"), "ios-simulator": ("ios-simulator-debug/bin/main.app", "main")}
            for platform, (contents, executable) in layouts.items():
                info = {"CFBundleExecutable": "main", "UIApplicationSceneManifest": {}}
                bundle = builds / haylen.build_folder_name(app) / contents
                (bundle / "Resources").mkdir(parents=True)
                (bundle / "Info.plist").write_bytes(haylen.plistlib.dumps(info))
                shutil.copy(haylen.ENGINE_DIR / "platform" / "apple" / "PrivacyInfo.xcprivacy", bundle / ("Resources" if platform == "macos" else ""))
                patches = [mock.patch.object(haylen, "CPP_BUILDS_DIR", builds), mock.patch.object(haylen, "ARTIFACTS_DIR", Path(scratch) / "artifacts"), mock.patch.object(haylen, "apple_entitlements", return_value={}), mock.patch.object(haylen, "capture", return_value=""), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {}))]
                with contextlib.ExitStack() as stack:
                    for patch in patches:
                        stack.enter_context(patch)
                    linked = stack.enter_context(mock.patch.object(haylen, "apple_linked", return_value=set()))
                    haylen.command_check(haylen.build_parser().parse_args(["check", str(app), "--platform", platform]))
                self.assertEqual(linked.call_args.args[0], builds / haylen.build_folder_name(app) / layouts[platform][0].removesuffix("/Contents") / executable, platform)

    def test_stops_at_the_cpp_builds_that_the_cmake_project_lays_out_itself(self):
        with tempfile.TemporaryDirectory() as scratch:
            app = Path(scratch).resolve() / "game"
            app.mkdir()
            (app / "app.json").write_text('{"name": "Game", "identifier": "com.example.game", "version": "1.0.0"}')
            (app / "CMakeLists.txt").write_text("")
            with self.assertRaisesRegex(haylen.BuildError, 'The command "check" covers C\\+\\+ apps on Apple platforms and Android.'):
                haylen.command_check(haylen.build_parser().parse_args(["check", str(app), "--platform", "web"]))


class WebMemoryTest(unittest.TestCase):
    """The memory check of the web stops on a page that failed, missed the download or holds memory that grows with what it downloads."""

    def test_stops_on_memory_that_grows_with_the_download(self):
        good = {"failure": None, "finished": True, "receivedBytes": haylen.WEB_MEMORY_FILE_SIZE + 1, "peakWasmBytes": 128 * 1024**2, "buffersAfterCollection": 16 * 1024**2, "log": []}
        haylen.check_web_memory(good)
        for change, message in (({"peakWasmBytes": haylen.WEB_MEMORY_FILE_SIZE}, "grows with what it downloads"), ({"buffersAfterCollection": 128 * 1024**2}, "array buffers"), ({"receivedBytes": 1024}, "fewer than"), ({"finished": False}, "did not finish"), ({"failure": "Broken."}, "failed: Broken.")):
            with self.assertRaisesRegex(haylen.BuildError, message):
                haylen.check_web_memory({**good, **change})

    def test_stops_on_a_page_without_files_that_misses_its_room(self):
        refused = {"downloadedMedium": True, "finished": False, "failure": "The content needs 486613920 bytes of room on the device and 251656020 are free."}
        haylen.check_web_memory_without_files(refused)
        for change, message in (({"downloadedMedium": False}, "did not download the medium file"), ({"finished": True, "failure": None}, "downloaded it"), ({"failure": "The network failed."}, "reported: The network failed.")):
            with self.assertRaisesRegex(haylen.BuildError, message):
                haylen.check_web_memory_without_files({**refused, **change})


class WebErrorScreenTest(unittest.TestCase):
    def test_the_browsers_of_the_check_draw_through_swiftshader_so_webgpu_starts_without_a_gpu(self):
        target = mock.Mock(plugins="web")
        target.prepare.return_value = Path("site")
        browsers = []

        @contextlib.contextmanager
        def browser(arguments, purpose):
            browsers.append(arguments)
            yield 0

        with mock.patch.object(haylen, "App"), mock.patch.object(haylen, "serve_site"), mock.patch.object(haylen, "headless_browser", side_effect=browser), mock.patch.object(haylen, "emsdk_node"), mock.patch.object(haylen, "capture", return_value="{}"), mock.patch.object(haylen, "check_web_page_steps"), mock.patch.object(haylen, "terminal"):
            haylen.check_web_error_screen(mock.Mock(graphics_api=None), target)
        # Every mode opens once per graphics API, and the page with an unknown graphics API opens once more.
        self.assertEqual(len(browsers), 2 * len(haylen.WEB_ERROR_SCREEN_STEPS) + 1)
        self.assertTrue(all(arguments == haylen.WEB_GPU_BROWSER_FLAGS for arguments in browsers))

    def test_a_named_graphics_api_draws_every_page_of_the_checks_through_it(self):
        target = mock.Mock(plugins="web")
        target.prepare.return_value = Path("site")
        server = mock.Mock(server_address=("127.0.0.1", 8000))
        commands = []

        @contextlib.contextmanager
        def browser(arguments, purpose):
            yield 0

        def capture(command):
            commands.append(command)
            return "{}"

        def pages(api):
            commands.clear()
            arguments = mock.Mock(graphics_api=api)
            with mock.patch.object(haylen, "App"), mock.patch.object(haylen, "serve_site", return_value=server), mock.patch.object(haylen, "headless_browser", side_effect=browser), mock.patch.object(haylen, "emsdk_node"), mock.patch.object(haylen, "capture", side_effect=capture), mock.patch.object(haylen, "check_web_page_steps"), mock.patch.object(haylen, "terminal"):
                haylen.check_web_error_screen(arguments, target)
                haylen.check_web_wake_lock(arguments, target)
            return [command[3] for command in commands]

        # The page with an unknown graphics API draws its error screen through the first API that starts, so only the checks of that API open it.
        unknown = "http://127.0.0.1:8000/?graphicsApi=bogus"
        for api, opens_unknown in (("webGpu", True), ("webGl2", False)):
            named = f"http://127.0.0.1:8000/?graphicsApi={api}"
            self.assertEqual(pages(api), [named] * len(haylen.WEB_ERROR_SCREEN_STEPS) + [unknown] * opens_unknown + [named] * len(haylen.WEB_WAKE_LOCK_STEPS))

    def test_on_the_web_a_graphics_api_names_the_api_of_the_checks_unless_the_gpu_tests_run(self):
        with mock.patch.object(haylen, "command_test_web") as checks, mock.patch.object(haylen, "command_test_gpu_web") as gpu:
            haylen.command_test(haylen.build_parser().parse_args(["test", "--platform", "web", "--graphics-api", "webGl2"]))
            haylen.command_test(haylen.build_parser().parse_args(["test", "--gpu", "--platform", "web", "--graphics-api", "webGl2"]))
        self.assertEqual(checks.call_args.args[0].graphics_api, "webGl2")
        self.assertEqual(gpu.call_args.args[0].graphics_api, "webGl2")
        with self.assertRaisesRegex(haylen.BuildError, 'draw through "webGpu" or "webGl2", not "metal"'):
            haylen.command_test(haylen.build_parser().parse_args(["test", "--platform", "web", "--graphics-api", "metal"]))


class CppReleaseTest(unittest.TestCase):
    """The Release configuration of a C++ project ships the protected release with the content tool of the desktop artifacts, and every other configuration turns it off again in the cache of the build tree."""

    def test_turns_the_protected_release_on_only_for_release(self):
        self.assertEqual(haylen.cpp_release_options(mock.Mock(config="Debug")), ["-DHAYLEN_PROTECTED_RELEASE=OFF"])
        with mock.patch.object(haylen, "content_tool", return_value=Path("/tools/haylen-content")):
            options = haylen.cpp_release_options(mock.Mock(config="Release", engine_config="Release", jobs=2, previous_release=None))
            following = haylen.cpp_release_options(mock.Mock(config="Release", engine_config="Release", jobs=2, previous_release="/shipped/app"))
        self.assertEqual(options, ["-DHAYLEN_PROTECTED_RELEASE=ON", "-DHAYLEN_CONTENT_TOOL=/tools/haylen-content", "-DHAYLEN_PREVIOUS_RELEASE="])
        self.assertEqual(following[-1], f"-DHAYLEN_PREVIOUS_RELEASE={Path('/shipped/app').resolve().as_posix()}")

    def test_a_release_follows_the_release_that_shipped_before_the_one_of_the_build_folder(self):
        scratch = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, scratch)
        app = mock.Mock(folder=scratch / "game", build_folder=scratch / "build", build=7, content_server=None, content_packs=[])
        app.layout = haylen.PackageLayout(app.folder, {})
        local = haylen.release_folder(app, "desktop")
        local.mkdir(parents=True)
        (local / "app.hmanifest").write_bytes(b"")
        commands = []
        def build(command, **_):
            commands.append(command)
            Path(command[command.index("--output") + 1]).mkdir(parents=True)
        patches = [mock.patch.object(haylen, "compile_app_shaders"), mock.patch.object(haylen, "content_tool", return_value=Path("haylen-content")), mock.patch.object(haylen, "ensure_keys", return_value=scratch / "keys"), mock.patch.object(haylen, "run", side_effect=build), mock.patch.object(haylen, "warn_store_size"), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {}))]
        with contextlib.ExitStack() as stack:
            for patch in patches:
                stack.enter_context(patch)
            haylen.build_release(app, "desktop", "Release", 1, str(scratch / "shipped"))
            (local / "app.hmanifest").write_bytes(b"")
            haylen.build_release(app, "desktop", "Release", 1, None)
        self.assertEqual(commands[0][commands[0].index("--previous") + 1], scratch / "shipped")
        self.assertEqual(commands[1][commands[1].index("--previous") + 1], local)

    def test_names_the_build_that_made_no_release(self):
        with tempfile.TemporaryDirectory() as scratch:
            with self.assertRaises(haylen.BuildError) as raised:
                haylen.cpp_release_app(Path(scratch), "game")
        self.assertIn("made no protected release", str(raised.exception))


class SteamTest(unittest.TestCase):
    """The Steam staging names a depot for the platform and one for every pack, and writes the build scripts that steamcmd runs."""

    def test_writes_a_build_script_per_depot(self):
        with tempfile.TemporaryDirectory() as scratch:
            args = mock.Mock(depot=["macos=1001", "forest=1002"], platform="macos")
            depots = haylen.steam_depot_ids(args, ["macos", "forest"])
            haylen.write_steam_scripts(Path(scratch), "1000", "Island 1.2.0", depots, "beta")
            build = (Path(scratch) / "scripts" / "app_build.vdf").read_text()
            self.assertIn('"AppID" "1000"', build)
            self.assertIn('"SetLive" "beta"', build)
            self.assertIn('"1002" "depot_build_1002.vdf"', build)
            forest = (Path(scratch) / "scripts" / "depot_build_1002.vdf").read_text()
            self.assertIn('"ContentRoot" "../content/forest/"', forest)

    def test_escapes_every_value_that_comes_from_the_app_or_the_command(self):
        with tempfile.TemporaryDirectory() as scratch:
            haylen.write_steam_scripts(Path(scratch), "1000", 'Island "Deluxe" C:\\saves\nsecond line\tend', {"forest": "1002"}, 'beta"\n"AppID" "1')
            build = (Path(scratch) / "scripts" / "app_build.vdf").read_text()
        self.assertIn('\t"Desc" "Island \\"Deluxe\\" C:\\\\saves\\nsecond line\\tend"\n', build)
        self.assertIn('\t"SetLive" "beta\\"\\n\\"AppID\\" \\"1"\n', build)
        self.assertEqual(build.count('"AppID"'), 1)

    def test_names_the_depots_without_an_id(self):
        args = mock.Mock(depot=["macos=1001", "forest=x"], platform="macos")
        with self.assertRaises(haylen.BuildError):
            haylen.steam_depot_ids(args, ["macos"])
        args = mock.Mock(depot=["macos=1001"], platform="macos")
        with self.assertRaisesRegex(haylen.BuildError, '"forest"'):
            haylen.steam_depot_ids(args, ["macos", "forest"])


class SiteServerTest(unittest.TestCase):
    """The server of `run --platform web` serves the shards of a protected release by byte range with a strong validator, as a content delivery network does."""

    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        (Path(self.scratch.name) / "app").mkdir()
        (Path(self.scratch.name) / "app" / "shard.hpak").write_bytes(bytes(range(256)) * 4)
        handler = functools.partial(type("Handler", (haylen.WebHandler,), {"log_message": lambda *arguments: None}), directory=self.scratch.name)
        self.web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        threading.Thread(target=self.web.serve_forever, daemon=True).start()
        self.addCleanup(self.web.server_close)
        self.addCleanup(self.web.shutdown)

    def get(self, headers):
        connection = http.client.HTTPConnection("127.0.0.1", self.web.server_address[1], timeout=5)
        self.addCleanup(connection.close)
        connection.request("GET", "/app/shard.hpak", headers=headers)
        response = connection.getresponse()
        return response, response.read()

    def test_serves_every_connection_that_a_page_opens_while_the_server_is_busy(self):
        site = Path(self.scratch.name)
        (site / "app.zip").write_bytes(b"z" * 200_000)
        handler = functools.partial(type("Handler", (haylen.WebHandler,), {"log_message": lambda *arguments: None}), directory=self.scratch.name)
        server = haylen.LocalServer(("127.0.0.1", 0), handler)
        self.addCleanup(server.server_close)

        # The page opens its connections while the server is too busy to accept them, as on a machine under load, and the listen queue holds every one until the server takes it.
        statuses = []
        connections = []
        for _ in range(48):
            try:
                connection = socket.create_connection(("127.0.0.1", server.server_address[1]), timeout=10)
                self.addCleanup(connection.close)
                connection.sendall(b"GET /app.zip HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
                connections.append(connection)
            except OSError as error:
                statuses.append(type(error).__name__.encode())
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.shutdown)
        for connection in connections:
            try:
                response = b""
                while chunk := connection.recv(1 << 16):
                    response += chunk
                statuses.append(response.split(b"\r\n", 1)[0] if len(response) > 200_000 else b"short")
            except OSError as error:
                statuses.append(type(error).__name__.encode())
        self.assertEqual(statuses, [b"HTTP/1.0 200 OK"] * 48)

    def test_serves_ranges_and_the_whole_file_when_the_file_changed(self):
        whole, body = self.get({})
        self.assertEqual(whole.status, 200)
        self.assertEqual(len(body), 1024)
        validator = whole.getheader("ETag")
        self.assertTrue(validator.startswith('"'))

        part, body = self.get({"Range": "bytes=10-19"})
        self.assertEqual(part.status, 206)
        self.assertEqual(part.getheader("Content-Range"), "bytes 10-19/1024")
        self.assertEqual(body, bytes(range(10, 20)))

        resumed, body = self.get({"Range": "bytes=1000-", "If-Range": validator})
        self.assertEqual((resumed.status, len(body)), (206, 24))
        changed, body = self.get({"Range": "bytes=1000-", "If-Range": '"other"'})
        self.assertEqual((changed.status, len(body)), (200, 1024))
        beyond, _ = self.get({"Range": "bytes=5000-"})
        self.assertEqual(beyond.status, 416)

    def test_sends_no_isolation_policy_that_blocks_the_scripts_of_plugins(self):
        response, _ = self.get({})
        self.assertIsNone(response.getheader("Cross-Origin-Embedder-Policy"))
        self.assertIsNone(response.getheader("Cross-Origin-Opener-Policy"))
        self.assertEqual(haylen.build_parser().parse_args(["serve", "site"]).coep, "off")


class DevelopmentServerTest(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.folder = Path(self.scratch.name) / "app"
        for path, text in {"app.json": "{}", "source/main.lua": "print(1)", "source/level.lua": "return {}", "content/data.json": "{}"}.items():
            (self.folder / path).parent.mkdir(parents=True, exist_ok=True)
            (self.folder / path).write_text(text)
        self.layout = haylen.PackageLayout(self.folder, {})
        self.manifest = haylen.DevelopmentServer.hash_files({path: self.folder / path for path in ("app.json", "source/main.lua", "source/level.lua", "content/data.json")})
        quiet = mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {}))
        quiet.start()
        self.addCleanup(quiet.stop)

    def tearDown(self):
        self.scratch.cleanup()

    def write(self, path, text):
        file = self.folder / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text)
        os.utime(file, ns=(file.stat().st_atime_ns, file.stat().st_mtime_ns + 5_000_000_000))

    def test_the_accept_key_follows_rfc_6455(self):
        self.assertEqual(haylen.websocket_accept("dGhlIHNhbXBsZSBub25jZQ=="), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=")

    def test_frames_encode_and_masked_frames_decode_at_every_length(self):
        for size in (5, 300, 70_000):
            payload = bytes(index % 251 for index in range(size))
            self.assertEqual(haylen.read_websocket_frame(io.BytesIO(haylen.websocket_frame(haylen.WEBSOCKET_BINARY, payload))), (True, haylen.WEBSOCKET_BINARY, payload))
            mask = bytes([1, 2, 3, 4])
            length = bytes([0x80 | size]) if size < 126 else bytes([0x80 | 126]) + size.to_bytes(2, "big") if size < 1 << 16 else bytes([0x80 | 127]) + size.to_bytes(8, "big")
            masked = bytes([0x80 | haylen.WEBSOCKET_TEXT]) + length + mask + bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
            self.assertEqual(haylen.read_websocket_frame(io.BytesIO(masked)), (True, haylen.WEBSOCKET_TEXT, payload))
        self.assertIsNone(haylen.read_websocket_frame(io.BytesIO(b"\x81")))

    def test_only_files_of_the_package_count(self):
        for path in ("app.json", "source/scenes/level.lua", "content/hero.png", "plugins/ads/plugin.json", "plugins/ads/source/init.lua"):
            self.assertTrue(haylen.development_watched(path), path)
        for path in ("source/.level.lua.swp", "source/level.lua~", "source/#level.lua#", "source/notes.txt", "content/hero.png.tmp", "content/.cache", "plugins/ads/android/build.gradle", "platform/web/index.html", "README.md"):
            self.assertFalse(haylen.development_watched(path), path)

    def test_the_watcher_sends_saved_files_and_leaves_editor_files(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        self.write("source/level.lua", "return {edited = true}")
        self.write("source/.level.lua.swp", "swap")
        self.write("content/new.json", "[]")
        (self.folder / "content" / "data.json").unlink()
        self.assertEqual(server.scan(), (["content/new.json", "source/level.lua"], ["content/data.json"]))
        self.assertEqual(server.scan(), ([], []))

    def test_a_file_that_goes_away_while_the_watcher_reads_it_keeps_the_watcher_running(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        # An editor or a tool deletes its scratch file between the listing of the package and the read of the file.
        listed = {**self.layout.files(), "content/scratch.png": self.folder / "content" / "scratch.png"}
        with mock.patch.object(haylen.PackageLayout, "files", return_value=listed):
            self.assertEqual(server.scan(), ([], []))
        # An editor that saves "app.json" may move the file aside before it writes the new one.
        (self.folder / "app.json").rename(self.folder / "app.json~")
        self.assertEqual(server.scan(), ([], ["app.json"]))
        (self.folder / "app.json~").rename(self.folder / "app.json")
        self.assertEqual(server.scan(), (["app.json"], []))
        self.write("content/sprite.png", "sprite")
        self.assertEqual(server.scan(), (["content/sprite.png"], []))
        (self.folder / "content" / "sprite.png").unlink()
        message, contents = server.batch(["content/sprite.png"], [])
        self.assertEqual((message["files"], contents), ([], []))
        self.assertEqual(server.scan(), ([], ["content/sprite.png"]))

    def test_late_apps_catch_up(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        self.write("source/level.lua", "return {edited = true}")
        (self.folder / "content" / "data.json").unlink()
        server.scan()
        fresh, contents = server.catch_up({"type": "hello", "session": "", "revision": 0})
        self.assertEqual([file["path"] for file in fresh["files"]], ["source/level.lua"])
        self.assertEqual(fresh["removed"], ["content/data.json"])
        self.assertEqual(contents, [b"return {edited = true}"])
        foreign, _ = server.catch_up({"type": "hello", "session": "earlier", "revision": 3})
        self.assertEqual([file["path"] for file in foreign["files"]], ["app.json", "source/level.lua", "source/main.lua"])
        self.assertIsNone(server.catch_up({"type": "hello", "session": server.session, "revision": server.revision}))

    def test_the_console_runs_its_lines_in_every_app_and_keeps_its_line_under_their_results(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        sent = []
        client = mock.Mock(send=lambda opcode, payload: sent.append(json.loads(payload)) or True)
        output = io.StringIO()
        with mock.patch.object(haylen, "terminal", haylen.Terminal(output, output, {})):
            self.assertEqual(server.send_line("gold 50"), 0)
            server.clients.append(client)
            session = haylen.RunSession(server, [])
            session.press("d")
            for key in [*"gold 55", "Backspace", "0", "Enter", "Enter", *"  help ", "Enter"]:
                session.press(key)
            self.assertTrue(output.getvalue().endswith("\r\x1b[2K" + haylen.CONSOLE_PROMPT))
            server.receive(client, {"type": "consoleResult", "id": 2, "ok": True, "output": 'Gold 50, the "shop" pays.'})
            server.receive(client, {"type": "consoleResult", "id": 3, "ok": False, "output": "There is no command."})
            session.press("Escape")
            session.press("q")
        self.assertEqual([message for message in sent if message["type"] == "console"], [{"type": "console", "id": 2, "line": "gold 50"}, {"type": "console", "id": 3, "line": "help"}])
        self.assertEqual(sum(message["type"] == "consoleCommands" for message in sent), 3)
        printed = output.getvalue()
        self.assertIn(haylen.CONSOLE_HINT.split(".")[0], printed)
        self.assertIn('\r\x1b[2KGold 50, the "shop" pays.\n\r\x1b[2K> ', printed)
        self.assertIn("Error: There is no command.", printed)
        self.assertTrue(printed.endswith("\r\x1b[2K"))
        self.assertFalse(session.typing)
        self.assertTrue(session.quitting.is_set())

    def test_the_line_of_the_console_edits_recalls_and_completes_from_the_commands_of_the_app(self):
        line = haylen.ConsoleLine()
        line.commands = [{"name": "god", "arguments": ["boolean"]}, {"name": "gold", "arguments": ["integer"]}, {"name": "help", "arguments": ["string?"]}]
        for key in "gd":
            line.edit(key)
        line.edit("Left")
        line.edit("o")
        self.assertEqual((line.text, line.cursor), ("god", 2))
        line.edit("End")
        self.assertEqual(line.edit("Enter"), "god")
        self.assertEqual(line.edit("Enter"), "")
        line.edit("x")
        line.edit("Up")
        self.assertEqual(line.text, "god")
        line.edit("Down")
        self.assertEqual(line.text, "x")
        line.edit("ClearLine")

        line.edit("g")
        self.assertEqual(line.complete(), ["god", "gold"])
        self.assertEqual(line.text, "go")
        line.edit("d")
        self.assertEqual(line.complete(), [])
        self.assertEqual(line.text, "god ")
        line.edit("t")
        line.complete()
        self.assertEqual(line.text, "god true ")
        line.replace("gold ")
        self.assertEqual(line.complete(), [])
        self.assertEqual(line.text, "gold ")

    def test_the_keys_of_the_tools_run_their_lines_in_the_apps(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        sent = []
        server.clients.append(mock.Mock(send=lambda opcode, payload: sent.append(json.loads(payload)) or True))
        session = haylen.RunSession(server, [])
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            for key in "sip":
                session.press(key)
        self.assertEqual([message["line"] for message in sent], ["stats", "inspector", "draw"])

    def test_the_reader_names_the_keys_that_edit(self):
        reader = haylen.KeyReader(io.StringIO())
        self.assertEqual([reader.name(character) for character in ("a", "\r", "\t", "\x7f", "\x15", "\x02")], ["a", "Enter", "Tab", "Backspace", "ClearLine", ""])
        with mock.patch.object(reader, "waiting", return_value=True), mock.patch.object(reader, "character", side_effect=["[", "A", "[", "3", "~"]):
            self.assertEqual(reader.name("\x1b"), "Up")
            self.assertEqual(reader.name("\x1b"), "Delete")
        with mock.patch.object(reader, "waiting", return_value=False):
            self.assertEqual(reader.name("\x1b"), "Escape")

    def answering_app(self, server, answer, report):
        """An app that answers the console line of a capture with its result and then with the report of its capture, from another thread as a connection does."""
        sent = []

        def answer_line(app, message):
            server.receive(app, {"type": "consoleResult", "id": message["id"], **answer})
            if report is not None:
                server.receive(app, {"type": "report", "gpuCapture": report})

        def send(opcode, payload):
            message = json.loads(payload)
            sent.append(message)
            threading.Thread(target=answer_line, args=(app, message)).start()
            return True

        app = mock.Mock(send=send)
        return app, sent

    def test_the_key_g_asks_the_apps_for_a_gpu_capture_and_its_report_prints(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        sent = []
        client = mock.Mock(send=lambda opcode, payload: sent.append(json.loads(payload)) or True)
        server.clients.append(client)
        output = io.StringIO()
        with mock.patch.object(haylen, "terminal", haylen.Terminal(output, output, {})):
            haylen.RunSession(server, []).press("g")
            server.receive(client, {"type": "report", "gpuCapture": {"ok": True, "path": "/captures/one.gputrace", "tool": "metalFrameCapture", "frames": 1}})
            server.receive(client, {"type": "report", "gpuCapture": {"ok": False, "error": "RenderDoc wrote no capture."}})
        self.assertEqual(sent, [{"type": "console", "id": 1, "line": "gpuCapture"}])
        self.assertIn('The app wrote the GPU capture of 1 frame with "metalFrameCapture" to /captures/one.gputrace.', output.getvalue())
        self.assertIn("The GPU capture of the app failed: RenderDoc wrote no capture.", output.getvalue())

    def test_a_gpu_capture_waits_for_every_app_and_returns_what_each_one_wrote(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        self.assertEqual(server.capture_gpu(1), [])
        written = {"ok": True, "path": "/captures/two.rdc", "tool": "renderDoc", "frames": 2}
        capturing, sent = self.answering_app(server, {"ok": True, "output": "Capturing the next 2 frames."}, written)
        refusing, _ = self.answering_app(server, {"ok": False, "output": "RenderDoc is not loaded into the app."}, None)
        server.clients += [capturing, refusing]
        results = server.capture_gpu(2)
        self.assertEqual(sent, [{"type": "console", "id": 2, "line": "gpuCapture 2"}])
        self.assertCountEqual(results, [written, {"ok": False, "error": "RenderDoc is not loaded into the app."}])
        self.assertEqual(server.awaited, {})
        self.assertIsNone(server.gpu_reports)

    def test_the_capture_command_reaches_the_run_through_the_file_of_its_server_and_its_token(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        written = {"ok": True, "path": "/captures/one.gputrace", "tool": "metalFrameCapture", "frames": 1}
        app, _ = self.answering_app(server, {"ok": True, "output": "Capturing the next frame."}, written)
        server.clients.append(app)
        handler = type("Handler", (haylen.DevelopmentHandler,), {"development": server})
        web = haylen.LocalServer(("127.0.0.1", 0), handler)
        threading.Thread(target=web.serve_forever, daemon=True).start()
        self.addCleanup(web.server_close)
        self.addCleanup(web.shutdown)
        port = web.server_address[1]
        apps = Path(self.scratch.name) / "apps"
        output = io.StringIO()
        arguments = argparse.Namespace(app=str(self.folder), frames=1)
        with mock.patch.object(haylen, "APPS_DIR", apps), mock.patch.object(haylen, "resolve_lua_app", return_value=self.folder), mock.patch.object(haylen, "terminal", haylen.Terminal(output, output, {})):
            with self.assertRaisesRegex(haylen.BuildError, "No run of the app"):
                haylen.command_capture(arguments)
            server.advertise("0.0.0.0", port)
            record = apps / haylen.build_folder_name(self.folder) / haylen.DEVELOPMENT_RECORD
            self.assertEqual(record.stat().st_mode & 0o777, 0o600)
            self.assertEqual(json.loads(record.read_text())["address"], f"http://127.0.0.1:{port}")
            haylen.command_capture(arguments)
            self.assertIn('The app wrote the GPU capture of 1 frame with "metalFrameCapture" to /captures/one.gputrace.', output.getvalue())

            for token, body, status in (("wrong", b"{}", 403), (server.token, b'{"frames": 0}', 400)):
                connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
                connection.request("POST", f"{haylen.GPU_CAPTURE_PATH}?token={token}", body=body)
                self.assertEqual(connection.getresponse().status, status)
                connection.close()

            server.close()
            self.assertFalse(record.exists())
            with self.assertRaisesRegex(haylen.BuildError, "No run of the app"):
                haylen.command_capture(arguments)

    def test_a_development_launch_starts_apple_apps_with_the_frame_capture_of_metal(self):
        bundle = Path(self.scratch.name) / "Game.app"
        bundle.mkdir()
        (bundle / "Info.plist").write_bytes(plistlib.dumps({"CFBundleExecutable": "Game", "CFBundleIdentifier": "com.haylen.game"}))
        simulator = {"udid": "SIMULATOR", "name": "iPhone 17e", "state": "Booted"}
        for development, simulated, device in ((haylen.DevelopmentServer(self.layout, self.manifest), {"SIMCTL_CHILD_MTL_CAPTURE_ENABLED": "1"}, ["--environment-variables", '{"MTL_CAPTURE_ENABLED": "1"}']), (None, {}, [])):
            session = haylen.RunSession(development, [])
            with mock.patch.object(haylen, "run"), mock.patch.object(haylen, "unified_log", return_value=contextlib.nullcontext()), mock.patch.object(haylen, "run_rows", return_value=[]), mock.patch.object(haylen, "apple_device_name", return_value="iPhone"), mock.patch.object(session, "show"), mock.patch.object(session, "follow") as follow:
                haylen.launch_apple(mock.Mock(), bundle, argparse.Namespace(platform="ios-simulator", device=None), simulator, session)
                self.assertEqual(follow.call_args.kwargs["environment"], simulated)
                haylen.launch_apple(mock.Mock(), bundle, argparse.Namespace(platform="ios", device="DEVICE"), None, session)
                self.assertEqual(follow.call_args.args[0], ["xcrun", "devicectl", "device", "process", "launch", "--console", "--device", "DEVICE", *device, "com.haylen.game"])

    def test_the_console_keys_send_what_changed_and_restart_the_apps(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        sent = []
        client = mock.Mock(send=lambda opcode, payload: sent.append(json.loads(payload)) or True, send_batch=mock.Mock(return_value=True))
        server.clients.append(client)
        output = io.StringIO()
        session = haylen.RunSession(server, [])
        with mock.patch.object(haylen, "terminal", haylen.Terminal(output, output, {})):
            self.write("source/level.lua", "return {edited = true}")
            for key in "rRr":
                session.press(key)
        self.assertEqual([file["path"] for file in client.send_batch.call_args.args[0]["files"]], ["source/level.lua"])
        self.assertEqual(sent, [{"type": "restart"}])
        self.assertIn("Nothing changed since the last files the apps received.", output.getvalue())

    def test_the_keys_of_a_run_quit_clear_and_list_and_a_release_offers_only_those(self):
        output = io.StringIO()
        stop = mock.Mock()
        with mock.patch.object(haylen, "terminal", haylen.Terminal(output, output, {"FORCE_COLOR": "1"})):
            session = haylen.RunSession()
            self.assertEqual(list(session.keys), ["c", "q", "h"])
            session.show([("App", "Demo")], stop)
            for key in "rRdsipgx":
                session.press(key)
            session.press("c")
            session.press("q")
        stop.assert_called_once_with()
        self.assertTrue(session.quitting.is_set())
        self.assertIn("\x1b[2J", output.getvalue())
        self.assertIn("Quit", output.getvalue())
        self.assertEqual(list(haylen.RunSession(haylen.DevelopmentServer(self.layout, self.manifest), []).keys), ["r", "R", "d", "s", "i", "p", "g", "c", "q", "h"])

    def test_the_log_of_android_shows_the_lines_of_the_engine_as_it_wrote_them_and_the_tag_of_the_others(self):
        self.assertEqual(haylen.android_log_line("I/haylen  : [12:00:01] [info] The app draws through \"vulkan\"."), "[12:00:01] [info] The app draws through \"vulkan\".")
        self.assertEqual(haylen.android_log_line("E/AndroidRuntime: FATAL EXCEPTION: main"), "E/AndroidRuntime: FATAL EXCEPTION: main")
        self.assertIsNone(haylen.android_log_line("--------- beginning of main"))

    def test_the_log_of_a_simulator_leaves_out_the_lines_of_its_launcher(self):
        lines = ["com.haylen.game: 65678", "[info] The app draws through \"metal\".", "Child process terminated with signal 15: Terminated"]
        self.assertEqual([haylen.simulator_line("com.haylen.game", line) for line in lines], [None, lines[1], None])

    def test_a_run_that_quits_ends_its_process_without_an_error(self):
        session = haylen.RunSession()
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            session.show([("App", "Demo")])
        threading.Timer(0.2, session.quit).start()
        session.follow([sys.executable, "-c", "import time; time.sleep(30)"])
        self.assertTrue(session.quitting.is_set())
        with self.assertRaisesRegex(haylen.BuildError, "failed with exit code 3"):
            haylen.RunSession().follow([sys.executable, "-c", "raise SystemExit(3)"])

    @unittest.skipIf(os.name == "nt", "Windows ends a process that a signal reaches, so a command there has no signal to stop with.")
    def test_a_run_that_a_signal_ends_stops_the_app_it_follows(self):
        previous = {name: signal.getsignal(getattr(signal, name)) for name in haylen.TERMINATING_SIGNALS}
        self.addCleanup(lambda: [signal.signal(getattr(signal, name), handler) for name, handler in previous.items()])
        haylen.Terminated.install()

        # The app prints a line and runs on, and the signal comes as a bound on the time of the run would send it.
        def line(text):
            os.kill(os.getpid(), signal.SIGALRM)
            return text

        stop = mock.Mock()
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})), self.assertRaises(haylen.Terminated) as ended:
            with haylen.RunSession() as session:
                session.show([("App", "Demo")], stop)
                stop.side_effect = lambda: session.process.terminate()
                session.follow([sys.executable, "-c", "import time; print('Ready.', flush=True); time.sleep(30)"], line=line)
        self.assertEqual(ended.exception.number, signal.SIGALRM)
        stop.assert_called_once_with()
        self.assertIsNotNone(session.process.poll(), "The app ends with the run.")

    def test_an_app_whose_connection_resets_leaves_and_the_server_goes_on(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        hello = json.dumps({"type": "hello", "session": server.session, "revision": server.revision}).encode()

        class Resetting(io.BytesIO):
            def read(self, size=-1):
                data = super().read(size)
                if not data:
                    raise ConnectionResetError(54, "Connection reset by peer")
                return data

        handler = mock.Mock(path=f"{haylen.DEVELOPMENT_PATH}?token={server.token}", headers={"Sec-WebSocket-Key": "dGhlIHNhbXBsZSBub25jZQ==", "Upgrade": "websocket"}, wfile=io.BytesIO(), rfile=Resetting(bytes([0x81, len(hello)]) + hello))
        output = io.StringIO()
        with mock.patch.object(haylen, "terminal", haylen.Terminal(output, output, {})):
            server.accept(handler)
        self.assertEqual(server.clients, [])
        self.assertIn("An app left the development server.", output.getvalue())

    def test_connections_need_the_token_and_receive_every_batch(self):
        server = haylen.DevelopmentServer(self.layout, self.manifest)
        handler = functools.partial(type("Handler", (haylen.WebHandler,), {"development": server, "log_message": lambda *arguments: None}), directory=self.scratch.name)
        web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        threading.Thread(target=web.serve_forever, daemon=True).start()
        self.addCleanup(web.shutdown)
        port = web.server_address[1]

        def connect(token):
            sock = socket.create_connection(("127.0.0.1", port), timeout=5)
            sock.sendall(f"GET {haylen.DEVELOPMENT_PATH}?token={token} HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n".encode())
            stream = sock.makefile("rb")
            self.addCleanup(sock.close)
            self.addCleanup(stream.close)
            return sock, stream, stream.readline().decode()

        _, _, status = connect("wrong")
        self.assertIn("403", status)

        sock, stream, status = connect(server.token)
        self.assertIn("101", status)
        while stream.readline() not in (b"\r\n", b""):
            pass
        hello = json.dumps({"type": "hello", "session": server.session, "revision": server.revision}).encode()
        mask = bytes([9, 8, 7, 6])
        sock.sendall(bytes([0x81, 0x80 | len(hello)]) + mask + bytes(byte ^ mask[index % 4] for index, byte in enumerate(hello)))
        for _ in range(100):
            if server.clients:
                break
            time.sleep(0.01)
        self.write("source/level.lua", "return {pushed = true}")
        server.broadcast(*server.scan())
        final, opcode, message = haylen.read_websocket_frame(stream)
        self.assertEqual((final, opcode), (True, haylen.WEBSOCKET_TEXT))
        self.assertEqual(json.loads(message)["files"], [{"path": "source/level.lua", "size": 22}])
        self.assertEqual(haylen.read_websocket_frame(stream), (True, haylen.WEBSOCKET_BINARY, b"return {pushed = true}"))

    def test_only_development_runs_tell_the_page_to_connect(self):
        site = Path(self.scratch.name) / "site"
        site.mkdir()
        haylen.package_folder(self.layout, site / "app.zip")
        (site / "config.json").write_text(json.dumps({"name": "App"}))
        app = mock.Mock(folder=self.folder, layout=self.layout)
        development = haylen.start_web_development(app, site)
        config = json.loads((site / "config.json").read_text())
        self.assertEqual(config["development"], {"path": haylen.DEVELOPMENT_PATH, "token": development.token})
        self.assertRegex(development.token, r"^[0-9a-f]{12}$")
        self.assertEqual(development.manifest, self.manifest)

    def test_device_runs_reach_the_server_by_their_platform(self):
        with mock.patch.object(haylen, "lan_address", return_value="192.168.1.20"):
            self.assertEqual(haylen.development_address("android", "127.0.0.1", 8000, "token"), ("127.0.0.1", f"ws://127.0.0.1:8000{haylen.DEVELOPMENT_PATH}?token=token"))
            self.assertEqual(haylen.development_address("ios-simulator", "127.0.0.1", 8000, "token")[1], f"ws://127.0.0.1:8000{haylen.DEVELOPMENT_PATH}?token=token")
            self.assertEqual(haylen.development_address("ios", "127.0.0.1", 9000, "token"), ("0.0.0.0", f"ws://192.168.1.20:9000{haylen.DEVELOPMENT_PATH}?token=token"))
            self.assertEqual(haylen.development_address("tvos", "10.0.0.5", 9000, "token"), ("10.0.0.5", f"ws://10.0.0.5:9000{haylen.DEVELOPMENT_PATH}?token=token"))
            self.assertEqual(haylen.development_address("macos", "0.0.0.0", 9000, "token")[1], f"ws://127.0.0.1:9000{haylen.DEVELOPMENT_PATH}?token=token")

    def test_a_device_run_on_a_machine_without_a_network_stops_with_what_to_do(self):
        probe = mock.MagicMock()
        probe.__enter__.return_value.connect.side_effect = OSError(51, "Network is unreachable")
        with mock.patch.object(haylen.socket, "socket", return_value=probe):
            with self.assertRaisesRegex(haylen.BuildError, 'no address on a network that devices reach: Network is unreachable. Join a network, or name the address with "--host".'):
                haylen.development_address("ios", "127.0.0.1", 9000, "token")

    def test_only_debug_launches_connect_to_a_server_that_knows_the_shipped_package(self):
        shipped = Path(self.scratch.name) / "shipped"
        shutil.copytree(self.folder, shipped)
        self.write("source/level.lua", "return {edited = true}")
        app = mock.Mock(folder=self.folder, layout=self.layout)
        with haylen.run_session(app, shipped, mock.Mock(config="Release", platform="ios-simulator")) as session:
            self.assertEqual(session.arguments, [])
            self.assertIsNone(session.development)

        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        options = mock.Mock(config="Debug", platform="ios-simulator", host="127.0.0.1", port=port, debugger_port=0)
        with haylen.run_session(app, shipped, options) as session:
            arguments = session.arguments
            self.assertEqual(arguments[0], "--dev-server")
            token = urllib.parse.parse_qs(urllib.parse.urlsplit(arguments[1]).query)["token"][0]
            with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
                sock.sendall(b"GET /app.json HTTP/1.1\r\nHost: localhost\r\n\r\n")
                self.assertIn(b"404", sock.recv(64))
            with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
                sock.sendall(f"GET {haylen.DEVELOPMENT_PATH}?token={token} HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n".encode())
                stream = sock.makefile("rb")
                self.assertIn(b"101", stream.readline())
                while stream.readline() not in (b"\r\n", b""):
                    pass
                hello = json.dumps({"type": "hello", "session": "", "revision": 0}).encode()
                mask = bytes([9, 8, 7, 6])
                sock.sendall(bytes([0x81, 0x80 | len(hello)]) + mask + bytes(byte ^ mask[index % 4] for index, byte in enumerate(hello)))
                _, _, message = haylen.read_websocket_frame(stream)
                self.assertEqual([file["path"] for file in json.loads(message)["files"]], ["source/level.lua"])
                stream.close()



class DebugRelayTest(unittest.TestCase):
    class App:
        """An app connected to the development server, which keeps the messages of the debugger that the relay sends it."""

        def __init__(self):
            self.messages = []

        def send(self, opcode, payload):
            self.messages.append(json.loads(payload)["message"])
            return True

    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.folder = Path(self.scratch.name).resolve() / "app"
        (self.folder / "source").mkdir(parents=True)
        quiet = mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {}))
        quiet.start()
        self.addCleanup(quiet.stop)

    def wait(self, condition):
        deadline = time.monotonic() + 5
        while not condition():
            self.assertLess(time.monotonic(), deadline, "The relay did not get there in time.")
            time.sleep(0.01)

    def test_messages_keep_their_framing_and_source_paths_turn_into_package_paths_and_back(self):
        message = {"seq": 1, "type": "request", "command": "setBreakpoints", "arguments": {"source": {"path": "/app/source/main.lua"}}}
        self.assertEqual(haylen.read_debug_message(io.BytesIO(haylen.debug_message_frame(message))), message)
        self.assertIsNone(haylen.read_debug_message(io.BytesIO(b"Content-Length: 10\r\n\r\n{}")))
        frames = {"body": {"stackFrames": [{"source": {"name": "main.lua", "path": "source/main.lua"}}, {"name": "[C]"}]}}
        self.assertEqual(haylen.debug_source_paths(frames, lambda path: "/app/" + path)["body"]["stackFrames"][0]["source"]["path"], "/app/source/main.lua")

    def test_the_relay_holds_requests_until_an_app_connects_and_configures_every_app_that_connects_later(self):
        relay = haylen.DebugRelay(haylen.PackageLayout(self.folder, {}), 0)
        relay.start()
        self.addCleanup(relay.close)
        editor = socket.create_connection(("127.0.0.1", relay.port), timeout=5)
        self.addCleanup(editor.close)
        stream = editor.makefile("rb")
        self.addCleanup(stream.close)
        script = str(self.folder / "source" / "main.lua")
        editor.sendall(haylen.debug_message_frame({"seq": 1, "type": "request", "command": "initialize", "arguments": {}}))
        editor.sendall(haylen.debug_message_frame({"seq": 2, "type": "request", "command": "setBreakpoints", "arguments": {"source": {"path": script}, "breakpoints": [{"line": 3}]}}))
        self.wait(lambda: len(relay.pending) == 2)

        # The first app receives what the editor sent, with package paths, and its answers reach the editor with the paths of this machine.
        first = self.App()
        relay.app_connected(first)
        self.assertEqual([message["command"] for message in first.messages], ["initialize", "setBreakpoints"])
        self.assertEqual(first.messages[1]["arguments"]["source"]["path"], "source/main.lua")
        relay.from_app(first, {"seq": 1, "type": "response", "request_seq": 2, "success": True, "command": "setBreakpoints", "body": {"breakpoints": [{"verified": True, "source": {"path": "source/main.lua"}}]}})
        relay.from_app(first, {"seq": 2, "type": "event", "event": "initialized"})
        self.assertEqual(haylen.read_debug_message(stream)["body"]["breakpoints"][0]["source"]["path"], script)
        self.assertEqual(haylen.read_debug_message(stream)["event"], "initialized")

        # An app that connects later, such as one that restarted, gets the configuration again, and the editor hears none of its answers to it.
        relay.app_left(first)
        second = self.App()
        relay.app_connected(second)
        self.assertEqual([message["command"] for message in second.messages], ["initialize", "setBreakpoints"])
        self.assertTrue(all(message["seq"] > haylen.DEBUGGER_REPLAY_SEQUENCE for message in second.messages))
        relay.from_app(second, {"seq": 1, "type": "response", "request_seq": second.messages[1]["seq"], "success": True, "command": "setBreakpoints"})
        relay.from_app(second, {"seq": 2, "type": "event", "event": "initialized"})
        relay.from_app(first, {"seq": 3, "type": "event", "event": "output", "body": {"output": "From an app that left."}})
        relay.from_app(second, {"seq": 3, "type": "event", "event": "stopped", "body": {"reason": "breakpoint"}})
        self.assertEqual(haylen.read_debug_message(stream)["event"], "stopped")

        # An editor that leaves lets the app forget it, and a second editor waits for the first to leave.
        intruder = socket.create_connection(("127.0.0.1", relay.port), timeout=5)
        self.assertEqual(intruder.recv(1), b"")
        intruder.close()
        stream.close()
        editor.close()
        self.wait(lambda: second.messages[-1]["command"] == "disconnect")
        self.assertEqual(relay.configuration, {})


class PackageServerTest(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        quiet = mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {}))
        quiet.start()
        self.addCleanup(quiet.stop)

    def test_the_zip_needs_the_token_and_names_the_development_server(self):
        folder = Path(self.scratch.name) / "app"
        (folder / "source").mkdir(parents=True)
        (folder / "app.json").write_text("{}")
        (folder / "source" / "main.lua").write_text("print(1)")
        archive = Path(self.scratch.name) / "app.zip"
        haylen.package_folder(haylen.PackageLayout(folder, {}), archive)
        development = haylen.DevelopmentServer(haylen.PackageLayout(folder, {}), {})
        handler = type("Handler", (haylen.PackageHandler,), {"development": development, "archive": archive, "development_address": "ws://192.168.1.20:8000/haylen/development?token=0123456789ab"})
        server = haylen.LocalServer(("127.0.0.1", 0), handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)

        def get(path):
            connection = http.client.HTTPConnection("127.0.0.1", server.server_address[1], timeout=5)
            self.addCleanup(connection.close)
            connection.request("GET", path)
            response = connection.getresponse()
            return response, response.read()

        response, body = get(f"/haylen/{development.token}/app.zip")
        self.assertEqual(response.status, 200)
        self.assertEqual(body, archive.read_bytes())
        self.assertEqual(response.getheader(haylen.DEVELOPMENT_HEADER), "ws://192.168.1.20:8000/haylen/development?token=0123456789ab")
        self.assertEqual(response.getheader("Access-Control-Expose-Headers"), haylen.DEVELOPMENT_HEADER)
        self.assertEqual(get("/haylen/0123456789ab/app.zip")[0].status, 403)
        self.assertEqual(get(f"/haylen/app.zip?token={development.token}")[0].status, 404)
        self.assertEqual(get("/app.json")[0].status, 404)

    def test_only_the_package_of_a_lua_app_is_served(self):
        folder = Path(self.scratch.name) / "app"
        folder.mkdir()
        options = argparse.Namespace(open=False, coep="off", coop="off", host="127.0.0.1", port=0)
        with self.assertRaisesRegex(haylen.BuildError, '"--open", "--coep" and "--coop"'):
            haylen.serve_app(folder, argparse.Namespace(**{**vars(options), "open": True}))
        (folder / "CMakeLists.txt").write_text("")
        with self.assertRaisesRegex(haylen.BuildError, "C\\+\\+ app"):
            haylen.serve_app(folder, options)


if __name__ == "__main__":
    unittest.main()
