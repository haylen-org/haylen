from haylen.development.console_line import ConsoleLine


class ConsoleLineTest:
    def test_the_line_edits_recalls_and_completes_from_the_commands_of_the_app(self):
        line = ConsoleLine()
        line.commands = [{"name": "god", "arguments": ["boolean"]}, {"name": "gold", "arguments": ["integer"]}, {"name": "help", "arguments": ["string?"]}]
        for key in "gd":
            line.edit(key)
        line.edit("Left")
        line.edit("o")
        assert (line.text, line.cursor) == ("god", 2)

        line.edit("End")
        assert line.edit("Enter") == "god"
        assert line.edit("Enter") == ""
        line.edit("x")
        line.edit("Up")
        assert line.text == "god"
        line.edit("Down")
        assert line.text == "x"
        line.edit("ClearLine")

        line.edit("g")
        assert line.complete() == ["god", "gold"]
        assert line.text == "go"
        line.edit("d")
        assert line.complete() == []
        assert line.text == "god "
        line.edit("t")
        line.complete()
        assert line.text == "god true "
        line.replace("gold ")
        assert line.complete() == []
        assert line.text == "gold "
