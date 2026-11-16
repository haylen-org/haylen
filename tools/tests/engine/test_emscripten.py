from pathlib import Path

import pytest

from haylen.native.cmake_tree import CMakeTree
from haylen.toolchains.emscripten import Emscripten
from tests.support.recorded_commands import RecordedCommands


class EmscriptenTest:
    """The pinned Emscripten installs into `.tools/emsdk` once, an SDK of another version takes the newest releases and installs the pin, and a web build tree that another version configured builds again from nothing, since Emscripten keeps no ABI between versions."""

    @staticmethod
    def installed(tmp_path: Path, monkeypatch: pytest.MonkeyPatch, version: str) -> list[list[str]]:
        monkeypatch.setattr(Emscripten, "ROOT", tmp_path / "emsdk")
        Emscripten.emcmake().parent.mkdir(parents=True)
        Emscripten.emcmake().write_text("", encoding="utf-8")
        (Emscripten.emcmake().parent / "emscripten-version.txt").write_text(f'"{version}"\n', encoding="utf-8")
        recorded = RecordedCommands(tmp_path)
        Emscripten.install(recorded.context)
        return recorded.commands

    def test_the_pinned_version_is_used_as_it_is(self, tmp_path, monkeypatch):
        assert EmscriptenTest.installed(tmp_path, monkeypatch, Emscripten.VERSION) == []
        assert Emscripten.status().ready

    def test_another_version_takes_the_newest_releases_and_installs_the_pinned_one(self, tmp_path, monkeypatch):
        commands = EmscriptenTest.installed(tmp_path, monkeypatch, "1.0.0")
        assert commands[0][:2] == ["git", "fetch"]
        assert [command[1:] for command in commands[2:]] == [["install", Emscripten.VERSION], ["activate", Emscripten.VERSION]]
        assert "Emscripten 1.0.0" in Emscripten.status().problem

    def test_a_web_tree_of_another_version_builds_again_from_nothing(self, tmp_path, monkeypatch):
        EmscriptenTest.installed(tmp_path, monkeypatch, Emscripten.VERSION)
        recorded = RecordedCommands(tmp_path)
        tree = CMakeTree(recorded.context, tmp_path / "build" / "web-release")
        tree.folder.mkdir(parents=True)
        (tree.folder / "CMakeCache.txt").write_text("HAYLEN_SHADERC:FILEPATH=haylen-shaderc\n", encoding="utf-8")
        (tree.folder / "object.o").write_bytes(b"object")
        (tree.folder / Emscripten.MARKER).write_text("1.0.0", encoding="utf-8")
        assert Emscripten.is_stale(tree.folder)

        Emscripten.configure(recorded.context, tree, ["cmake", "-B", tree.folder])
        assert recorded.commands == [[str(Emscripten.emcmake()), "cmake", "-B", str(tree.folder)]]
        assert not (tree.folder / "object.o").exists()
        (tree.folder / "CMakeCache.txt").write_text("HAYLEN_SHADERC:FILEPATH=haylen-shaderc\n", encoding="utf-8")
        assert not Emscripten.is_stale(tree.folder)
        assert tree.configured
