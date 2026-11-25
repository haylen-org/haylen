import re

import pytest

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.platforms.android.android_contributions import AndroidContributions
from haylen.platforms.android.java_properties import JavaProperties
from tests.platforms.android.android_fixture import AndroidFixture


class AndroidSettingsTest:
    """The settings of the Android project that the app chooses through `platforms.android` of `haylen.json`: the features, the attributes of the activity, the limited permissions, the API level it targets, the shrinking of releases, the Gradle properties and the ABIs, with the values that the engine needs held and named with both owners when the app contradicts them."""

    @staticmethod
    def project(fixture: AndroidFixture, android: dict, document: dict | None = None, name: str = "app") -> tuple[str, str, dict[str, str]]:
        """The manifest, the script of the app module and the Gradle properties of an app with this section."""
        project = fixture.prepare(fixture.resolve(fixture.app({**(document or {}), "platforms": {"android": android}}, name)), f"{name}-product").project
        manifest = (project / "app" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")
        script = (project / "app" / "build.gradle.kts").read_text(encoding="utf-8")
        properties = JavaProperties.read((project / "gradle.properties").read_text(encoding="utf-8"))
        return manifest, script, properties

    def test_the_app_uses_every_device_feature_without_requiring_it_unless_it_says_so(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        manifest, _, _ = AndroidSettingsTest.project(fixture, {})
        assert '<uses-feature android:name="android.hardware.touchscreen" android:required="false" />' in manifest
        assert '<uses-feature android:name="android.software.leanback" android:required="false" />' in manifest

        features = {"android.hardware.touchscreen": True, "android.hardware.camera.any": False}
        manifest, _, _ = AndroidSettingsTest.project(fixture, {"manifest": {"features": features}}, name="touch")
        assert '<uses-feature android:name="android.hardware.touchscreen" android:required="true" />' in manifest
        assert '<uses-feature android:name="android.hardware.camera.any" android:required="false" />' in manifest
        assert manifest.count("android.hardware.touchscreen") == 1

    def test_the_activity_takes_the_attributes_of_the_app_besides_the_ones_of_the_engine(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        attributes = {"windowSoftInputMode": "adjustResize", "launchMode": "singleTop", "screenOrientation": "sensorPortrait"}
        manifest, _, _ = AndroidSettingsTest.project(fixture, {"manifest": {"activityAttributes": attributes}}, {"orientation": "portrait"})
        assert 'android:windowSoftInputMode="adjustResize"' in manifest
        assert manifest.count('android:launchMode="singleTop"') == 1
        assert 'android:name="dev.haylen.HaylenActivity"' in manifest

    def test_an_activity_attribute_that_contradicts_the_engine_or_the_orientation_stops_with_both_owners(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        attributes = {"launchMode": "standard", "screenOrientation": "fullSensor"}
        with pytest.raises(InvalidManifest) as raised:
            AndroidSettingsTest.project(fixture, {"manifest": {"activityAttributes": attributes}})
        message = raised.value.message
        assert (
            'The key "platforms.android.manifest.activityAttributes.launchMode" is "standard", while the engine, whose single activity keeps the screens of plugins over the app when the launcher brings it back, gives "singleTop".'
            in message
        )
        assert re.search(r'activityAttributes\.screenOrientation" is "fullSensor", while "orientation" of `.+`, which is "landscape", gives "sensorLandscape"\.', message)

    def test_a_permission_applies_up_to_the_api_level_that_the_app_names(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        permissions = ["android.permission.CAMERA", {"name": "android.permission.WRITE_EXTERNAL_STORAGE", "maxSdkVersion": 28}]
        manifest, _, _ = AndroidSettingsTest.project(fixture, {"manifest": {"permissions": permissions}})
        assert '<uses-permission android:name="android.permission.CAMERA" />' in manifest
        assert '<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" android:maxSdkVersion="28" />' in manifest

    def test_a_game_takes_the_game_mode_of_android(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        game, _, _ = AndroidSettingsTest.project(fixture, {}, {"category": "game"}, "game")
        music, _, _ = AndroidSettingsTest.project(fixture, {}, {"category": "music"}, "music")
        assert 'android:isGame="true"' in game
        assert "android:isGame" not in music
        overridden, _, _ = AndroidSettingsTest.project(fixture, {"manifest": {"applicationAttributes": {"isGame": "false"}}}, {"category": "game"}, "tool")
        assert 'android:isGame="false"' in overridden

    def test_the_app_targets_an_api_level_between_its_oldest_and_the_one_of_the_engine(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        _, script, _ = AndroidSettingsTest.project(fixture, {})
        assert f"targetSdk = {AndroidContributions.TARGET_SDK}" in script
        assert f"version = release({AndroidContributions.COMPILE_SDK})" in script
        _, script, _ = AndroidSettingsTest.project(fixture, {"targetSdkVersion": 35, "minimumSystemVersion": "29"}, name="older")
        assert "targetSdk = 35" in script

        with pytest.raises(InvalidManifest, match=r"The app targets API level 28, from .+ at platforms\.android\.targetSdkVersion, while its oldest API level is 29"):
            AndroidSettingsTest.project(fixture, {"targetSdkVersion": 28, "minimumSystemVersion": "29"}, name="below")
        with pytest.raises(InvalidManifest, match=f"while the engine compiles against API level {AndroidContributions.COMPILE_SDK}"):
            AndroidSettingsTest.project(fixture, {"targetSdkVersion": AndroidContributions.COMPILE_SDK + 1}, name="above")

    def test_a_release_shrinks_the_app_unless_it_says_otherwise(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        _, shrunk, _ = AndroidSettingsTest.project(fixture, {})
        _, kept, _ = AndroidSettingsTest.project(fixture, {"minify": False}, name="kept")
        assert "isMinifyEnabled = true" in shrunk
        assert "isShrinkResources = true" in shrunk
        assert "isMinifyEnabled = false" in kept
        assert "isShrinkResources = false" in kept

    def test_the_gradle_properties_of_the_app_join_the_ones_that_the_engine_needs(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        _, _, properties = AndroidSettingsTest.project(fixture, {"gradleProperties": {"org.gradle.jvmargs": "-Xmx8g", "org.gradle.caching": True, "android.useAndroidX": True}})
        assert properties["org.gradle.jvmargs"] == "-Xmx8g"
        assert properties["org.gradle.caching"] == "true"
        assert properties["android.useAndroidX"] == "true"
        assert "kotlin.project.persistent.dir" in properties

        with pytest.raises(InvalidManifest, match=re.escape('The key "platforms.android.gradleProperties.android.useAndroidX" is "false", while the Android backend gives "true".')):
            AndroidSettingsTest.project(fixture, {"gradleProperties": {"android.useAndroidX": False}}, name="without")

    def test_the_app_builds_the_abis_that_it_names_which_the_command_narrows(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"platforms": {"android": {"architectures": ["arm64-v8a"]}}})
        assert [project.target.abi for project in fixture.resolve(app).projects] == ["arm64-v8a"]
        assert fixture.resolve(app).primary.setting_origins()["android"]["architectures"]["value"] == ["arm64-v8a"]
        with pytest.raises(UnsupportedTarget, match=r'The option "--architecture" names "x86_64", which "platforms\.android\.architectures" of `.+` leaves out\.'):
            fixture.resolve(app, architectures=("x86_64",))
        with pytest.raises(InvalidManifest, match=r'names "mips"'):
            fixture.resolve(fixture.app({"platforms": {"android": {"architectures": ["mips"]}}}, "mips"))
