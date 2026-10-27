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
import shutil
import socket
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import tokenize
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
        app = mock.Mock(identifier="com.haylen.music", version="1.2.0")
        app.name = "Music"
        apple = {"entitlements": {"com.apple.security.application-groups": ["group.$(HAYLEN_BUNDLE_IDENTIFIER)"]}, "infoPlist": {"Build": "$(MARKETING_VERSION)", "Other": "$(SRCROOT)/file"}}
        self.assertEqual(haylen.expand_app_settings(apple, haylen.apple_app_settings(app)), {"entitlements": {"com.apple.security.application-groups": ["group.com.haylen.music"]}, "infoPlist": {"Build": "1.2.0", "Other": "$(SRCROOT)/file"}})

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
        self.assertEqual(lines[0], "\x1b[1;33mWarning:\x1b[0m The plugin \x1b[36mdemo\x1b[0m needs \x1b[36mandroid.permission.CAMERA\x1b[0m.")
        self.assertEqual(lines[1], "  Add to \x1b[4mapp/src/main/AndroidManifest.xml\x1b[0m:")
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


class PropertiesTest(unittest.TestCase):
    def test_values_survive_as_ascii_with_escapes(self):
        self.assertEqual(haylen.java_property("Ilha Tropical ✓\\n"), "Ilha Tropical \\u2713\\\\n")


class StoryboardTest(unittest.TestCase):
    def test_storyboards_of_apple_tv_are_documents_of_apple_tv(self):
        # The interface compiler of tvOS fails without a message on a storyboard of the iOS document type.
        for path in [*haylen.ENGINE_DIR.glob("platform/apple/**/*.storyboard"), *haylen.TEMPLATES_DIR.glob("**/*.storyboard")]:
            text = path.read_text(encoding="utf-8")
            self.assertEqual('targetRuntime="AppleTV"' in text, 'type="com.apple.InterfaceBuilder.AppleTV.Storyboard"' in text, path)


class TextFileTest(unittest.TestCase):
    def test_every_text_file_is_read_and_written_as_utf_8(self):
        # Windows reads and writes text in its own code page by default, which turns the copyright sign of `app.json` into a byte that no UTF-8 tool reads.
        tree = ast.parse(Path(haylen.__file__).read_text(encoding="utf-8"))
        calls = [node for node in ast.walk(tree) if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr in ("read_text", "write_text")]
        self.assertTrue(calls)
        self.assertEqual([node.lineno for node in calls if not any(keyword.arg == "encoding" for keyword in node.keywords)], [])


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
            # Android resource names allow only letters, digits and underscores, and Python imports modules and discovers tests by names that are identifiers.
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
        """Yields the numbered lines of a page outside its code blocks."""
        inside = False
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
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
        # The main page opens with the logo, and the features of the plugin template are parts of the page of a new plugin.
        wrong = []
        for path in self.pages():
            name = path.relative_to(self.root).as_posix()
            if name == "README.md" or name.startswith("templates/plugin-features/"):
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

    def test_with_color_reserved_expressions_and_paths_take_their_styles_and_the_line_keeps_its_own(self):
        rendered = haylen.Terminal.render('Run "python3 haylen.py run" in `apps/demo`.', True, haylen.Terminal.SUCCESS)
        self.assertEqual(rendered, "\x1b[32mRun \x1b[36mpython3 haylen.py run\x1b[0m\x1b[32m in \x1b[4mapps/demo\x1b[0m\x1b[32m.\x1b[0m")

    def test_an_empty_quote_stays_as_it_is(self):
        self.assertEqual(haylen.Terminal.render('The key "name" is "".', True), 'The key \x1b[36mname\x1b[0m is "".')

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
            self.assertEqual(manifest["platforms"], ["ios", "catalyst", "tvos", "macos", "android", "web", "windows", "linux"])
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
            self.assertEqual(manifest["parameters"]["macDevices"]["platforms"], ["macos"])
            self.assertEqual(set(manifest["apple"]["entitlements"].values()), {"${macDevices}"})

            # The shared parts come once, before the features that need them, and the Android hooks hold the lines of every feature.
            kotlin = (folder / "android" / "src" / "main" / "kotlin" / "com" / "haylen" / "plugins" / "devicekit" / "DeviceKitPlugin.kt").read_text()
            imports = [line for line in kotlin.splitlines() if line.startswith("import ")]
            self.assertEqual(imports, sorted(imports))
            self.assertIn("import android.os.Bundle", imports)
            self.assertEqual(kotlin.count("private val permissions = DeviceKitPermissions()"), 1)
            self.assertIn("    override fun onActivityDestroyed(activity: HaylenActivity) {\n        permissions.unregister()\n        banner.activityDestroyed()\n    }", kotlin)
            self.assertTrue((folder / "android" / "src" / "main" / "kotlin" / "com" / "haylen" / "plugins" / "devicekit" / "DeviceKitPermissions.kt").is_file())
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


