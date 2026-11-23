import subprocess
from types import SimpleNamespace

from haylen.engine.benchmarks import Benchmarks
from tests.support.recorded_commands import RecordedCommands


class BenchmarksTest:
    def test_the_runs_end_with_the_result_of_every_graphics_api(self, tmp_path, monkeypatch):
        recorded = RecordedCommands(tmp_path)
        codes = {"metal": 0, "vulkan": Benchmarks.UNAVAILABLE}
        monkeypatch.setattr(subprocess, "run", lambda command, **options: SimpleNamespace(returncode=codes[command[-1]]))

        started = Benchmarks(recorded.context).run_on("sprites", tmp_path / "benchmark", ["metal", "vulkan"])

        assert started == ["metal"]
        lines = recorded.output.getvalue().splitlines()
        assert lines[-4:] == [
            'Results of the "sprites" benchmark  2',
            "  Graphics API  Result",
            "  metal         ✓ Ran",
            "  vulkan        ○ Cannot start on this machine",
        ]
