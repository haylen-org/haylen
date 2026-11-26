import json
import re
from pathlib import Path

import pytest

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.engine.engine_tree import EngineTree
from haylen.engine.gcc_check import GccCheck
from haylen.toolchains.build_tools import BuildTools
from tests.support.recorded_commands import RecordedCommands


class GccCheckTest:
    """The check of the engine with the GCC of a Linux machine: it runs on Linux alone, configures a tree of its own with GCC and warnings as errors, and compiles the object of every compile command of that tree without linking."""

    def test_the_check_runs_on_a_linux_machine_alone(self, tmp_path, monkeypatch):
        for system in ("macos", "windows"):
            monkeypatch.setattr(Host, "system", staticmethod(lambda system=system: system))
            with pytest.raises(UnsupportedTarget, match=re.escape(f'and this machine runs "{system}". Run it on a Linux machine.')):
                GccCheck(RecordedCommands(tmp_path).context).run("Debug")

    def test_the_check_compiles_every_object_of_its_tree_with_gcc_and_links_nothing(self, tmp_path, monkeypatch):
        configured = []
        monkeypatch.setattr(Host, "system", staticmethod(lambda: "linux"))
        monkeypatch.setattr(Repository, "ENGINE_BUILDS", tmp_path)
        monkeypatch.setattr(EngineTree, "command", lambda self, configuration, *options: ["cmake", *options])
        monkeypatch.setattr(EngineTree, "ensure", lambda self, command, env=None: configured.append(list(command)))
        monkeypatch.setattr(type(BuildTools.NINJA), "require", lambda self: Path(f"/usr/bin/{self.name}"))
        tree = tmp_path / "linux-debug-gcc"
        tree.mkdir()
        entries = [
            {"directory": str(tree), "file": "/repo/My Folder/a.cpp", "output": str(tree / "objects/My Folder/a.cpp.o")},
            {"directory": str(tree), "file": "/repo/b.cpp", "output": "objects/b.cpp.o"},
        ]
        (tree / "compile_commands.json").write_text(json.dumps(entries), encoding="utf-8")

        recorded = RecordedCommands(tmp_path)
        result = GccCheck(recorded.context).run("Debug")

        assert result["tree"] == str(tree)
        assert "-DCMAKE_CXX_COMPILER=/usr/bin/g++" in configured[0] and "-DHAYLEN_WARNINGS_AS_ERRORS=ON" in configured[0], "The tree builds with the GCC of the machine and warnings as errors."
        assert recorded.commands[0] == ["/usr/bin/ninja", "-C", str(tree), "build.ninja"], "A tree whose CMake files changed generates again before the check reads its compile commands."
        assert recorded.commands[1] == ["/usr/bin/ninja", "-C", str(tree), "-f", "objects.ninja", "-j", "4", "-k", "0", "haylen-objects"], (
            "The check goes on after a failure and builds only the objects."
        )
        assert (tree / "objects.ninja").read_text(encoding="utf-8").splitlines() == [
            "# Written by the tooling for the check of every source with GCC.",
            "include build.ninja",
            "build haylen-objects: phony objects/My$ Folder/a.cpp.o objects/b.cpp.o",
        ]
