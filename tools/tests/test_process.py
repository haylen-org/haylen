import sys

from haylen.process import Process
from haylen.terminal import Terminal


class ProcessTest:
    def test_a_tool_that_streams_with_structured_output_keeps_the_standard_output_for_the_json_document(self, tmp_path):
        with (tmp_path / "stdout").open("w+", encoding="utf-8") as stdout, (tmp_path / "stderr").open("w+", encoding="utf-8") as stderr:
            terminal = Terminal(stdout, stderr, {}, tmp_path / "command.log", structured=True)
            terminal.verbose = True
            Process(terminal).run([sys.executable, "-c", "print('The tool streams this line.')"], cwd=tmp_path)

        assert (tmp_path / "stdout").read_text(encoding="utf-8") == ""
        assert "The tool streams this line." in (tmp_path / "stderr").read_text(encoding="utf-8")
