import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.platforms.tool_status import ToolStatus


@dataclass(frozen=True)
class PathTool:
    """A build tool that this machine provides on its path, such as CMake, Ninja or clang-format, which the tooling finds and never installs. The install hint says how to get it."""

    # The first start of a tool takes under a second even on the cold disk of a fresh CI runner, so the bound only tells a command that hangs from one that a busy machine starts slowly.
    VERSION_SECONDS = 60

    name: str
    install: str

    def find(self) -> Path | None:
        found = shutil.which(self.name)
        return Path(found) if found else None

    def require(self) -> Path:
        found = self.find()
        if found is None:
            raise MissingToolchain(f'The command "{self.name}" was not found. {self.install}')
        return found

    def status(self) -> ToolStatus:
        """Tells whether the tool is on the path and its version, which the first line of `--version` names, or why it does not answer."""
        found = self.find()
        if found is None:
            return ToolStatus(self.name, ToolStatus.Role.HOST, None, None, f'The command "{self.name}" was not found. {self.install}')
        try:
            completed = subprocess.run([found, "--version"], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=PathTool.VERSION_SECONDS, check=False)
        except subprocess.TimeoutExpired:
            problem = f'The command "{self.name}" at `{found}` did not answer "--version" within {PathTool.VERSION_SECONDS} seconds, so it hangs or the machine is too busy to start it.'
            return ToolStatus(self.name, ToolStatus.Role.HOST, None, found, problem)
        except OSError as error:
            problem = f'The command "{self.name}" at `{found}` does not start, and the system says "{error.strerror or error}". Install it again.'
            return ToolStatus(self.name, ToolStatus.Role.HOST, None, found, problem)
        lines = completed.stdout.strip().splitlines()
        return ToolStatus(self.name, ToolStatus.Role.HOST, lines[0] if lines else None, found, None)
