import shutil
from collections.abc import Mapping
from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.host import Host
from haylen.platforms.tool_status import ToolStatus


class Java:
    """The JDK that Gradle and the Android tools use: the one of `JAVA_HOME`, or else the one on the path."""

    def __init__(self, environment: Mapping[str, str]) -> None:
        self.environment = environment

    def find(self, name: str) -> Path | None:
        home = self.environment.get("JAVA_HOME")
        if home and (Path(home) / "bin" / Host.executable(name)).is_file():
            return Path(home) / "bin" / Host.executable(name)
        found = shutil.which(name, path=self.environment.get("PATH"))
        return Path(found) if found else None

    def tool(self, name: str) -> Path:
        """A command of the JDK, such as `java` or `keytool`."""
        found = self.find(name)
        if found is None:
            raise MissingToolchain(f'The command "{name}" of the JDK was not found. Install JDK 17 or newer and set "JAVA_HOME" to its folder.')
        return found

    def status(self) -> ToolStatus:
        found = self.find("java")
        problem = None if found else 'No JDK was found. Install JDK 17 or newer and set "JAVA_HOME" to its folder.'
        return ToolStatus("JDK", ToolStatus.Role.HOST, None, found, problem)
