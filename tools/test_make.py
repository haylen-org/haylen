"""Tests of the rules of make.py that need no build: how it merges Info.plist keys, entitlements and privacy manifests, what it checks in a built app and when it generates App.xcodeproj again."""

import plistlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import make  # noqa: E402


class PluginKeysTest(unittest.TestCase):
    def test_objects_merge_and_arrays_gain_missing_items(self):
        merged = {"SKAdNetworkItems": [{"SKAdNetworkIdentifier": "a"}], "Settings": {"first": 1}}
        owners = {"SKAdNetworkItems": '"app.json"', "Settings": '"app.json"'}
        make.merge_plugin_keys(merged, {"SKAdNetworkItems": [{"SKAdNetworkIdentifier": "a"}, {"SKAdNetworkIdentifier": "b"}], "Settings": {"second": 2}}, owners, 'the plugin "ads"', '"Info.plist"')
        self.assertEqual(merged, {"SKAdNetworkItems": [{"SKAdNetworkIdentifier": "a"}, {"SKAdNetworkIdentifier": "b"}], "Settings": {"first": 1, "second": 2}})

    def test_two_plugins_that_disagree_stop_the_build_with_both_named(self):
        merged, owners = {}, {}
        make.merge_plugin_keys(merged, {"GADApplicationIdentifier": "one"}, owners, 'the plugin "first"', '"Info.plist"')
        with self.assertRaisesRegex(make.BuildError, 'the plugin "first" and "two" for the plugin "second"'):
            make.merge_plugin_keys(merged, {"GADApplicationIdentifier": "two"}, owners, 'the plugin "second"', '"Info.plist"')


class DeveloperKeysTest(unittest.TestCase):
    def test_the_value_of_the_developer_wins_with_a_warning(self):
        warnings: list[str] = []
        completed = make.complete_keys({"NSCameraUsageDescription": "Ours"}, {"NSCameraUsageDescription": "Theirs"}, {"NSCameraUsageDescription": 'the plugin "camera"'}, "ios/Info.plist", warnings)
        self.assertEqual(completed, {"NSCameraUsageDescription": "Ours"})
        self.assertEqual(warnings, ['The key "NSCameraUsageDescription" of "ios/Info.plist" keeps its value "Ours", while the plugin "camera" gives "Theirs".'])

    def test_generated_keys_fill_what_the_developer_lacks(self):
        warnings: list[str] = []
        own = {"UIRequiresFullScreen": True, "CFBundleURLTypes": [{"CFBundleURLSchemes": ["own"]}], "UIApplicationSceneManifest": {"UIApplicationSupportsMultipleScenes": True}}
        generated = {"UISupportedInterfaceOrientations": ["UIInterfaceOrientationPortrait"], "CFBundleURLTypes": [{"CFBundleURLSchemes": ["demo"]}], "UIApplicationSceneManifest": {"UISceneConfigurations": {}}}
        owners = {key: '"app.json"' for key in generated}
        completed = make.complete_keys(own, generated, owners, "ios/Info.plist", warnings)
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
        merged = make.merge_privacy([engine, plugin, developer])
        self.assertEqual(merged["NSPrivacyAccessedAPITypes"], [{"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryFileTimestamp", "NSPrivacyAccessedAPITypeReasons": ["C617.1", "3B52.1"]}])
        self.assertTrue(merged["NSPrivacyTracking"])
        self.assertEqual(merged["NSPrivacyTrackingDomains"], ["ads.example.com", "own.example.com"])

    def test_a_manifest_without_keys_adds_nothing(self):
        self.assertEqual(make.merge_privacy([{}, {}]), {})


