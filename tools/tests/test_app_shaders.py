import json
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

from haylen.assets.app_shaders import AppShaders
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.selected_sdk import SelectedSdk
from haylen.toolchains.shader_compiler import ShaderCompiler
from tests.support.tooling_fixture import ToolingFixture


class AppShadersTest:
    """The shaders of an app compile with the shader compiler and, for the target of a port, through the shader tool that the SDK of the port carries, which here is the one of the test port."""

    # A compiler of the tests that writes a package whose one program has the SPIR-V target that the shader tool of the test port copies.
    COMPILER = (
        "import json, sys\n"
        "output = sys.argv[sys.argv.index('--output') + 1]\n"
        "package = {'programs': {'glow': {'targets': {'spirv': {'vertex': {'source': 0, 'entry': 'main'}}}}}}\n"
        "open(output, 'w', encoding='utf-8').write(json.dumps(package))\n"
    )

    @staticmethod
    def app(fixture: ToolingFixture, monkeypatch: pytest.MonkeyPatch) -> Path:
        compiler = fixture.folder / "haylen-shaderc"
        compiler.write_text(f"#!{sys.executable}\n{AppShadersTest.COMPILER}", encoding="utf-8")
        compiler.chmod(0o755)
        monkeypatch.setattr(ShaderCompiler, "path", staticmethod(lambda context: compiler))
        app = fixture.app({})
        (app / "content" / "shaders").mkdir(parents=True)
        (app / "content" / "shaders" / "glow.glsl").write_text("@program glow haylen_vs glow_fs\n", encoding="utf-8")
        return app

    @staticmethod
    def port_sdk(fixture: ToolingFixture) -> SelectedSdk:
        folder = fixture.folder / "port-sdk"
        (folder / "bin").mkdir(parents=True)
        shutil.copy2(Repository.ENGINE / "tests" / "data" / "port" / "port-shaders.cmake", folder / "bin" / "port-shaders.cmake")
        slice_entry = {
            "system": "test-port",
            "environment": "host",
            "architectures": [Host.architecture()],
            "abi": f"test-port-{Host.architecture()}",
            "toolchain": "test-port-cc",
            "runtime": "host-c++",
            "minimumSystemVersion": "1.0",
            "graphicsApis": ["port"],
            "capabilities": [],
            "libraries": {"haylen": "lib/libhaylen.a", "haylen-runtime": "lib/libhaylen-runtime.a", "haylen-lua": "lib/libhaylen-lua.a", "haylen-player": "lib/haylen-player.o"},
        }
        tool = {"name": "port-shaders.cmake", "system": Host.system(), "architecture": Host.architecture(), "path": "bin/port-shaders.cmake"}
        document = {
            "format": 1,
            "port": "test-port",
            "portVersion": "1.0.0",
            "engineVersion": Repository.engine_version(),
            "revision": ToolingFixture.REVISION,
            "configuration": "Debug",
            "slices": [slice_entry],
            "hostTools": [tool],
        }
        (folder / SdkManifest.FILE).write_text(json.dumps(document), encoding="utf-8")
        manifest = SdkManifest.load(folder)
        return SelectedSdk(manifest, manifest.slices[0])

    @staticmethod
    def targets(app: Path) -> dict:
        return json.loads((app / "content" / "shaders" / "glow.shader").read_text(encoding="utf-8"))["programs"]["glow"]["targets"]

    @pytest.mark.skipif(sys.platform == "win32", reason="The compiler of the test is a script with a shebang.")
    def test_the_shader_tool_of_a_port_gives_every_package_its_port_target(self, tmp_path, monkeypatch):
        fixture = ToolingFixture(tmp_path)
        app = AppShadersTest.app(fixture, monkeypatch)
        AppShaders(fixture.context).update(app)
        assert sorted(AppShadersTest.targets(app)) == ["spirv"]

        # A package that is newer than its sources but lacks the port target compiles again for the target of the port.
        port = AppShadersTest.port_sdk(fixture)
        assert AppShaders(fixture.context, port).update(app) == [app / "content" / "shaders" / "glow.glsl"]
        targets = AppShadersTest.targets(app)
        assert targets["port"] == targets["spirv"]
        assert AppShaders(fixture.context, port).update(app) == []

    def test_a_source_that_goes_away_while_the_folder_is_read_is_skipped_and_never_compiles(self, tmp_path, monkeypatch):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        shaders = app / "content" / "shaders"
        shaders.mkdir(parents=True)
        (shaders / "glow.glsl").write_text("@program glow haylen_vs glow_fs\n", encoding="utf-8")
        gone = shaders / "gone.glsl"
        listed = Path.rglob

        def rglob(folder: Path, pattern: str):
            yield from listed(folder, pattern)
            if folder == shaders:
                yield gone

        def compiler(*arguments: object, **options: object) -> None:
            raise AssertionError("The compiler ran for a source that went away.")

        monkeypatch.setattr(Path, "rglob", rglob)
        monkeypatch.setattr(subprocess, "run", compiler)

        assert AppShaders.sources(app) == [shaders / "glow.glsl"]
        assert AppShaders.newest_input(app) >= (shaders / "glow.glsl").stat().st_mtime
        AppShaders(fixture.context).compile(gone, gone.with_suffix(".shader"), None)
        assert not gone.with_suffix(".shader").exists()
