import argparse
import re

from haylen.cli.command_tree import CommandTree
from haylen.core.repository import Repository


class CommandDocsTest:
    """The reference of the commands has a section for every command of the command line that names each of its options, and the guide of the agents names every command of the tree, so neither falls behind the parser."""

    COMMANDS_PAGE = Repository.ROOT / "docs" / "tools" / "commands.md"
    AGENTS = Repository.ROOT / "AGENTS.md"

    @staticmethod
    def options(command_class: type) -> list[list[str]]:
        """The names of each option of a command, such as "-o" and "--output" of one option."""
        parser = argparse.ArgumentParser(add_help=False)
        command_class().configure(parser)
        return [action.option_strings for action in parser._actions if action.option_strings]

    @staticmethod
    def sections() -> dict[str, str]:
        text = CommandDocsTest.COMMANDS_PAGE.read_text(encoding="utf-8")
        parts = re.split(r"^### (.+)$", text, flags=re.MULTILINE)
        return {parts[index].strip(): parts[index + 1].split("\n## ", 1)[0] for index in range(1, len(parts), 2)}

    @staticmethod
    def names(text: str, word: str) -> bool:
        return re.search(rf"(?<![\w-]){re.escape(word)}(?![\w-])", text) is not None

    def test_every_command_has_a_section_of_the_commands_page(self):
        sections = CommandDocsTest.sections()
        missing = [" ".join(command.PATH) for command in CommandTree.COMMANDS if " ".join(command.PATH) not in sections]
        assert missing == []

    def test_the_section_of_every_command_names_each_of_its_options(self):
        sections = CommandDocsTest.sections()
        missing = []
        for command in CommandTree.COMMANDS:
            title = " ".join(command.PATH)
            section = sections.get(title, "")
            for names in CommandDocsTest.options(command):
                if not any(CommandDocsTest.names(section, name) for name in names):
                    missing.append(f"{title} {names[-1]}")
        assert missing == []

    def test_the_paragraph_of_the_command_line_in_the_agents_guide_names_every_command(self):
        text = CommandDocsTest.AGENTS.read_text(encoding="utf-8")
        paragraph = next(line for line in text.splitlines() if line.startswith("- **The command haylen.**"))
        missing = []
        for command in CommandTree.COMMANDS:
            *group, word = command.PATH
            scope = paragraph
            if group:
                listed = re.search(rf"`{re.escape(' '.join(group))}` \(([^)]*)\)", paragraph)
                scope = listed.group(1) if listed else ""
            if f"`{word}`" not in scope:
                missing.append(" ".join(command.PATH))
        assert missing == []
