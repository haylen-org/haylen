import re

import pytest

from haylen.core.app_category import AppCategory
from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.platforms.apple.apple_project import AppleProject
from haylen.platforms.apple.info_plist_keys import InfoPlistKeys
from haylen.platforms.apple.privacy_manifest import PrivacyManifest
from haylen.platforms.apple.property_lists import PropertyLists
from tests.platforms.apple.apple_fixture import AppleFixture


class ApplePropertyListsTest:
    """The Info.plist, entitlements and privacy manifest of Apple apps: the template, the keys of the engine from `haylen.json` and the contributions of the plugins and the app join, and two values of one key stop the preparation with both origins."""

    @pytest.fixture
    def apple(self, tmp_path, monkeypatch) -> AppleFixture:
        return AppleFixture(tmp_path, monkeypatch)

    def info(self, apple: AppleFixture, document: dict, target: str = "ios-simulator") -> dict:
        app = apple.app(document)
        return AppleFixture.plist(apple.prepare(app, target), AppleProject.INFO_PLIST)

    def test_objects_join_and_lists_gain_missing_items(self):
        joined = PropertyLists.join(
            [
                ("the template", {"SKAdNetworkItems": [{"SKAdNetworkIdentifier": "a"}], "Settings": {"first": 1}}),
                ('"ads"', {"SKAdNetworkItems": [{"SKAdNetworkIdentifier": "a"}, {"SKAdNetworkIdentifier": "b"}], "Settings": {"second": 2}}),
            ],
            "the Info.plist",
        )
        assert joined == {"SKAdNetworkItems": [{"SKAdNetworkIdentifier": "a"}, {"SKAdNetworkIdentifier": "b"}], "Settings": {"first": 1, "second": 2}}

    def test_two_values_of_one_key_name_both_origins(self):
        with pytest.raises(InvalidContribution, match=re.escape('The key "Settings.mode" is "one" for the template and "two" for "ads"')):
            PropertyLists.join([("the template", {"Settings": {"mode": "one"}}), ('"ads"', {"Settings": {"mode": "two"}})], "the Info.plist")

    def test_the_app_launches_with_the_status_bar_of_haylen_json(self, apple):
        assert self.info(apple, {})["UIStatusBarHidden"] is True
        shown = self.info(apple, {"window": {"statusBar": "dark"}})
        assert (shown["UIStatusBarHidden"], shown["UIStatusBarStyle"]) == (False, "UIStatusBarStyleDarkContent")

    def test_iphones_list_every_orientation_and_refresh_above_60_hertz(self, apple):
        info = self.info(apple, {"orientation": "portrait"})
        assert info["CADisableMinimumFrameDurationOnPhone"] is True
        assert info["UISupportedInterfaceOrientations"] == ["UIInterfaceOrientationPortrait", "UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"]
        assert info["UISupportedInterfaceOrientations~ipad"] == [
            "UIInterfaceOrientationPortrait",
            "UIInterfaceOrientationPortraitUpsideDown",
            "UIInterfaceOrientationLandscapeLeft",
            "UIInterfaceOrientationLandscapeRight",
        ]
        assert info["UIApplicationSceneManifest"] == {"UIApplicationSupportsMultipleScenes": True}

    def test_a_mac_app_that_hides_from_the_dock_is_an_agent(self, apple):
        assert self.info(apple, {"window": {"showInTaskbar": False}}, "macos")["LSUIElement"] is True

    def test_mac_apps_take_the_category_of_haylen_json(self, apple):
        assert self.info(apple, {"category": "game"}, "macos")["LSApplicationCategoryType"] == "public.app-category.games"
        assert self.info(apple, {"category": "developer"}, "catalyst")["LSApplicationCategoryType"] == "public.app-category.developer-tools"
        assert "LSApplicationCategoryType" not in self.info(apple, {"category": "game"}, "ios-simulator")
        assert set(InfoPlistKeys.CATEGORIES) == set(AppCategory)

    def test_an_app_that_sets_another_category_of_its_own_stops_the_preparation(self, apple):
        app = apple.app({"category": "music", "platforms": {"macos": {"infoPlist": {"LSApplicationCategoryType": "public.app-category.games"}}}})
        with pytest.raises(InvalidContribution, match=r'"LSApplicationCategoryType" is "public.app-category.music" for the engine from .* and "public.app-category.games" for "app"'):
            apple.prepare(app, "macos")

    def test_plugins_and_the_app_add_keys_and_items(self, apple):
        app = apple.app({"plugins": {"links": {}}, "platforms": {"ios": {"infoPlist": {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["own"]}], "UIBackgroundModes": ["fetch"]}}}})
        values = {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["${scheme}"]}], "UIBackgroundModes": ["audio"]}
        parameters = {"scheme": {"type": "string", "default": "demo", "description": "The scheme."}}
        apple.plugin(app, "links", {"implementations": {"ios": "native"}, "parameters": parameters, "contributions": [{"type": "infoPlist", "platforms": ["ios"], "values": values}]})
        info = AppleFixture.plist(apple.prepare(app), AppleProject.INFO_PLIST)
        assert info["CFBundleURLTypes"] == [{"CFBundleURLSchemes": ["demo"]}, {"CFBundleURLSchemes": ["own"]}]
        assert info["UIBackgroundModes"] == ["audio", "fetch"]

    def test_a_plugin_that_changes_a_key_of_the_engine_stops_the_preparation(self, apple):
        app = apple.app({"plugins": {"bar": {}}})
        apple.plugin(app, "bar", {"implementations": {"ios": "native"}, "contributions": [{"type": "infoPlist", "platforms": ["ios"], "values": {"UIStatusBarHidden": False}}]})
        with pytest.raises(InvalidContribution, match=r'"UIStatusBarHidden" is true for the engine from .* and false for "bar"'):
            apple.prepare(app)

    def test_the_entitlements_of_plugins_and_the_app_sign_the_app(self, apple):
        app = apple.app({"plugins": {"groups": {}}, "platforms": {"macos": {"entitlements": {"com.apple.security.network.client": True}}}})
        values = {"com.apple.security.application-groups": ["group.$(HAYLEN_BUNDLE_IDENTIFIER)"]}
        apple.plugin(app, "groups", {"implementations": {"macos": "native"}, "contributions": [{"type": "entitlements", "platforms": ["macos"], "values": values}]})
        prepared = apple.prepare(app, "macos")
        assert AppleFixture.plist(prepared, AppleProject.ENTITLEMENTS) == {**values, "com.apple.security.network.client": True}
        assert AppleFixture.spec(prepared)["targets"]["App"]["settings"]["base"]["CODE_SIGN_ENTITLEMENTS"] == str(prepared.folders.project / AppleProject.ENTITLEMENTS)
        assert not (apple.prepare(apple.app({}, "plain"), "macos").folders.project / AppleProject.ENTITLEMENTS).exists()

    def test_the_reasons_of_an_api_type_join_one_entry(self):
        engine = {"NSPrivacyAccessedAPITypes": [{"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryFileTimestamp", "NSPrivacyAccessedAPITypeReasons": ["C617.1"]}]}
        plugin = {
            "NSPrivacyTracking": True,
            "NSPrivacyTrackingDomains": ["ads.example.com"],
            "NSPrivacyAccessedAPITypes": [{"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryFileTimestamp", "NSPrivacyAccessedAPITypeReasons": ["3B52.1"]}],
        }
        app = {"NSPrivacyTrackingDomains": ["ads.example.com", "own.example.com"]}
        merged = PrivacyManifest.merge([("the engine", engine), ('"ads"', plugin), ("the app", app)])
        assert merged["NSPrivacyAccessedAPITypes"] == [{"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryFileTimestamp", "NSPrivacyAccessedAPITypeReasons": ["C617.1", "3B52.1"]}]
        assert merged["NSPrivacyTracking"] is True
        assert merged["NSPrivacyTrackingDomains"] == ["ads.example.com", "own.example.com"]
        assert PrivacyManifest.merge([("the engine", {}), ("the app", {})]) == {}

    def test_the_privacy_manifest_joins_the_engine_and_the_app(self, apple):
        app = apple.app({"platforms": {"apple": {"privacy": {"NSPrivacyTracking": False}}}})
        privacy = AppleFixture.plist(apple.prepare(app), AppleProject.PRIVACY)
        assert privacy["NSPrivacyTracking"] is False
        assert "NSPrivacyAccessedAPICategoryFileTimestamp" in [entry["NSPrivacyAccessedAPIType"] for entry in privacy["NSPrivacyAccessedAPITypes"]]

    def test_a_built_value_holds_the_items_and_keys_it_needs(self):
        built = {
            "CFBundleURLTypes": [{"CFBundleURLName": "other"}, {"CFBundleURLName": "demo", "CFBundleURLSchemes": ["demo", "more"]}],
            "NSCameraUsageDescription": "Reworded",
            "com.apple.developer.game-center": True,
        }
        assert PropertyLists.holds(built, {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["demo"]}]})
        assert PropertyLists.holds(built, {"NSCameraUsageDescription": "The text of the plugin"})
        assert not PropertyLists.holds(built, {"CFBundleURLTypes": [{"CFBundleURLSchemes": ["missing"]}]})
        assert not PropertyLists.holds(built, {"NSMicrophoneUsageDescription": "Needed"})
        assert not PropertyLists.holds({"com.apple.developer.game-center": False}, {"com.apple.developer.game-center": True})
