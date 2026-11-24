import io
import os
import re
import shutil
from pathlib import Path

from haylen.terminal import Terminal


class TerminalTest:
    COLUMNS = (Terminal.Column("Name", shrink=1), Terminal.Column("State"), Terminal.Column("Identifier", style=Terminal.EXPRESSION, stacks=True))
    ROWS = (
        ("iPhone 17", Terminal.Cell("Booted", Terminal.Sign.READY), "F06B9DE9-BA9F-4923-A236-6A8C5F6BEFAC"),
        ("Apple TV 4K (3rd generation)", Terminal.Cell("Shutdown", Terminal.Sign.IDLE), "52A61E3A"),
        ("Tablet", "", "T1"),
    )

    class LatinStream(io.StringIO):
        """A stream whose encoding lacks the symbols of the signs, as a console of an old code page does."""

        @property
        def encoding(self) -> str:
            return "latin-1"

    @staticmethod
    def terminal(stream: io.StringIO, color: bool = False, width: int = 0) -> Terminal:
        terminal = Terminal(stream, stream, {"FORCE_COLOR": "1"} if color else {}, Path("test.log"))
        terminal.width = lambda: width
        return terminal

    def test_a_message_breaks_between_words_as_they_show_and_never_inside_a_reserved_expression(self):
        message = 'Run "haylen sdk build" once, then `build/sdk` holds it.'

        assert Terminal.wrap(message, 20, color=True) == ['Run "haylen sdk build"', "once, then `build/sdk`", "holds it."]
        assert Terminal.wrap(message, 20, color=False) == ["Run", '"haylen sdk build"', "once, then `build/sdk`", "holds it."]
        assert Terminal.wrap("A wordthatislongerthantheline fits.", 10, color=False) == ["A", "wordthatislongerthantheline", "fits."]

    def test_a_table_has_a_row_of_titles_and_columns_aligned_by_their_widest_text(self):
        output = io.StringIO()
        TerminalTest.terminal(output).table(TerminalTest.COLUMNS, TerminalTest.ROWS)

        assert output.getvalue().splitlines() == [
            "  Name                          State       Identifier",
            "  iPhone 17                     ● Booted    F06B9DE9-BA9F-4923-A236-6A8C5F6BEFAC",
            "  Apple TV 4K (3rd generation)  ○ Shutdown  52A61E3A",
            "  Tablet                        —           T1",
        ]

    def test_without_color_the_same_structure_prints_without_escapes_and_with_color_only_escapes_differ(self):
        plain = io.StringIO()
        colored = io.StringIO()
        TerminalTest.terminal(plain).grouped_table(TerminalTest.COLUMNS, [("iOS", TerminalTest.ROWS[:1]), ("tvOS", TerminalTest.ROWS[1:])])
        TerminalTest.terminal(colored, color=True).grouped_table(TerminalTest.COLUMNS, [("iOS", TerminalTest.ROWS[:1]), ("tvOS", TerminalTest.ROWS[1:])])

        assert "\x1b" not in plain.getvalue()
        assert Terminal.ESCAPE.sub("", colored.getvalue()) == plain.getvalue()
        assert "\x1b[1miOS\x1b[0m  \x1b[2m1\x1b[0m" in colored.getvalue()
        assert "\x1b[32m●\x1b[0m Booted" in colored.getvalue()
        assert f"\x1b[{Terminal.EXPRESSION}mF06B9DE9-BA9F-4923-A236-6A8C5F6BEFAC\x1b[0m" in colored.getvalue()

    def test_groups_print_their_title_and_count_apart_and_their_tables_line_up_from_one_group_to_the_next(self):
        output = io.StringIO()
        TerminalTest.terminal(output).grouped_table(TerminalTest.COLUMNS, [("iOS", TerminalTest.ROWS[:1]), ("tvOS", TerminalTest.ROWS[1:])])
        lines = output.getvalue().splitlines()

        assert lines[0] == "iOS  1"
        assert lines[3] == ""
        assert lines[4] == "tvOS  2"
        assert [line.index("Identifier") for line in (lines[1], lines[5])] == [44, 44]

    def test_a_narrow_terminal_wraps_the_columns_that_shrink_and_stacks_an_identifier_under_its_row(self):
        output = io.StringIO()
        TerminalTest.terminal(output, width=30).table(TerminalTest.COLUMNS, TerminalTest.ROWS)

        assert output.getvalue().splitlines() == [
            "  Name              State",
            "  iPhone 17         ● Booted",
            "    F06B9DE9-BA9F-4923-A236-6A8C5F6BEFAC",
            "  Apple TV 4K (3rd  ○ Shutdown",
            "  generation)",
            "    52A61E3A",
            "  Tablet            —",
            "    T1",
        ]

    def test_a_column_that_cuts_keeps_both_ends_of_its_text_and_no_line_passes_the_width(self):
        output = io.StringIO()
        columns = (Terminal.Column("Tool"), Terminal.Column("Path", shrink=1, cut=True))
        rows = [("cmake", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX27.2.sdk")]
        TerminalTest.terminal(output, width=40).table(columns, rows)
        lines = output.getvalue().splitlines()

        assert lines[1] == "  cmake  /Applications/X…/MacOSX27.2.sdk"
        assert all(Terminal.visible_width(line) <= 40 for line in lines)

    def test_a_stream_whose_encoding_lacks_the_symbols_prints_their_plain_characters(self):
        output = TerminalTest.LatinStream()
        terminal = TerminalTest.terminal(output)
        terminal.table(TerminalTest.COLUMNS, TerminalTest.ROWS)
        terminal.success("The tests passed.")

        printed = output.getvalue()
        assert re.search(r"iPhone 17 +\* Booted", printed)
        assert re.search(r"Tablet +- +T1", printed)
        assert printed.endswith("+ The tests passed.\n")

    def test_items_and_successes_carry_their_sign_and_break_under_the_start_of_their_text(self):
        output = io.StringIO()
        terminal = TerminalTest.terminal(output, width=40)
        terminal.item(Terminal.Sign.FAILED, 'The target "windows" builds only on windows, and this machine runs "macos".')
        terminal.success("The engine tests passed.")

        assert output.getvalue().splitlines() == [
            '  ✗ The target "windows" builds only on',
            "    windows, and this machine runs",
            '    "macos".',
            "✓ The engine tests passed.",
        ]

    def test_with_color_only_urls_underline_paths_print_bare_and_reserved_expressions_take_their_color(self):
        message = 'Open `https://haylen.dev/docs` and `build/sdk` with "haylen sdk build".'

        assert Terminal.render(message, True) == f"Open \x1b[{Terminal.LINK}mhttps://haylen.dev/docs\x1b[0m and build/sdk with \x1b[{Terminal.EXPRESSION}mhaylen sdk build\x1b[0m."
        assert Terminal.render(message, False) == 'Open https://haylen.dev/docs and build/sdk with "haylen sdk build".'

    def test_a_line_that_is_too_long_loses_its_middle_and_keeps_its_start_and_end_by_the_columns_they_take(self):
        assert Terminal.shorten("Compiling App.cpp", 17) == "Compiling App.cpp"
        assert Terminal.shorten("Compiling the sources/of/the/app/App.cpp", 20) == "Compiling \u2026p/App.cpp"
        assert Terminal.shorten("\u6f22\u5b57\u6f22\u5b57\u6f22\u5b57", 7) == "\u6f22\u2026\u6f22\u5b57"
        assert Terminal.shorten("cafe\u0301 au lait", 12) == "cafe\u0301 au lait"

    def test_the_progress_line_drops_the_escapes_and_tabs_of_a_tool_and_fits_the_width_of_the_terminal(self, monkeypatch):
        monkeypatch.setattr(shutil, "get_terminal_size", lambda: os.terminal_size((21, 24)))
        output = io.StringIO()
        TerminalTest.terminal(output, color=True).progress("\x1b[32mCompiling\x1b[0m\tsome/very/long/path/to/file.cpp\n")

        assert output.getvalue() == f"\r\x1b[2K\x1b[{Terminal.COMMAND}mCompiling \u2026/file.cpp\x1b[0m"
