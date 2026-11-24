import re

from haylen.cli.commands.test import TestCommand
from haylen.core.repository import Repository


class GraphicsApiNamesTest:
    # A row of the table of the names of the graphics APIs that every device of the engine names its API with.
    ROW = re.compile(r'\{"(\w+)", Api::\w+\}')

    def test_the_tests_take_every_graphics_api_of_the_engine(self):
        source = (Repository.ENGINE / "include" / "haylen" / "gpu" / "Device.hpp").read_text(encoding="utf-8")
        table = source[source.index("kApiNames") :]
        names = GraphicsApiNamesTest.ROW.findall(table[: table.index("}};")])
        assert sorted(TestCommand.GRAPHICS_APIS) == sorted(name for name in names if name not in ("none", "port"))
