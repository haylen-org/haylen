"""Tests of the rules of haylen.py that need no build: how it merges Info.plist keys, entitlements and privacy manifests, what it checks in a built app, when it generates App.xcodeproj again, how it prints to the terminal and how its commands read their arguments."""

import base64
import contextlib
import functools
import http.server
import io
import json
import os
import plistlib
import shutil
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
        self.assertEqual(set(haylen.CONTENT_PROFILES), set(haylen.RUN_TARGETS) - {"web"})
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