class RequirementTest(unittest.TestCase):
    def test_a_built_value_holds_the_items_and_keys_it_needs(self):
        built = {"CFBundleURLTypes": [{"CFBundleURLName": "other"}, {"CFBundleURLName": "demo", "CFBundleURLSchemes": ["demo", "more"]}], "NSCameraUsageDescription": "Reworded", "com.apple.developer.game-center": True}
        self.assertTrue(make.holds(built, {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["demo"]}]}))
        self.assertTrue(make.holds(built, {"NSCameraUsageDescription": "The text of the plugin"}))
        self.assertFalse(make.holds(built, {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["missing"]}]}))
        self.assertFalse(make.holds(built, {"NSMicrophoneUsageDescription": "Needed"}))
        self.assertFalse(make.holds({"com.apple.developer.game-center": False}, {"com.apple.developer.game-center": True}))

    def test_a_snippet_holds_the_key_and_its_value(self):
        self.assertEqual(make.plist_snippet("NSCameraUsageDescription", "Scans codes."), "<key>NSCameraUsageDescription</key>\n<string>Scans codes.</string>")
        self.assertEqual(plistlib.loads(("<plist><dict>" + make.plist_snippet("UIApplicationSceneManifest", {"UIApplicationSupportsMultipleScenes": True}) + "</dict></plist>").encode()), {"UIApplicationSceneManifest": {"UIApplicationSupportsMultipleScenes": True}})

    def test_a_requirement_names_the_file_and_indents_the_snippet(self):
        requirement = make.Requirement('The plugin "demo" needs "NSCameraUsageDescription".', 'Add to "ios/Info.plist":', "<key>NSCameraUsageDescription</key>\n<string>Why.</string>")
        self.assertEqual(requirement.describe(), 'The plugin "demo" needs "NSCameraUsageDescription".\n  Add to "ios/Info.plist":\n    <key>NSCameraUsageDescription</key>\n    <string>Why.</string>')


class ProjectGenerationTest(unittest.TestCase):
    def test_a_project_without_the_include_is_never_generated(self):
        self.assertEqual(make.apple_project_action(False, {}, "inputs", "edited", "template"), "keep")

    def test_a_project_generated_from_the_same_inputs_is_kept_with_its_edits(self):
        self.assertEqual(make.apple_project_action(True, {"inputs": "inputs", "project": "project"}, "inputs", "project", "template"), "keep")
        self.assertEqual(make.apple_project_action(True, {"inputs": "inputs", "project": "project"}, "inputs", "edited", "template"), "keep")

    def test_new_inputs_generate_a_project_without_edits(self):
        self.assertEqual(make.apple_project_action(True, {"inputs": "old", "project": "project"}, "inputs", "project", "template"), "generate")

    def test_a_missing_project_or_an_untouched_copy_of_the_template_is_generated(self):
        self.assertEqual(make.apple_project_action(True, {"inputs": "inputs", "project": "project"}, "inputs", None, "template"), "generate")
        self.assertEqual(make.apple_project_action(True, {}, "inputs", "template", "template"), "generate")

    def test_new_inputs_for_a_project_with_edits_need_a_comparison(self):
        self.assertEqual(make.apple_project_action(True, {"inputs": "old", "project": "project"}, "inputs", "edited", "template"), "compare")
        self.assertEqual(make.apple_project_action(True, {}, "inputs", "cloned", "template"), "compare")

    def test_the_inputs_follow_the_files_of_the_plugins(self):
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            (root / "project.yml").write_text("name: App\n")
            (root / "haylen" / "plugins" / "demo" / "sources").mkdir(parents=True)
            (root / "haylen" / "project.yml").write_text("{}\n")
            first = make.apple_project_inputs(root)
            (root / "haylen" / "plugins" / "demo" / "sources" / "Demo.swift").write_text("")
            self.assertNotEqual(make.apple_project_inputs(root), first)


class PropertiesTest(unittest.TestCase):
    def test_values_survive_as_ascii_with_escapes(self):
        self.assertEqual(make.java_property("Ilha Tropical ✓\\n"), "Ilha Tropical \\u2713\\\\n")


if __name__ == "__main__":
    unittest.main()
