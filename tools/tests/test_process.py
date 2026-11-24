import io
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

    def test_a_tool_that_streams_prints_its_command_line_only_when_the_terminal_is_verbose(self, tmp_path):
        command = [sys.executable, "-c", "pass"]
        printed = []
        for verbose in (False, True):
            output = io.StringIO()
            terminal = Terminal(output, output, {}, tmp_path / "command.log")
            terminal.verbose = verbose
            Process(terminal).run(command, cwd=tmp_path, stream=True)
            printed.append(output.getvalue())

        assert printed == ["", f"$ {Terminal.command_line(command)}\n"]

    def test_a_quiet_tool_writes_its_command_line_into_the_log_and_never_into_the_terminal(self, tmp_path):
        command = [sys.executable, "-c", "print('The tool writes this line.')"]
        output = io.StringIO()
        Process(Terminal(output, output, {}, tmp_path / "command.log")).run(command, cwd=tmp_path)

        assert output.getvalue() == ""
        assert (tmp_path / "command.log").read_text(encoding="utf-8") == f"$ {Terminal.command_line(command)}\nThe tool writes this line.\n"
