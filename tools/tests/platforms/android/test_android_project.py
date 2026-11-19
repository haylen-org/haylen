import json
import re
import shutil

import pytest

from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.native.cmake_app import CMakeApp
from haylen.toolchains.android_ndk import AndroidNdk
from haylen.toolchains.gradle import Gradle
from tests.platforms.android.android_fixture import AndroidFixture


class AndroidProjectTest:
    def test_preparing_again_writes_nothing_and_a_deleted_project_comes_back_the_same(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"camera": {}}})
        fixture.plugin(app, "camera", {})
        product = fixture.resolve(app)

        first = fixture.prepare(product)
        files = AndroidFixture.files(first.project)
        times = {path: path.stat().st_mtime_ns for path in first.project.rglob("*") if path.is_file()}
        again = fixture.prepare(product)

        assert first.changed
        assert not again.changed
        assert {path: path.stat().st_mtime_ns for path in again.project.rglob("*") if path.is_file()} == times

        shutil.rmtree(first.project)
        rebuilt = fixture.prepare(product)
        assert rebuilt.changed
        assert AndroidFixture.files(rebuilt.project) == files
        assert rebuilt.state.output == first.state.output

    def test_one_project_per_abi_and_only_those_abis_in_the_apk(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({})

        every = fixture.resolve(app)
        two = fixture.resolve(app, architectures=("arm64-v8a", "x86_64"))
        script = (fixture.prepare(two).project / "app" / "build.gradle.kts").read_text(encoding="utf-8")

        assert [project.target.abi for project in every.projects] == list(AndroidNdk.ABIS)
        assert [project.target.abi for project in two.projects] == ["arm64-v8a", "x86_64"]
        assert 'abiFilters += listOf("arm64-v8a", "x86_64")' in script

    def test_the_identity_of_the_app_reaches_the_scripts_and_the_manifest(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        document = {
            "name": "@Tiny & Island",
            "version": "2.4.1",
            "build": 12,
            "orientation": "portrait",
            "window": {"statusBar": "dark"},
            "platforms": {"android": {"applicationClass": "com.haylen.tests.App", "minimumSystemVersion": "29"}},
        }
        project = fixture.prepare(fixture.resolve(fixture.app(document))).project
        script = (project / "app" / "build.gradle.kts").read_text(encoding="utf-8")
        manifest = (project / "app" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")

        assert 'applicationId = "com.haylen.tests"' in script
        assert 'versionName = "2.4.1"' in script
        assert "versionCode = 12" in script
        assert "minSdk = 29" in script
        assert f'ndkVersion = "{AndroidNdk.VERSION}"' in script
        assert 'android:name="com.haylen.tests.App"' in manifest
        assert 'android:label="\\@Tiny &amp; Island"' in manifest
        assert 'android:screenOrientation="sensorPortrait"' in manifest
        assert 'android:value="dark"' in manifest
        assert 'android:value="haylen"' in manifest, "A Lua debug build plays the package with the player of the haylen library."
        assert "{{" not in script + manifest

    def test_the_application_takes_the_category_that_android_has(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        manifests = {}
        for category in ("music", "education"):
            project = fixture.prepare(fixture.resolve(fixture.app({"category": category}, category)), f"{category}-product").project
            manifests[category] = (project / "app" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")

        assert 'android:appCategory="audio"' in manifests["music"]
        assert "android:appCategory" not in manifests["education"]

    def test_the_wrapper_downloads_the_pinned_gradle_with_its_hash(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        project = fixture.prepare(fixture.resolve(fixture.app({}))).project
        properties = (project / "gradle" / "wrapper" / "gradle-wrapper.properties").read_text(encoding="utf-8").splitlines()

        assert f"distributionUrl=https\\://services.gradle.org/distributions/gradle-{Gradle.VERSION}-bin.zip" in properties
        assert f"distributionSha256Sum={Gradle.SHA256}" in properties

    def test_the_manifest_section_adds_and_removes_permissions_and_sets_attributes(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        manifest = {"permissions": ["android.permission.CAMERA"], "removedPermissions": ["android.permission.VIBRATE"], "applicationAttributes": {"icon": "@mipmap/own", "largeHeap": "true"}}
        project = fixture.prepare(fixture.resolve(fixture.app({"platforms": {"android": {"manifest": manifest}}}))).project
        text = (project / "app" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")

        assert '<uses-permission android:name="android.permission.CAMERA" />' in text
        assert '<uses-permission android:name="android.permission.VIBRATE" tools:node="remove" />' in text
        assert '<uses-permission android:name="android.permission.INTERNET" />' in text
        assert 'android:icon="@mipmap/own"' in text
        assert 'android:largeHeap="true"' in text

    def test_lua_and_cpp_apps_with_the_same_plugin_get_the_same_plugin_parts(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        plugin = {
            "parameters": {"scheme": {"type": "string", "default": "kit", "description": "The scheme."}},
            "contributions": [
                {"type": "manifestPlaceholder", "platforms": ["android"], "name": "kitScheme", "value": "${scheme}"},
                {"type": "mavenRepository", "platforms": ["android"], "url": "https://maven.example.com/kit"},
            ],
        }
        lua = fixture.app({"plugins": {"kit": {"parameters": {"scheme": "game"}}}}, "lua")
        cpp = fixture.app({"kind": "cpp", "cpp": {"sources": ["source"]}, "plugins": {"kit": {"parameters": {"scheme": "game"}}}}, "cpp")
        for app in (lua, cpp):
            fixture.plugin(app, "kit", plugin)

        lua_parts = AndroidProjectTest.plugin_parts(fixture.prepare(fixture.resolve(lua), "lua-product").project)
        cpp_parts = AndroidProjectTest.plugin_parts(fixture.prepare(fixture.resolve(cpp), "cpp-product").project)

        assert lua_parts == cpp_parts
        assert json.loads(lua_parts[1]) == {"plugins": [{"id": "kit", "version": "1.0.0", "parameters": {"scheme": "game"}}]}
        assert '            "kitScheme" to "game",' in lua_parts[2]

    def test_the_module_of_a_plugin_reaches_the_modules_of_the_plugins_it_depends_on(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"camera": {}, "permissions": {}, "theme": {}}})
        fixture.plugin(app, "permissions", {})
        fixture.plugin(app, "theme", {})
        fixture.plugin(app, "camera", {"dependencies": {"permissions": "1.0.0", "theme": "1.0.0"}})

        files = AndroidFixture.files(fixture.prepare(fixture.resolve(app)).project)
        script = files["plugins/camera/build.gradle.kts"].decode()

        assert script.startswith('android {\n    namespace = "com.haylen.plugins.camera"\n}\n')
        assert script.endswith('dependencies {\n    api(project(":permissions"))\n    api(project(":theme"))\n}\n')
        assert files["plugins/permissions/build.gradle.kts"] == (app / "plugins" / "permissions" / "android" / "build.gradle.kts").read_bytes()

    @staticmethod
    def plugin_parts(project) -> tuple:
        """The files of the plugin modules, the list of the plugins of the runtime and the lines of the scripts that name the plugin."""
        files = AndroidFixture.files(project)
        scripts = files["settings.gradle.kts"].decode() + files["app/build.gradle.kts"].decode()
        modules = {name: content for name, content in files.items() if name.startswith("plugins/")}
        return modules, files["app/src/main/assets/haylen-plugins.json"], [line for line in scripts.splitlines() if "kit" in line.lower()]

    def test_the_plugins_file_lists_the_native_plugins_in_load_order_with_their_values(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"store": {"parameters": {"sandbox": True}}, "sounds": {}, "accounts": {}}})
        fixture.plugin(app, "accounts", {"version": "2.0.0", "parameters": {"server": {"type": "string", "default": "accounts.example.com", "description": "The server."}}})
        fixture.tooling.plugin(app, "sounds", {"implementations": {"android": "lua"}, "dependencies": {"accounts": "2.0"}})
        fixture.plugin(app, "store", {"version": "1.1.0", "dependencies": {"accounts": "2.0"}, "parameters": {"sandbox": {"type": "boolean", "default": False, "description": "Sandbox."}}})

        project = fixture.prepare(fixture.resolve(app)).project
        written = json.loads((project / "app" / "src" / "main" / "assets" / "haylen-plugins.json").read_text(encoding="utf-8"))

        assert written == {"plugins": [{"id": "accounts", "version": "2.0.0", "parameters": {"server": "accounts.example.com"}}, {"id": "store", "version": "1.1.0", "parameters": {"sandbox": True}}]}

    def test_the_maven_repositories_of_plugins_reach_the_project_once_and_in_order(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"ads": {}, "maps": {}}})
        for identifier, urls in (("ads", ["https://maven.ads.example/releases", "https://maven.shared.example"]), ("maps", ["https://maven.shared.example"])):
            fixture.plugin(app, identifier, {"contributions": [{"type": "mavenRepository", "platforms": ["android"], "url": url} for url in urls]})

        project = fixture.prepare(fixture.resolve(app)).project

        for name in ("settings.gradle.kts", "build.gradle.kts"):
            lines = [line.strip() for line in (project / name).read_text(encoding="utf-8").splitlines() if "maven { url" in line]
            assert lines == ['maven { url = uri("https://maven.ads.example/releases") }', 'maven { url = uri("https://maven.shared.example") }']

    def test_plugin_files_and_app_files_reach_the_project(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"cloud": {"parameters": {"services": "config/services.json"}}}, "platforms": {"android": {"files": {"app/src/main/res/drawable/banner.xml": "art/banner.xml"}}}})
        (app / "config").mkdir()
        (app / "config" / "services.json").write_text("{}", encoding="utf-8")
        (app / "art").mkdir()
        (app / "art" / "banner.xml").write_text("<vector />", encoding="utf-8")
        plugin = fixture.plugin(
            app,
            "cloud",
            {
                "parameters": {"services": {"type": "file", "description": "The services file."}},
                "contributions": [
                    {"type": "projectFile", "platforms": ["android"], "from": "${services}", "to": "app/services.json"},
                    {"type": "projectFile", "platforms": ["android"], "from": "proguard.txt", "to": "app/cloud-rules.txt"},
                ],
            },
        )
        (plugin / "proguard.txt").write_text("-keep class cloud.**", encoding="utf-8")

        project = fixture.prepare(fixture.resolve(app)).project

        assert (project / "app" / "services.json").read_text(encoding="utf-8") == "{}"
        assert (project / "app" / "cloud-rules.txt").read_text(encoding="utf-8") == "-keep class cloud.**"
        assert (project / "app" / "src" / "main" / "res" / "drawable" / "banner.xml").read_text(encoding="utf-8") == "<vector />"

    def test_a_plugin_file_never_replaces_a_generated_file(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"rogue": {}}})
        plugin = fixture.plugin(app, "rogue", {"contributions": [{"type": "projectFile", "platforms": ["android"], "from": "build.txt", "to": "app/build.gradle.kts"}]})
        (plugin / "build.txt").write_text("", encoding="utf-8")

        with pytest.raises(InvalidContribution, match=re.escape('places "app/build.gradle.kts" in the Android project, which the project generates itself')):
            fixture.prepare(fixture.resolve(app))

    def test_a_cpp_app_compiles_its_library_against_the_sdk_of_each_abi(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"kind": "cpp", "cpp": {"sources": ["src"], "definitions": ["GAME=1"]}, "native": {"physics": {"cmake": "physics", "platforms": ["android"]}}})
        (app / "src").mkdir()
        (app / "src" / "App.cpp").write_text("", encoding="utf-8")
        (app / "physics").mkdir()
        (app / "physics" / "CMakeLists.txt").write_text("", encoding="utf-8")

        project = fixture.prepare(fixture.resolve(app, architectures=("arm64-v8a",))).project
        cmake = (project / "native" / "CMakeLists.txt").read_text(encoding="utf-8")
        script = (project / "app" / "build.gradle.kts").read_text(encoding="utf-8")
        manifest = (project / "app" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")

        assert f"set({CMakeApp.text('HAYLEN_SDK_arm64-v8a')} {CMakeApp.text((fixture.sdk.folder / 'abis/arm64-v8a/lib/cmake/haylen').as_posix())})" in cmake
        assert f"haylen_add_app(haylen-app CPP SOURCES {CMakeApp.text((app / 'src' / 'App.cpp').as_posix())} CONTENT" in cmake
        assert f"target_compile_definitions(haylen-app PRIVATE {CMakeApp.text('GAME=1')})" in cmake
        assert "add_subdirectory(" in cmake
        assert "BOOTSTRAP" not in cmake
        assert 'targets += listOf("haylen-app", "physics")' in script
        assert '"-DCMAKE_CXX_SCAN_FOR_MODULES=OFF"' in script
        assert 'excludes += "lib/*/libhaylen.so"' in script
        assert 'android:value="haylen-app"' in manifest

    def test_a_cpp_app_with_objective_c_sources_fails_before_gradle_compiles_them(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"kind": "cpp", "cpp": {"sources": ["src"]}})
        (app / "src").mkdir()
        (app / "src" / "App.cpp").write_text("", encoding="utf-8")
        (app / "src" / "Bridge.mm").write_text("", encoding="utf-8")

        with pytest.raises(InvalidManifest, match=re.escape("names the Objective-C files `")):
            fixture.prepare(fixture.resolve(app, architectures=("arm64-v8a",)))

    def test_a_lua_debug_app_compiles_only_its_native_libraries(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        bare = fixture.prepare(fixture.resolve(fixture.app({}, "bare")), "bare-product").project
        app = fixture.app({"native": {"physics": {"cmake": "physics", "platforms": ["android"]}}})
        (app / "physics").mkdir()
        (app / "physics" / "CMakeLists.txt").write_text("", encoding="utf-8")
        project = fixture.prepare(fixture.resolve(app)).project

        assert not (bare / "native").exists()
        assert "externalNativeBuild" not in (bare / "app" / "build.gradle.kts").read_text(encoding="utf-8")
        assert 'targets += listOf("physics")' in (project / "app" / "build.gradle.kts").read_text(encoding="utf-8")
        assert "haylen_add_app" not in (project / "native" / "CMakeLists.txt").read_text(encoding="utf-8")

    def test_the_package_ships_in_the_assets_with_its_index(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"kit": {}}})
        fixture.plugin(app, "kit", {})

        assets = fixture.prepare(fixture.resolve(app)).project / "app" / "src" / "main" / "assets" / "app"

        assert json.loads((assets / "haylen-package-index.json").read_text(encoding="utf-8")) == ["haylen.json", "plugins/kit/plugin.json", "plugins/kit/source/init.lua", "source/main.lua"]
        assert (assets / "plugins" / "kit" / "source" / "init.lua").is_file()

    def test_the_splash_of_the_app_replaces_the_one_of_the_engine(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"splash": {"background": "#102030", "darkBackground": "#80405060", "logo": "logo.png"}})
        (app / "content" / "logo.png").write_bytes(b"png")

        resources = fixture.prepare(fixture.resolve(app)).project / "app" / "src" / "main" / "res"

        assert "#FF102030" in (resources / "values" / "haylen_splash.xml").read_text(encoding="utf-8")
        assert "#80405060" in (resources / "values-night" / "haylen_splash.xml").read_text(encoding="utf-8")
        assert (resources / "drawable" / "haylen_splash_logo.png").read_bytes() == b"png"

    def test_an_identifier_that_android_refuses_stops_the_preparation(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"identifier": "com.my-studio.game"})

        with pytest.raises(UnsupportedTarget, match="is no application id of Android"):
            fixture.prepare(fixture.resolve(app))

    def test_background_audio_brings_the_playback_library(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        script = fixture.prepare(fixture.resolve(fixture.app({"audio": {"background": True}}))).project / "app" / "build.gradle.kts"

        assert 'implementation("dev.haylen:haylen-playback:0.0.1")' in script.read_text(encoding="utf-8")

    def test_the_kotlin_and_java_folders_of_the_app_compile_where_they_lie(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"platforms": {"android": {"sources": ["native/android"], "applicationClass": "com.haylen.tests.App"}}})
        (app / "native" / "android").mkdir(parents=True)
        folder = (app / "native" / "android").as_posix()

        script = (fixture.prepare(fixture.resolve(app)).project / "app" / "build.gradle.kts").read_text(encoding="utf-8")

        # The Kotlin compiler of the Android plugin compiles only the Kotlin folders, so a folder of the app is one of both kinds.
        assert f'        java.directories.add("{folder}")' in script
        assert f'        kotlin.directories.add("{folder}")' in script
