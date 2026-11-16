from pathlib import Path

from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.engine.coverage import Coverage
from haylen.engine.engine_tree import EngineTree
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.llvm import Llvm
from tests.support.recorded_commands import RecordedCommands


class CoverageTest:
    def test_the_report_reads_the_tests_by_the_name_of_their_executable_on_this_machine(self, tmp_path, monkeypatch):
        for system, executable in (("windows", "haylen-tests.exe"), ("linux", "haylen-tests")):
            monkeypatch.setattr(Repository, "ENGINE_BUILDS", tmp_path / system)
            monkeypatch.setattr(Host, "system", staticmethod(lambda system=system: system))
            monkeypatch.setattr(EngineTree, "ensure", lambda self, command, env=None: None)
            monkeypatch.setattr(EngineTree, "command", lambda self, configuration, *options: ["cmake", *options])
            monkeypatch.setattr(EngineTree, "build", lambda self, targets, configuration, *arguments: None)
            monkeypatch.setattr(Llvm, "tool", staticmethod(lambda name, context: Path(name)))
            monkeypatch.setattr(type(BuildTools.CTEST), "require", lambda self: Path(self.name))
            recorded = RecordedCommands(tmp_path)

            # The run of the tests writes a profile, as the instrumented tests do.
            def record(command, recorded=recorded, **options):
                recorded.run(command, **options)
                if str(command[0]) == "ctest":
                    profiles = Path(command[2]) / "coverage"
                    profiles.mkdir(parents=True, exist_ok=True)
                    (profiles / "haylen-1.profraw").write_bytes(b"")

            monkeypatch.setattr(recorded.context.process, "run", record)
            Coverage(recorded.context).run(None)
            binary = str(tmp_path / system / "coverage" / "bin" / executable)
            assert [command[2] for command in recorded.commands if command[0] == "llvm-cov"] == [binary, binary]
