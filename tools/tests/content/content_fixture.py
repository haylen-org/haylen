from pathlib import Path

import pytest

from haylen.content.protected_releases import ProtectedReleases
from haylen.content.release_package import ReleasePackage
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from tests.content.recording_content_tool import RecordingContentTool
from tests.support.tooling_fixture import ToolingFixture


class ContentFixture:
    """Builds the protected releases of the tests in a temporary folder: the build folders of apps and the configuration folder of the user lie inside it, so no test writes into the checkout or reaches a key of the machine, and the content tool only records its commands."""

    def __init__(self, folder: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        self.folder = folder
        self.tooling = ToolingFixture(folder)
        monkeypatch.setattr(Repository, "APPS", folder / "apps")
        monkeypatch.setenv("HOME", str(folder / "home"))
        environment = {"APPDATA": str(folder / "home" / "AppData"), "XDG_CONFIG_HOME": str(folder / "home" / ".config")}
        base = self.tooling.context
        self.context = ToolingContext(base.terminal, base.process, 1, environment)
        self.tool = RecordingContentTool(self.context)
        self.releases = ProtectedReleases(self.context, self.tool)

    @property
    def output(self) -> str:
        return self.tooling.output.getvalue()

    def source(self, document: dict | None = None) -> ReleasePackage:
        app = self.tooling.app(document or {})
        target = self.tooling.registry().target("fake")
        return ReleasePackage.resolve(app, target, self.context, self.tooling.registry())

    def commands(self, name: str) -> list[list[str]]:
        return [command for command in self.tool.commands if command[0] == name]
