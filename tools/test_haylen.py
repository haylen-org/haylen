"""Tests of the rules of haylen.py that need no build: how it merges Info.plist keys, entitlements and privacy manifests, what it checks in a built app, when it generates App.xcodeproj again, how it prints to the terminal and how its commands read their arguments."""

import argparse
import ast
import base64
import dataclasses
import contextlib
import functools
import http.client
import importlib.util
import http.server
import io
import json
import os
import plistlib
import re
import shutil
import subprocess
import socket
import sys
import tempfile
import threading
import time
import tokenize
import unittest
import urllib.parse
import zipfile
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


class RequirementTest(unittest.TestCase):
    def test_a_built_value_holds_the_items_and_keys_it_needs(self):
        built = {"CFBundleURLTypes": [{"CFBundleURLName": "other"}, {"CFBundleURLName": "demo", "CFBundleURLSchemes": ["demo", "more"]}], "NSCameraUsageDescription": "Reworded", "com.apple.developer.game-center": True}
        self.assertTrue(haylen.holds(built, {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["demo"]}]}))
        self.assertTrue(haylen.holds(built, {"NSCameraUsageDescription": "The text of the plugin"}))
        self.assertFalse(haylen.holds(built, {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["missing"]}]}))
        self.assertFalse(haylen.holds(built, {"NSMicrophoneUsageDescription": "Needed"}))
        self.assertFalse(haylen.holds({"com.apple.developer.game-center": False}, {"com.apple.developer.game-center": True}))

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

            # The desktop library and its init function take the id with underscores, the name that "native.load" finds.
            source = (folder / "native" / "CameraScanner.c").read_text()
            self.assertIn("CAMERA_SCANNER_EXPORT int camera_scanner_haylen_init(const HaylenNativeApi* api)", source)
            self.assertIn('api->registerHandler("camera-scanner.echo"', source)
            self.assertIn("add_library(camera_scanner CameraScanner.c)", (folder / "native" / "CMakeLists.txt").read_text())
            self.assertIn("native.load('camera_scanner', {init = 'camera_scanner_haylen_init'})", (folder / "source" / "init.lua").read_text())
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

    def test_every_f_string_keeps_its_quote_out_of_its_fields_so_an_older_python_reads_the_file(self):
        source = (haylen.ROOT / "haylen.py").read_text(encoding="utf-8")
        offsets = [0]
        for line in source.splitlines(keepends=True):
            offsets.append(offsets[-1] + len(line))
        depth, start, quote = 0, 0, ""
        with tokenize.open(haylen.ROOT / "haylen.py") as file:
            for token in tokenize.generate_tokens(file.readline):
                if token.type == tokenize.FSTRING_START:
                    if depth == 0:
                        quote = token.string.lstrip("rRfFbB")
                        start = offsets[token.start[0] - 1] + token.start[1] + len(token.string)
                    depth += 1
                elif token.type == tokenize.FSTRING_END:
                    depth -= 1
                    if depth == 0:
                        fields = re.sub(r"\\.", "", source[start : offsets[token.start[0] - 1] + token.start[1]])
                        self.assertNotIn(quote, fields, f"The f-string on line {token.start[0]} uses its own quote inside, which only Python 3.12 reads.")

class NativeLibrariesTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.library = haylen.NativeLibrary("native_test_static", {}, self.scratch, ("ios", "tvos"), True, ("native_test_add",))
        self.app = mock.Mock(native=[self.library], build_folder=self.scratch / "build")

    def prepare(self, run_platform):
        archive = self.scratch / "libnative_test_static.a"
        archive.write_bytes(b"")
        with mock.patch.object(haylen, "build_apple_native", return_value=archive):
            return haylen.prepare_apple_native(self.app, self.scratch / "generated", run_platform, 1)

    def test_static_libraries_link_only_into_the_mac_catalyst_build_of_the_ios_target(self):
        settings = self.prepare("catalyst")
        self.assertEqual(settings, ['HAYLEN_NATIVE_LDFLAGS_MACCATALYST_YES = "$(PROJECT_DIR)/haylen/native/ios-macosx/libnative_test_static.a"', "HAYLEN_NATIVE_LDFLAGS[sdk=macosx*] = $(HAYLEN_NATIVE_LDFLAGS_MACCATALYST_$(IS_MACCATALYST))"])
        self.assertTrue((self.scratch / "generated" / "native" / "ios-macosx" / "libnative_test_static.a").is_file())
        self.assertEqual(self.prepare("ios-simulator"), ['HAYLEN_NATIVE_LDFLAGS[sdk=iphonesimulator*] = "$(PROJECT_DIR)/haylen/native/ios-iphonesimulator/libnative_test_static.a"'])

    def test_the_symbols_of_ios_libraries_register_on_mac_catalyst_too(self):
        self.assertTrue(haylen.write_apple_native_symbols(self.app, self.scratch))
        table = (self.scratch / "HaylenNativeSymbols.mm").read_text()
        self.assertIn("#if TARGET_OS_IOS || TARGET_OS_TV\n", table)
        self.assertIn('registerLinked("native_test_static", {{"native_test_add", reinterpret_cast<void*>(&native_test_add)}});', table)


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
        with mock.patch.object(haylen, "require_host"), mock.patch.object(haylen, "run"), mock.patch.object(haylen, "ensure_shdc", return_value="shdc"), mock.patch.object(haylen, "App"), mock.patch.object(haylen, "write_cpp_apple_plugins", return_value=False), mock.patch.object(haylen, "apple_simulator", return_value=simulator), mock.patch.object(haylen, "launch_apple", autospec=True) as launch:
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
            mock.patch.object(haylen, "ensure_shdc", return_value="shdc"),
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
        self.assertEqual(mocks[-1].call_args.args[2:], ("emulator-5554", []))


    def test_a_build_only_run_builds_the_app_of_this_machine_without_launching_it(self):
        (self.scratch / "project").mkdir()
        (self.scratch / "project" / "CMakeLists.txt").write_text("")
        executable = self.scratch / "app"
        for options, launched in (([], True), (["--build-only"], False)):
            args = haylen.build_parser().parse_args(["run-cpp", str(self.scratch / "project"), *options])
            with mock.patch.object(haylen, "run") as run, mock.patch.object(haylen, "ensure_shdc", return_value="shdc"), mock.patch.object(haylen, "cmake_app_executable", return_value=executable), mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
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


