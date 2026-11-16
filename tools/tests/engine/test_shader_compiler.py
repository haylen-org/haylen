import hashlib
import io
import json
import os
import shutil
import subprocess
import tarfile
import zipfile
from pathlib import Path

import pytest

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.toolchains.shader_compiler import ShaderCompiler
from tests.support.recorded_commands import RecordedCommands


class ShaderCompilerTest:
    """The engine script `haylen-shader-compiler.cmake` provides the pinned compiler: the release binary of this machine, downloaded once into a folder of its commit and checked against its digest, or the commit built from source where the release has no binary. The tooling takes what the script provides, or the build that `HAYLEN_SHADER_COMPILER` names."""

    @staticmethod
    def archive(folder: Path, name: str, files: dict[str, str]) -> tuple[str, str]:
        """Writes an archive like the ones of the releases, a zip or a gzipped tar after its name, and returns its URL and SHA-256."""
        target = folder / "server" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if name.endswith(".zip"):
            with zipfile.ZipFile(target, "w") as package:
                for path, text in files.items():
                    package.writestr(path, text)
        else:
            with tarfile.open(target, "w:gz") as package:
                for path, text in files.items():
                    info = tarfile.TarInfo(path)
                    info.size = len(text.encode())
                    info.mode = 0o755
                    package.addfile(info, io.BytesIO(text.encode()))
        return target.as_uri(), hashlib.sha256(target.read_bytes()).hexdigest()

    @staticmethod
    def provide(folder: Path, pins: dict, environment: dict[str, str] | None = None) -> tuple[subprocess.CompletedProcess, Path | None]:
        """Runs the script of the engine with the pins as a machine would, and returns the run and the compiler that it named."""
        (folder / "pins.json").write_text(json.dumps(pins), encoding="utf-8")
        result = folder / "result"
        result.unlink(missing_ok=True)
        command = [
            "cmake",
            f"-DHAYLEN_SHADERC_CACHE={(folder / 'cache').as_posix()}",
            f"-DHAYLEN_SHADERC_RESULT={result.as_posix()}",
            f"-DHAYLEN_SHADERC_PINS={(folder / 'pins.json').as_posix()}",
            "-P",
            ShaderCompiler.SCRIPT,
        ]
        run = subprocess.run(command, capture_output=True, text=True, env={**os.environ, **(environment or {})}, check=False)
        return run, Path(result.read_text(encoding="utf-8")) if result.exists() else None

    @staticmethod
    def host() -> str:
        architecture = "x64" if Host.architecture() == "x86_64" else Host.architecture()
        return f"{Host.system()}-{architecture}"

    @pytest.fixture(autouse=True)
    def needs_cmake(self):
        if shutil.which("cmake") is None:
            pytest.skip('The command "cmake" runs the script of the engine.')

    def test_the_release_binary_of_the_machine_downloads_once_into_a_folder_of_its_commit(self, tmp_path):
        executable = Host.executable("haylen-shaderc")
        name = "haylen-shaderc-9.9.9-host.zip" if Host.system() == "windows" else "haylen-shaderc-9.9.9-host.tar.gz"
        url, digest = ShaderCompilerTest.archive(tmp_path, name, {executable: "compiler", "LICENSE": "license"})
        pins = {
            "version": "9.9.9",
            "commit": "abcdef0123456789",
            "source": {"url": "file:///missing.tar.gz", "sha256": "0" * 64},
            "binaries": {ShaderCompilerTest.host(): {"url": url, "sha256": digest}},
        }
        run, compiler = ShaderCompilerTest.provide(tmp_path, pins)
        assert run.returncode == 0, run.stderr
        assert compiler == tmp_path / "cache" / "haylen-shaderc-abcdef0" / executable
        assert compiler.read_text(encoding="utf-8") == "compiler"
        assert os.access(compiler, os.X_OK)
        assert [path.name for path in compiler.parent.iterdir()] == [executable]

        (tmp_path / "server").rename(tmp_path / "gone")
        run, again = ShaderCompilerTest.provide(tmp_path, pins)
        assert (run.returncode, again) == (0, compiler)

        # The release keeps its version when it is rebuilt at another commit, so the new pin downloads the new binary.
        url, digest = ShaderCompilerTest.archive(tmp_path, name, {executable: "rebuilt"})
        run, rebuilt = ShaderCompilerTest.provide(tmp_path, {**pins, "commit": "1234567890abcdef", "binaries": {ShaderCompilerTest.host(): {"url": url, "sha256": digest}}})
        assert run.returncode == 0, run.stderr
        assert rebuilt == tmp_path / "cache" / "haylen-shaderc-1234567" / executable
        assert rebuilt.read_text(encoding="utf-8") == "rebuilt"

    def test_a_download_that_differs_from_its_digest_fails_and_keeps_nothing(self, tmp_path):
        url, digest = ShaderCompilerTest.archive(tmp_path, "haylen-shaderc-9.9.9-host.tar.gz", {"haylen-shaderc": "compiler"})
        pins = {"version": "9.9.9", "commit": "abcdef0123456789", "source": {"url": url, "sha256": digest}, "binaries": {ShaderCompilerTest.host(): {"url": url, "sha256": "0" * 64}}}
        run, compiler = ShaderCompilerTest.provide(tmp_path, pins)
        assert run.returncode != 0
        assert "haylen-shaderc-9.9.9-host.tar.gz" in run.stderr
        assert compiler is None
        assert list((tmp_path / "cache" / "haylen-shaderc-abcdef0").iterdir()) == []

    def test_a_machine_without_a_binary_builds_the_commit_without_the_toolchain_of_a_cross_build(self, tmp_path):
        executable = Host.executable("haylen-shaderc")
        project = f"""cmake_minimum_required(VERSION 3.28)
project(compiler LANGUAGES NONE)
if(HAYLEN_SHADERC_BUILD_TESTS)
  message(FATAL_ERROR "The tests of the compiler are on.")
endif()
add_custom_target(haylen-shaderc ALL COMMAND ${{CMAKE_COMMAND}} -E copy ${{CMAKE_CURRENT_SOURCE_DIR}}/compiler ${{CMAKE_BINARY_DIR}}/bin/{executable})
"""
        sources = {"haylen-shader-compiler-abcdef0123456789/CMakeLists.txt": project, "haylen-shader-compiler-abcdef0123456789/compiler": "built"}
        url, digest = ShaderCompilerTest.archive(tmp_path, "abcdef0123456789.tar.gz", sources)
        pins = {"version": "9.9.9", "commit": "abcdef0123456789", "source": {"url": url, "sha256": digest}, "binaries": {"other-host": {"url": "file:///missing.tar.gz", "sha256": "0" * 64}}}
        run, compiler = ShaderCompilerTest.provide(tmp_path, pins, {"CMAKE_TOOLCHAIN_FILE": str(tmp_path / "missing-toolchain.cmake")})
        assert run.returncode == 0, run.stdout + run.stderr
        assert compiler == tmp_path / "cache" / "haylen-shaderc-abcdef0" / executable
        assert compiler.read_text(encoding="utf-8") == "built"
        assert [path.name for path in compiler.parent.iterdir()] == [executable]

    def test_the_tooling_takes_the_compiler_that_the_script_of_the_engine_provides(self, tmp_path, monkeypatch):
        script = tmp_path / "cmake" / "haylen-shader-compiler.cmake"
        script.parent.mkdir()
        shutil.copy2(ShaderCompiler.SCRIPT, script)
        executable = Host.executable("haylen-shaderc")
        url, digest = ShaderCompilerTest.archive(tmp_path, "haylen-shaderc-9.9.9-host.zip", {executable: "compiler"})
        pins = {"version": "9.9.9", "commit": "abcdef0123456789", "binaries": {ShaderCompilerTest.host(): {"url": url, "sha256": digest}}}
        (script.parent / "haylen-shader-compiler.json").write_text(json.dumps(pins), encoding="utf-8")
        monkeypatch.setattr(ShaderCompiler, "SCRIPT", script)
        monkeypatch.setattr(ShaderCompiler, "found", {})
        monkeypatch.setattr(Repository, "TOOLS", tmp_path / "tools")
        context = RecordedCommands(tmp_path).context
        monkeypatch.setattr(context.process, "run", lambda command, **options: subprocess.run([str(part) for part in command], check=True, capture_output=True))
        assert ShaderCompiler.path(context) == tmp_path / "tools" / "haylen-shaderc-abcdef0" / executable

    def test_the_variable_names_a_build_of_the_compiler_to_use_instead_of_the_pinned_one(self, tmp_path, monkeypatch):
        compiler = tmp_path / "haylen-shaderc"
        compiler.write_text("compiler", encoding="utf-8")
        recorded = RecordedCommands(tmp_path, {ShaderCompiler.VARIABLE: str(compiler)})
        monkeypatch.setattr(ShaderCompiler, "found", {})
        assert ShaderCompiler.path(recorded.context) == compiler.resolve()
        assert recorded.commands == []

        monkeypatch.setattr(ShaderCompiler, "found", {})
        with pytest.raises(MissingToolchain, match=ShaderCompiler.VARIABLE):
            ShaderCompiler.path(RecordedCommands(tmp_path, {ShaderCompiler.VARIABLE: str(tmp_path / "missing")}).context)

    def test_the_pins_name_the_archives_of_their_release_and_the_source_of_their_commit(self):
        pins = json.loads(ShaderCompiler.PIN.read_text(encoding="utf-8"))
        version, commit = pins["version"], pins["commit"]
        assert pins["source"]["url"].endswith(f"/archive/{commit}.tar.gz")
        for host, binary in pins["binaries"].items():
            assert f"/releases/download/v{version}/haylen-shaderc-{version}-" in binary["url"], host
            assert len(binary["sha256"]) == 64, host
        assert pins["binaries"]["macos-arm64"] == pins["binaries"]["macos-x64"]
