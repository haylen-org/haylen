from dataclasses import dataclass
from pathlib import Path

from haylen.platforms.desktop.desktop_app import DesktopApp


@dataclass(frozen=True)
class DesktopLayout:
    """Where an installed Windows or Linux app keeps its executable, its package and its native libraries: next to each other on Windows, and in `lib/<executable>` on Linux, with the native libraries in `lib` beside the executable and `bin/<executable>` linking to it."""

    folder: Path
    home: str
    executable_path: str
    libraries_path: str

    @staticmethod
    def of(platform: str, app: DesktopApp, folder: Path) -> DesktopLayout:
        if platform == "windows":
            return DesktopLayout(folder, "", f"{app.executable}.exe", "")
        home = f"lib/{app.executable}"
        return DesktopLayout(folder, home, f"bin/{app.executable}", f"{home}/lib")

    @property
    def executable(self) -> Path:
        return self.folder / self.executable_path

    @property
    def package(self) -> str:
        """The path of the folder of the package inside the installed app, where the runtime opens it."""
        return f"{self.home}/app" if self.home else "app"

    @property
    def libraries(self) -> Path:
        return self.folder / self.libraries_path
