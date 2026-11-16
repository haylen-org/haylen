from pathlib import Path

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.engine.engine_tree import EngineTree
from haylen.native.cmake_tree import CMakeTree
from haylen.toolchains.shader_compiler import ShaderCompiler
from tests.support.recorded_commands import RecordedCommands


class EngineTreeTest:
    """A build tree of the engine configures again when the shader compiler of another pin or another uv configured it, or when its configure failed, so its shaders and tests never use an old tool."""

    @staticmethod
    def tree(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, compiler: Path, uv: Path) -> tuple[EngineTree, RecordedCommands]:
        monkeypatch.setattr(Repository, "ENGINE_BUILDS", tmp_path / "engine")
        monkeypatch.setattr(ShaderCompiler, "found", {"compiler": compiler})
        monkeypatch.setattr(EngineTree, "uv", lambda self: uv)
        recorded = RecordedCommands(tmp_path)
        return EngineTree(recorded.context, "macos-debug"), recorded

    @staticmethod
    def cache(tree: EngineTree, text: str) -> None:
        tree.folder.mkdir(parents=True, exist_ok=True)
        (tree.folder / CMakeTree.CONFIGURED).write_text("", encoding="utf-8")
        (tree.folder / "CMakeCache.txt").write_text(text, encoding="utf-8")

    def test_another_shader_compiler_or_uv_configures_the_tree_again(self, tmp_path, monkeypatch):
        compiler, uv = tmp_path / "haylen-shaderc-new" / "haylen-shaderc", tmp_path / "bin" / "uv"
        tree, recorded = EngineTreeTest.tree(tmp_path, monkeypatch, compiler, uv)
        cases = {
            f"HAYLEN_SHADERC:FILEPATH={tmp_path / 'haylen-shaderc-old' / 'haylen-shaderc'}\nHAYLEN_UV:FILEPATH={uv}\n": 1,
            f"HAYLEN_SHADERC:FILEPATH={compiler}\nHAYLEN_UV:FILEPATH={uv}\n": 0,
            f"HAYLEN_SHADERC:FILEPATH={compiler}\nHAYLEN_UV:FILEPATH=/usr/bin/uv\n": 1,
            f"HAYLEN_SHADERC:FILEPATH={compiler}\n": 1,
        }
        for cache, configures in cases.items():
            EngineTreeTest.cache(tree, cache)
            recorded.commands.clear()
            tree.ensure(["cmake", "-B", tree.folder])
            assert len(recorded.commands) == configures, cache

    def test_a_tree_whose_configure_failed_configures_again(self, tmp_path, monkeypatch):
        compiler, uv = tmp_path / "haylen-shaderc", tmp_path / "uv"
        tree, recorded = EngineTreeTest.tree(tmp_path, monkeypatch, compiler, uv)
        EngineTreeTest.cache(tree, f"HAYLEN_SHADERC:FILEPATH={compiler}\nHAYLEN_UV:FILEPATH={uv}\n")
        assert tree.is_current()

        # A configure that fails leaves the cache of CMake without the record of a configured tree, so the next command configures again instead of building.
        def fail(command, **_):
            raise BuildFailed("The configure failed.")

        monkeypatch.setattr(recorded.context.process, "run", fail)
        with pytest.raises(BuildFailed):
            tree.configure(["cmake", "-B", tree.folder])
        assert not tree.is_current()

    def test_each_sanitizer_and_generator_has_a_tree_of_its_own(self):
        names = {
            EngineTree.host_name("Debug"),
            EngineTree.host_name("Debug", "address"),
            EngineTree.host_name("Debug", "thread"),
            EngineTree.host_name("Debug", suffix="xcode"),
            EngineTree.host_name("Release"),
        }
        assert len(names) == 5

    def test_the_tree_hands_the_engine_tests_the_shader_compiler_and_uv(self, tmp_path, monkeypatch):
        compiler, uv = tmp_path / "haylen-shaderc", tmp_path / "uv"
        tree, _ = EngineTreeTest.tree(tmp_path, monkeypatch, compiler, uv)
        command = [str(part) for part in tree.command("Debug", "-DHAYLEN_BUILD_TESTS=ON")]
        assert command[:5] == ["cmake", "-S", str(Repository.ROOT), "-B", str(tree.folder)]
        assert f"-DHAYLEN_SHADERC={compiler.as_posix()}" in command
        assert f"-DHAYLEN_UV={uv.as_posix()}" in command
        assert command[-1] == "-DHAYLEN_BUILD_TESTS=ON"
