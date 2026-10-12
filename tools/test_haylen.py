"""Tests of the rules of haylen.py that need no build: how it merges Info.plist keys, entitlements and privacy manifests, what it checks in a built app, when it generates App.xcodeproj again, how it prints to the terminal and how its commands read their arguments."""

import base64
import contextlib
import functools
import http.client
import http.server
import io
import json
import os
import plistlib
import shutil
import subprocess
import socket
import sys
import tempfile
import threading
import time
import unittest
import urllib.parse
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

    def test_the_windows_launcher_hands_every_argument_to_the_tool(self):
        script = (haylen.ROOT / "haylen.cmd").read_text()
        self.assertIn('python "%~dp0haylen.py" %*', script)
        self.assertIn("exit /b %errorlevel%", script)


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
        args = haylen.build_parser().parse_args(["run-cpp", "project", "--platform", "ios-simulator"])
        simulator = {"udid": "id", "name": "Phone", "state": "Booted"}
        with mock.patch.object(haylen, "require_host"), mock.patch.object(haylen, "run"), mock.patch.object(haylen, "ensure_shdc", return_value="shdc"), mock.patch.object(haylen, "apple_simulator", return_value=simulator), mock.patch.object(haylen, "launch_apple", autospec=True) as launch:
            haylen.run_cpp_apple(self.scratch / "project", "game", self.scratch, args)
        self.assertEqual(launch.call_args.args[2:], (simulator, []))

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


class ConsumerArgumentTest(unittest.TestCase):
    def test_the_consumer_check_builds_the_listed_ways_or_every_way(self):
        parser = haylen.build_parser()
        self.assertEqual(haylen.consumer_modes(parser.parse_args(["sdk"])), [])
        self.assertEqual(haylen.consumer_modes(parser.parse_args(["sdk", "--check-consumers"])), ["subdirectory", "cpm", "package"])
        self.assertEqual(haylen.consumer_modes(parser.parse_args(["sdk", "--check-consumers", "package"])), ["package"])

    def test_the_consumer_check_runs_only_for_this_machine(self):
        with self.assertRaisesRegex(haylen.BuildError, '"--check-consumers" builds consumer projects for this machine'):
            haylen.consumer_modes(haylen.build_parser().parse_args(["sdk", "--platform", "web", "--check-consumers"]))


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

    def test_new_apps_get_a_project_of_every_platform_template(self):
        self.assertTrue({"apple", "android", "web", "windows", "linux"} <= set(haylen.platform_templates()))
        self.assertEqual({name: target.template for name, target in haylen.RUN_TARGETS.items() if name in ("windows", "linux")}, {"windows": "windows", "linux": "linux"})
        with mock.patch.object(haylen, "terminal", haylen.Terminal(io.StringIO(), io.StringIO(), {})):
            haylen.command_new(haylen.build_parser().parse_args(["new", str(self.scratch / "fresh")]))
        for template in haylen.platform_templates():
            self.assertEqual(sorted(haylen.project_files(self.scratch / "fresh" / "platform" / template)), sorted(haylen.project_files(haylen.PLATFORM_TEMPLATES_DIR / template)))

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
        manifest = (haylen.PLATFORM_TEMPLATES_DIR / "windows" / "resources" / "app.manifest").read_bytes()
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
