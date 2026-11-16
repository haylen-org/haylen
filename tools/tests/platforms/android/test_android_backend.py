import json

from haylen.content.protected_releases import ProtectedReleases
from haylen.core.configuration import Configuration
from haylen.core.repository import Repository
from haylen.platforms.android.android_backend import AndroidBackend
from haylen.platforms.android.android_gradle_build import AndroidGradleBuild
from haylen.platforms.android.android_port import AndroidPort
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.sdk_manifest import SdkManifest
from tests.content.recording_content_tool import RecordingContentTool
from tests.platforms.android.android_fixture import AndroidFixture
from tests.support.tooling_fixture import ToolingFixture


class AndroidBackendTest:
    def test_the_backend_and_the_port_serve_the_target_by_what_they_declare(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        registry = BackendRegistry(fixture.context, [("android", lambda: AndroidBackend)], [("android", lambda: AndroidPort)])

        port, target = registry.port_for(registry.target("android").description)

        assert registry.problems == []
        assert (port.name, target.name) == ("android", "android")
        assert sorted(registry.contribution_types("android")) == ["assetPacks", "gradleModule", "gradlePlugin", "manifestPlaceholder", "mavenRepository", "nativeLibrary", "projectFile"]
        assert list(registry.override_sections()) == ["android"]

    def test_the_port_keeps_the_slices_of_the_same_sources_and_drops_the_others(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        port = AndroidPort(fixture.context)
        request = SdkRequest(("android",), Configuration.RELEASE, ("x86_64",), ToolingFixture.REVISION, fixture.sdk.folder)
        (fixture.sdk.folder / "abis" / "arm64-v8a").mkdir(parents=True)

        assert [entry.abi for entry in port.kept_slices(request)] == ["arm64-v8a", "armeabi-v7a", "x86_64"]
        assert (fixture.sdk.folder / "abis" / "arm64-v8a").is_dir()

        other = SdkRequest(("android",), Configuration.RELEASE, ("x86_64",), "b" * 64, fixture.sdk.folder)
        assert port.kept_slices(other) == ()
        assert not (fixture.sdk.folder / "abis").exists()
        assert not (fixture.sdk.folder / SdkManifest.FILE).exists()

    def test_a_lua_release_ships_its_protected_release_with_a_library_of_its_own(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        monkeypatch.setattr(Repository, "APPS", tmp_path / "apps")
        monkeypatch.setenv("HOME", str(tmp_path / "home"))
        tool = RecordingContentTool(fixture.context)
        monkeypatch.setattr(AndroidBackend, "releases", lambda backend: ProtectedReleases(backend.context, tool))
        app = fixture.app({})

        prepared = fixture.prepare(fixture.resolve(app, "Release"))
        project = prepared.project
        cmake = (project / "native" / "CMakeLists.txt").read_text(encoding="utf-8")
        manifest = (project / "app" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")

        assert sorted(path.name for path in (project / "app" / "src" / "main" / "assets" / "app").iterdir()) == sorted(["app.hmanifest", "content.hmanifest", f"{'ab' * 32}.hpak"])
        assert (project / "native" / "HaylenBootstrap.cpp").read_bytes() == RecordingContentTool.BOOTSTRAP
        assert (project / "native" / "HaylenBootstrap.cpp").stat().st_mode & 0o777 == 0o600
        assert 'haylen_add_app(haylen-app LUA CONTENT "${CMAKE_CURRENT_LIST_DIR}/../app/src/main/assets/app" BOOTSTRAP "${CMAKE_CURRENT_LIST_DIR}/HaylenBootstrap.cpp")' in cmake
        assert 'android:value="haylen-app"' in manifest
        assert prepared.state.extra[AndroidGradleBuild.LIBRARY] == "haylen-app"
        assert not (project / "app" / "src" / "main" / "assets" / "app" / "haylen-package-index.json").exists(), "A release ships no package index."

        again = fixture.prepare(fixture.resolve(app, "Release"))
        assert not again.changed

    def test_a_debug_build_takes_the_debug_key_of_the_app_and_a_release_the_upload_key(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({})
        commands = []
        monkeypatch.setattr(fixture.context.process, "run", lambda command, **options: commands.append((command, options["env"])))
        monkeypatch.setattr(AndroidGradleBuild, "newest", lambda folder, pattern: tmp_path / "built.apk")
        (tmp_path / "built.apk").write_bytes(b"apk")
        monkeypatch.setattr("haylen.toolchains.gradle.Gradle.path", lambda context: tmp_path / "gradle")
        prepared = fixture.prepare(fixture.resolve(app))

        product = AndroidGradleBuild(fixture.context).run(prepared)

        command, environment = commands[0]
        assert command[2:4] == [prepared.project, ":app:assembleDebug"]
        assert "--max-workers=1" in command
        assert not any(name.startswith("HAYLEN_DEBUG_") for name in environment)
        assert product.path == prepared.folders.products / "com.haylen.tests.apk"
        assert [component.target.abi for component in product.components] == ["arm64-v8a", "armeabi-v7a", "x86_64"]
        assert json.loads((prepared.project / "app" / "src" / "main" / "assets" / "haylen-plugins.json").read_text(encoding="utf-8")) == {"plugins": []}
