from pathlib import Path

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.tool_status import ToolStatus
from haylen.toolchains.download import Download


class XcodeGen:
    """The pinned XcodeGen, which generates Xcode projects from their `project.yml`. Its release zip unpacks into `.tools/xcodegen-<version>`, with the setting presets next to its binary, so a new pin downloads its own release."""

    VERSION = "2.46.0"
    SHA256 = "4d9e34b62172d645eed6457cac13fc222569974098ef4ee9c3368bedf0196806"
    FOLDER = Repository.TOOLS / f"xcodegen-{VERSION}"
    EXECUTABLE = FOLDER / "xcodegen" / "bin" / "xcodegen"
    URL = f"https://github.com/yonaskolb/XcodeGen/releases/download/{VERSION}/xcodegen.zip"

    @staticmethod
    def path(context: ToolingContext) -> Path:
        """The executable, which the first call downloads."""
        if XcodeGen.EXECUTABLE.exists():
            return XcodeGen.EXECUTABLE
        if Host.system() != "macos":
            raise UnsupportedTarget(f'XcodeGen generates Xcode projects, which build only on macOS, and this machine runs "{Host.system()}".')
        Download.archive(XcodeGen.URL, XcodeGen.SHA256, XcodeGen.FOLDER, XcodeGen.EXECUTABLE, "", context.terminal)
        return XcodeGen.EXECUTABLE

    @staticmethod
    def status() -> ToolStatus:
        """Tells whether the pinned XcodeGen is in `.tools` without downloading it, for `doctor`."""
        if XcodeGen.EXECUTABLE.exists():
            return ToolStatus("XcodeGen", ToolStatus.Role.HOST, XcodeGen.VERSION, XcodeGen.EXECUTABLE, None)
        problem = f'XcodeGen {XcodeGen.VERSION} is not in ".tools" yet. The first build that generates an Xcode project downloads it.'
        return ToolStatus("XcodeGen", ToolStatus.Role.HOST, None, None, problem)