class AndroidGpuTestsTest(unittest.TestCase):
    def test_the_tests_build_alone_for_the_abi_and_run_from_the_folder_the_device_holds_them_in(self):
        with mock.patch.object(haylen, "ensure_shaderc", return_value=Path("/tools/haylen-shaderc")), mock.patch.object(haylen, "android_ndk", return_value=Path("/sdk/ndk")):
            command = [str(part) for part in haylen.android_gpu_tests_configure_command(Path("/build/android"), "x86_64", "Debug")]
        self.assertIn("-DANDROID_ABI=x86_64", command)
        self.assertIn("-DHAYLEN_BUILD_TESTS=ON", command)
        self.assertIn("-DHAYLEN_BUILD_PLAYER=OFF", command)
        self.assertIn(f"-DHAYLEN_ANDROID_GPU_TESTS_DIR={haylen.ANDROID_GPU_TESTS_DIR}", command)

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
            self.assertIn("quit before it wrote its results", haylen.apple_device_gpu_tests_problem(results))
            results.write_text('<testsuites tests="40" failures="2" errors="1"></testsuites>', encoding="utf-8")
            self.assertEqual(haylen.apple_device_gpu_tests_problem(results), "3 of the 40 GPU tests failed on the device.")
            results.write_text('<testsuites tests="40" failures="0" errors="0"></testsuites>', encoding="utf-8")
            self.assertIsNone(haylen.apple_device_gpu_tests_problem(results))

    def test_the_devices_take_only_the_gpu_tests_and_a_named_device(self):
        for platform_name in ("ios", "tvos", "visionos"):
            with self.assertRaisesRegex(haylen.BuildError, "only the GPU tests"):
                haylen.command_test(haylen.build_parser().parse_args(["test", "--platform", platform_name]))
            with mock.patch.object(haylen, "require_host"), self.assertRaisesRegex(haylen.BuildError, "Name the device"):
                haylen.command_test(haylen.build_parser().parse_args(["test", "--gpu", "--platform", platform_name]))


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
        self.assertNotIn("visionos", haylen.CPP_RUN_PLATFORMS)

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
        args = haylen.build_parser().parse_args(["run-cpp", "project", "--platform", "ios-simulator"])
        simulator = {"udid": "id", "name": "Phone", "state": "Booted"}
        with mock.patch.object(haylen, "require_host"), mock.patch.object(haylen, "run"), mock.patch.object(haylen, "ensure_shaderc", return_value="shaderc"), mock.patch.object(haylen, "App"), mock.patch.object(haylen, "write_cpp_apple_plugins", return_value=False), mock.patch.object(haylen, "apple_simulator", return_value=simulator), mock.patch.object(haylen, "launch_apple", autospec=True) as launch:
            haylen.run_cpp_apple(self.scratch / "project", "game", self.scratch, args)
        self.assertEqual(launch.call_args.args[2:], (simulator, []))

    def test_the_apple_parts_of_plugins_reach_the_cmake_project_of_cpp_apps(self):
        plugin = self.scratch / "plugins" / "camera"
        (plugin / "apple").mkdir(parents=True)
        (plugin / "apple" / "CameraPlugin.swift").write_text("")
        (plugin / "apple" / "notes.txt").write_text("")
        (plugin / "camera.bundle").mkdir()
        manifest = {"id": "camera", "version": "1.0.0", "platforms": ["ios"], "parameters": {}, "apple": {"class": "CameraPlugin", "sources": "apple", "frameworks": ["AVFoundation.framework"], "resources": ["camera.bundle"], "infoPlist": {"NSCameraUsageDescription": "Shows the camera."}}}
        app = mock.Mock(plugins=[haylen.Plugin(plugin, manifest, None)], plugin_values={"camera": {}})
        fragment = self.scratch / "build" / haylen.CPP_PLUGINS_FILE
        fragment.parent.mkdir()
        self.assertTrue(haylen.write_cpp_apple_plugins(app, "ios-simulator", fragment.parent, fragment))
        text = fragment.read_text()
        self.assertIn(f"set(HAYLEN_PLUGIN_SOURCES [==[{(fragment.parent / 'haylen-plugins' / 'camera' / 'sources' / 'CameraPlugin.swift').as_posix()}]==])", text)
        self.assertIn("set(HAYLEN_PLUGIN_FRAMEWORKS [==[AVFoundation]==])", text)
        self.assertIn("<key>NSCameraUsageDescription</key>", text)
        self.assertTrue((fragment.parent / "haylen-plugins" / "camera" / "resources" / "camera.bundle").is_dir())
        self.assertFalse(haylen.write_cpp_apple_plugins(app, "ios-simulator", fragment.parent, fragment))

        manifest["apple"]["packages"] = [{"url": "https://example.com/camera.git", "version": "1.0.0", "products": ["Camera"]}]
        with self.assertRaisesRegex(haylen.BuildError, 'The plugin "camera" needs "packages" on Apple platforms'):
            haylen.write_cpp_apple_plugins(app, "ios-simulator", fragment.parent, fragment)

    def test_cpp_apps_launch_on_android_with_an_empty_command_line(self):
        built = self.scratch / "android-arm64-v8a-debug" / "bin" / "game"
        built.mkdir(parents=True)
        (built / "package.txt").write_text(str(self.scratch / "package"))
        args = haylen.build_parser().parse_args(["run-cpp", "project", "--platform", "android"])
        patches = [
            mock.patch.object(haylen, "android_device", return_value="emulator-5554"),
            mock.patch.object(haylen, "capture", return_value="arm64-v8a"),
            mock.patch.object(haylen, "run"),
            mock.patch.object(haylen, "ensure_shaderc", return_value="shaderc"),
            mock.patch.object(haylen, "android_options", return_value=[]),
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
        self.assertEqual(mocks[-1].call_args.args[4:], ("emulator-5554", []))


    def test_a_build_only_run_builds_the_app_of_this_machine_without_launching_it(self):
        (self.scratch / "project").mkdir()
        (self.scratch / "project" / "CMakeLists.txt").write_text("")
        executable = self.scratch / "app"
        for options, launched in (([], True), (["--build-only"], False)):
            args = haylen.build_parser().parse_args(["run-cpp", str(self.scratch / "project"), *options])
            with mock.patch.object(haylen, "run") as run, mock.patch.object(haylen, "ensure_shaderc", return_value="shaderc"), mock.patch.object(haylen, "cmake_app_executable", return_value=executable), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
                haylen.command_run_cpp(args)
            self.assertEqual([executable] in [call.args[0] for call in run.call_args_list], launched)
            self.assertIn(f"-DHAYLEN_ENGINE_SOURCE={haylen.ENGINE_DIR.as_posix()}", run.call_args_list[0].args[0])
            self.assertIn("--target", run.call_args_list[1].args[0])
            self.assertEqual(run.call_args_list[1].args[0][run.call_args_list[1].args[0].index("--target") + 1], "main")


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

    def test_the_release_binary_of_the_machine_downloads_once_into_a_folder_of_its_version(self):
        executable = haylen.executable_name("haylen-shaderc")
        url, digest = self.archive("haylen-shaderc-9.9.9-host.zip" if haylen.host_name() == "windows" else "haylen-shaderc-9.9.9-host.tar.gz", {executable: "compiler", "LICENSE": "license"})
        pins = {"version": "9.9.9", "commit": "abcdef0123456789", "source": {"url": "file:///missing.tar.gz", "sha256": "0" * 64}, "binaries": {self.host(): {"url": url, "sha256": digest}}}
        run, compiler = self.provide(pins)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(compiler, self.scratch / "cache" / "haylen-shaderc-9.9.9" / executable)
        self.assertEqual(compiler.read_text(), "compiler")
        self.assertTrue(os.access(compiler, os.X_OK))
        self.assertEqual([path.name for path in compiler.parent.iterdir()], [executable])

        (self.scratch / "server").rename(self.scratch / "gone")
        run, again = self.provide(pins)
        self.assertEqual((run.returncode, again), (0, compiler))

    def test_a_download_that_differs_from_its_digest_fails_and_keeps_nothing(self):
        url, digest = self.archive("haylen-shaderc-9.9.9-host.tar.gz", {"haylen-shaderc": "compiler"})
        pins = {"version": "9.9.9", "commit": "abcdef0123456789", "source": {"url": url, "sha256": digest}, "binaries": {self.host(): {"url": url, "sha256": "0" * 64}}}
        run, compiler = self.provide(pins)
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("haylen-shaderc-9.9.9-host.tar.gz", run.stderr)
        self.assertIsNone(compiler)
        self.assertEqual(list((self.scratch / "cache" / "haylen-shaderc-9.9.9").iterdir()), [])

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
            self.assertEqual(haylen.ensure_shaderc(), self.scratch / "tools" / "haylen-shaderc-9.9.9" / executable)

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


class AndroidKeyTest(unittest.TestCase):
    def setUp(self):
        self.app = Path(tempfile.mkdtemp()).resolve()
        self.addCleanup(shutil.rmtree, self.app)
        (self.app / "app.json").write_text("{}")

    def arguments(self, *extra: str):
        return haylen.build_parser().parse_args(["android-key", str(self.app), *extra])

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
            haylen.command_android_key(self.arguments("--password", "short"))
        self.assertFalse((self.app / "platform").exists())

    def test_an_existing_key_stays_unless_forced(self):
        keystore = self.app / "platform" / "android" / "keystore" / "release.jks"
        keystore.parent.mkdir(parents=True)
        keystore.write_bytes(b"key")
        with self.assertRaisesRegex(haylen.BuildError, 'already has a release key.*"--force"'):
            haylen.command_android_key(self.arguments())
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
        self.assertIn('Run it with "haylen run-cpp', self.output.getvalue())
        self.assertEqual(json.loads((self.scratch / "cpp-app" / "app.json").read_text())["name"], "Cpp App")

    def test_the_commands_of_platform_projects_send_a_cpp_app_to_run_cpp(self):
        app = self.new_app("game", "--kind", "cpp")
        for arguments in (["run", str(app)], ["prepare", str(app), "--platform", "web"], ["dist", str(app), "--platform", "linux"], ["xcodegen", str(app)]):
            with self.assertRaisesRegex(haylen.BuildError, 'is a C\\+\\+ app, whose CMake project builds it for every platform. Run it with "haylen run-cpp'):
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
        arguments = ["steam", str(app), "--platform", haylen.host_name(), "--app-id", "7", "--depot", f"{haylen.host_name()}=8", "--depot", "chapter-2=9"]
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
            parsed = haylen.build_parser().parse_args(["steam", str(app), "--platform", other, "--app-id", "7", "--depot", f"{other}=8"])
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
        haylen.command_platform_diff(haylen.build_parser().parse_args(["platform", "diff", str(app), "--template", "android"]))
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
        self.assertIn("#define HAYLEN_APP_VERSION_NUMBERS 1, 2, 0, 0", header)

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
            "The application manifest of the built executable lacks the Common Controls of version 6, which the message dialogs of Haylen need.",
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
        self.assertEqual(written, {"plugins": [{"id": "accounts", "version": "2.0.0", "config": {"server": "accounts.example.com"}}, {"id": "store", "version": "1.1.0", "config": {"sandbox": True}}]})


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
        manifest["apple"]["extensions"] = [{**extension, "name": "downloads", "type": "widget", "minimumVersion": "latest"}, {"name": "Other"}]
        manifest["apple"]["assetPacks"] = {"prefetch": 3}
        self.assertEqual(haylen.PluginManifestCheck.problems_of(folder, manifest), [
            'The key "apple.extensions[0].name" must be a name of letters and digits that starts with a capital letter, such as "Downloads".',
            'The key "apple.extensions[0].type" must be "extensionKit", "appExtension".',
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
        assets = haylen.Plugin(scratch, {"id": "assets", "version": "1.0.0", "platforms": ["ios", "tvos"], "apple": {"assetPacks": {"prefetch": "${prefetchPacks}"}}}, None)
        app = mock.Mock(plugins=[assets], plugin_values={"assets": {"prefetchPacks": ["intro.music"]}}, content_packs=["chapter-2", "intro.music"], build_folder=scratch)
        manifests = {}
        def package(command, **_):
            manifests[Path(command[3]).stem] = json.loads(Path(command[3]).read_text())
            Path(command[-1]).write_bytes(b"archive")
        with mock.patch.object(haylen, "run", side_effect=package), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            archives = haylen.write_apple_asset_packs(app)
        self.assertEqual([path.name for path in archives], ["chapter-2.aar", "intro.music.aar"])
        self.assertEqual(manifests["chapter-2"]["downloadPolicy"], {"onDemand": {}})
        self.assertEqual(manifests["intro.music"]["downloadPolicy"], {"prefetch": {"installationEventTypes": ["firstInstallation", "subsequentUpdate"]}})
        self.assertEqual(manifests["intro.music"]["fileSelectors"], [{"directorySource": "intro.music", "directoryDestination": "intro.music"}])
        self.assertEqual(manifests["intro.music"]["platforms"], ["iOS", "tvOS"])
        self.assertEqual((haylen.apple_asset_packs_folder(app) / manifests["intro.music"]["sourceRoot"]).resolve(), scratch / "release" / "apple.packs")
        self.assertEqual(sorted(path.name for path in haylen.apple_asset_packs_folder(app).iterdir()), ["chapter-2.aar", "intro.music.aar"], "The manifests of the tool leave once the archives exist.")
        app.plugin_values["assets"]["prefetchPacks"] = ["chapter-3"]
        with self.assertRaisesRegex(haylen.BuildError, 'the packs "chapter-3", which "content.delivery"'):
            haylen.write_apple_asset_packs(app)

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
        app = mock.Mock(folder=scratch / "game", build_folder=scratch / "build", version_code=7, content_server=None, content_packs=[])
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
        self.manifest = haylen.DevelopmentServer.hash_files(self.folder, ["app.json", "source/main.lua", "source/level.lua", "content/data.json"])
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
        server = haylen.DevelopmentServer(self.folder, self.manifest)
        self.write("source/level.lua", "return {edited = true}")
        self.write("source/.level.lua.swp", "swap")
        self.write("content/new.json", "[]")
        (self.folder / "content" / "data.json").unlink()
        self.assertEqual(server.scan(), (["content/new.json", "source/level.lua"], ["content/data.json"]))
        self.assertEqual(server.scan(), ([], []))

    def test_late_apps_catch_up(self):
        server = haylen.DevelopmentServer(self.folder, self.manifest)
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

    def test_console_lines_reach_every_app_and_their_results_print(self):
        server = haylen.DevelopmentServer(self.folder, self.manifest)
        sent = []
        client = mock.Mock(send=lambda opcode, payload: sent.append((opcode, json.loads(payload))) or True)
        self.assertEqual(server.send_line("gold 50"), 0)
        server.clients.append(client)
        server.read_lines(io.StringIO("gold 50\n\n  help  \n"))
        self.assertEqual(sent, [(haylen.WEBSOCKET_TEXT, {"type": "console", "id": 2, "line": "gold 50"}), (haylen.WEBSOCKET_TEXT, {"type": "console", "id": 3, "line": "help"})])
        output = io.StringIO()
        with mock.patch.object(haylen, "terminal", haylen.Terminal(output, output, {})):
            server.receive(client, {"type": "consoleResult", "id": 2, "ok": True, "output": "Gold 50"})
            server.receive(client, {"type": "consoleResult", "id": 3, "ok": False, "output": "There is no command."})
        self.assertIn("Gold 50", output.getvalue())
        self.assertIn("There is no command.", output.getvalue())

    def test_the_console_keys_send_what_changed_and_restart_the_apps(self):
        server = haylen.DevelopmentServer(self.folder, self.manifest)
        sent = []
        client = mock.Mock(send=lambda opcode, payload: sent.append(json.loads(payload)) or True, send_batch=mock.Mock(return_value=True))
        server.clients.append(client)
        output = io.StringIO()
        with mock.patch.object(haylen, "terminal", haylen.Terminal(output, output, {})):
            self.write("source/level.lua", "return {edited = true}")
            server.read_lines(io.StringIO("r\nR\nr\n"))
        self.assertEqual([file["path"] for file in client.send_batch.call_args.args[0]["files"]], ["source/level.lua"])
        self.assertEqual(sent, [{"type": "restart"}])
        self.assertIn("Nothing changed since the last files the apps received.", output.getvalue())

    def test_an_app_whose_connection_resets_leaves_and_the_server_goes_on(self):
        server = haylen.DevelopmentServer(self.folder, self.manifest)
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
        server = haylen.DevelopmentServer(self.folder, self.manifest)
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
        haylen.package_folder(self.folder, site / "app.zip")
        (site / "config.json").write_text(json.dumps({"name": "App"}))
        app = mock.Mock(folder=self.folder)
        development = haylen.start_web_development(app, site)
        config = json.loads((site / "config.json").read_text())
        self.assertEqual(config["development"], {"path": haylen.DEVELOPMENT_PATH, "token": development.token})
        self.assertGreaterEqual(len(base64.urlsafe_b64decode(development.token)), 16)
        self.assertEqual(development.manifest, self.manifest)

    def test_device_runs_reach_the_server_by_their_platform(self):
        with mock.patch.object(haylen, "lan_address", return_value="192.168.1.20"):
            self.assertEqual(haylen.development_address("android", "127.0.0.1", 8000, "token"), ("127.0.0.1", f"ws://127.0.0.1:8000{haylen.DEVELOPMENT_PATH}?token=token"))
            self.assertEqual(haylen.development_address("ios-simulator", "127.0.0.1", 8000, "token")[1], f"ws://127.0.0.1:8000{haylen.DEVELOPMENT_PATH}?token=token")
            self.assertEqual(haylen.development_address("ios", "127.0.0.1", 9000, "token"), ("0.0.0.0", f"ws://192.168.1.20:9000{haylen.DEVELOPMENT_PATH}?token=token"))
            self.assertEqual(haylen.development_address("tvos", "10.0.0.5", 9000, "token"), ("10.0.0.5", f"ws://10.0.0.5:9000{haylen.DEVELOPMENT_PATH}?token=token"))
            self.assertEqual(haylen.development_address("macos", "0.0.0.0", 9000, "token")[1], f"ws://127.0.0.1:9000{haylen.DEVELOPMENT_PATH}?token=token")

    def test_only_debug_launches_connect_to_a_server_that_knows_the_shipped_package(self):
        shipped = Path(self.scratch.name) / "shipped"
        shutil.copytree(self.folder, shipped)
        self.write("source/level.lua", "return {edited = true}")
        app = mock.Mock(folder=self.folder)
        with haylen.device_development(app, shipped, mock.Mock(config="Release", platform="ios-simulator")) as arguments:
            self.assertEqual(arguments, [])

        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        options = mock.Mock(config="Debug", platform="ios-simulator", host="127.0.0.1", port=port)
        with haylen.device_development(app, shipped, options) as arguments:
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


if __name__ == "__main__":
    unittest.main()
