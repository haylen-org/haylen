import json
from collections.abc import Sequence
from pathlib import Path

from haylen.content.content_tool import ContentTool
from haylen.core.tooling_context import ToolingContext


class RecordingContentTool(ContentTool):
    """A content tool of the tests that records every command and writes what the real tool leaves behind: the keys of `keys create`, a release with its manifests and a shard for `build`, the shipped part and the folder of each pack of `packs` for `stage`, and the bootstrap source for `bootstrap`."""

    BOOTSTRAP = b"// The bootstrap of the tests.\n"

    def __init__(self, context: ToolingContext) -> None:
        super().__init__(context, Path("haylen-content"))
        self.commands: list[list[str]] = []
        self.packs: tuple[str, ...] = ()

    @staticmethod
    def option(command: list[str], name: str) -> Path:
        return Path(command[command.index(f"--{name}") + 1])

    def run(self, arguments: Sequence[object]) -> None:
        command = [str(argument) for argument in arguments]
        self.commands.append(command)
        if command[:2] == ["keys", "create"]:
            folder = Path(command[2])
            folder.mkdir(parents=True)
            (folder / "keys.json").write_text(json.dumps({"identifier": command[-1], "contentKeys": [], "signingKey": "0" * 64}), encoding="utf-8")
        elif command[0] == "build":
            output = RecordingContentTool.option(command, "output")
            output.mkdir(parents=True)
            for name in ("app.hmanifest", "content.hmanifest", f"{'ab' * 32}.hpak"):
                (output / name).write_bytes(b"release")
        elif command[0] == "stage":
            output = RecordingContentTool.option(command, "output")
            output.mkdir(parents=True)
            (output / "app.hmanifest").write_bytes(b"release")
            for pack in self.packs if "--packs" in command else ():
                folder = RecordingContentTool.option(command, "packs") / pack
                folder.mkdir(parents=True)
                (folder / f"{pack}.hpak").write_bytes(b"shard")
        elif command[0] == "bootstrap":
            output = RecordingContentTool.option(command, "output")
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(RecordingContentTool.BOOTSTRAP)

    def capture(self, arguments: Sequence[object]) -> str:
        self.commands.append([str(argument) for argument in arguments])
        return "The report of the tool.\n"