class ShaderCompilerTest(unittest.TestCase):
    def setUp(self):
        self.scratch = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)

    def test_a_new_pin_of_the_shader_compiler_configures_the_build_tree_again(self):
        compiler = self.scratch / "tools" / "sokol-shdc-new" / "sokol-shdc"
        tree = self.scratch / "build" / "macos-debug"
        tree.mkdir(parents=True)
        arguments = argparse.Namespace(platform="macos", config="Debug", backend=None, sanitizers=None)
        with mock.patch.object(haylen, "BUILD_ROOT", self.scratch / "build"), mock.patch.object(haylen, "ensure_shdc", return_value=compiler), mock.patch.object(haylen, "command_configure") as configure:
            (tree / "CMakeCache.txt").write_text(f"HAYLEN_RENDER_BACKEND:STRING=AUTO\nHAYLEN_SOKOL_SHDC:FILEPATH={(self.scratch / 'tools' / 'sokol-shdc').as_posix()}\n")
            haylen.ensure_configured(arguments)
            self.assertEqual(configure.call_count, 1)
            (tree / "CMakeCache.txt").write_text(f"HAYLEN_RENDER_BACKEND:STRING=AUTO\nHAYLEN_SOKOL_SHDC:FILEPATH={compiler.as_posix()}\n")
            haylen.ensure_configured(arguments)
            self.assertEqual(configure.call_count, 1)

    def test_the_shader_compiler_appears_only_once_it_is_complete_and_executable(self):
        target = self.scratch / f"sokol-shdc-{haylen.SHDC_COMMIT[:7]}" / haylen.executable_name("sokol-shdc")

        def download(url: str, partial: Path, sha256: str | None = None) -> None:
            self.assertFalse(target.exists())
            partial.parent.mkdir(parents=True, exist_ok=True)
            partial.write_bytes(b"compiler")

        with mock.patch.object(haylen, "TOOLS_DIR", self.scratch), mock.patch.object(haylen, "download", side_effect=download):
            self.assertEqual(haylen.ensure_shdc(), target)
        self.assertTrue(os.access(target, os.X_OK))
        self.assertEqual([path.name for path in target.parent.iterdir()], [target.name])


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
        (tree / "CMakeCache.txt").write_text("HAYLEN_RENDER_BACKEND:STRING=WGPU\n")
        (tree / "object.o").write_bytes(b"object")
        (tree / haylen.EMSCRIPTEN_MARKER).write_text("1.0.0")
        self.assertTrue(haylen.is_stale_web_tree(tree))
        with mock.patch.object(haylen, "run", side_effect=lambda command, env: tree.mkdir()) as run:
            haylen.configure_web_tree(["emcmake", "cmake"], tree)
        run.assert_called_once()
        self.assertFalse((tree / "object.o").exists())
        (tree / "CMakeCache.txt").write_text("HAYLEN_RENDER_BACKEND:STRING=WGPU\n")
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
        for arguments in (["run", str(app)], ["prepare", str(app), "--platform", "web"], ["dist", str(app), "--platform", "linux"], ["xcodegen", str(app)], ["steam", str(app), "--platform", "linux", "--app-id", "1", "--depot", "linux=2"]):
            with self.assertRaisesRegex(haylen.BuildError, 'is a C\\+\\+ app, whose CMake project builds it for every platform. Run it with "haylen run-cpp'):
                haylen.build_parser().parse_args(arguments).handler(haylen.build_parser().parse_args(arguments))

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
        for name in ("loader.js", "app.js", "webgpu/haylen.js", "webgpu/haylen-content-store.js", "plugins/banner/banner.js"):
            (site / name).parent.mkdir(parents=True, exist_ok=True)
            (site / name).write_text("const value = 1;\n")
        with mock.patch.object(haylen, "emsdk_root", return_value=emsdk), mock.patch.object(haylen, "run") as run:
            haylen.minify_web_scripts(site)
        commands = {Path(call.args[0][2]).relative_to(site).as_posix(): call.args[0] for call in run.call_args_list}
        self.assertEqual(sorted(commands), ["app.js", "loader.js", "plugins/banner/banner.js", "webgpu/haylen-content-store.js"])
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


class CppReleaseTest(unittest.TestCase):
    """The Release configuration of a C++ project ships the protected release with the content tool of the desktop artifacts, and every other configuration turns it off again in the cache of the build tree."""

    def test_turns_the_protected_release_on_only_for_release(self):
        self.assertEqual(haylen.cpp_release_options(mock.Mock(config="Debug")), ["-DHAYLEN_PROTECTED_RELEASE=OFF"])
        with mock.patch.object(haylen, "content_tool", return_value=Path("/tools/haylen-content")):
            options = haylen.cpp_release_options(mock.Mock(config="Release", engine_config="Release", jobs=2))
        self.assertEqual(options, ["-DHAYLEN_PROTECTED_RELEASE=ON", "-DHAYLEN_CONTENT_TOOL=/tools/haylen-content"])

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
