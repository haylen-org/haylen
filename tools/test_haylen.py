"""Tests of the rules of haylen.py that need no build: how it merges Info.plist keys, entitlements and privacy manifests, what it checks in a built app, when it generates App.xcodeproj again, how it prints to the terminal and how its commands read their arguments."""

import contextlib
import io
import json
import os
import plistlib
import shutil
import sys
import tempfile
import unittest
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


if __name__ == "__main__":
    unittest.main()
