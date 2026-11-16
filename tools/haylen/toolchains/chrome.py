import shutil
from collections.abc import Mapping
from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.platforms.tool_status import ToolStatus


class Chrome:
    """The Chrome or Chromium of this machine, or the one that `HAYLEN_CHROME` names, which the web checks and the GPU tests of the web open pages in without a window."""

    VARIABLE = "HAYLEN_CHROME"
    CANDIDATES = (
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "google-chrome",
        "google-chrome-stable",
        "chromium",
        "chromium-browser",
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    )

    @staticmethod
    def find(environment: Mapping[str, str]) -> Path | None:
        named = environment.get(Chrome.VARIABLE)
        for candidate in [named] if named else Chrome.CANDIDATES:
            found = shutil.which(candidate)
            if found:
                return Path(found)
            if Path(candidate).is_file():
                return Path(candidate)
        return None

    @staticmethod
    def require(environment: Mapping[str, str]) -> Path:
        found = Chrome.find(environment)
        if found is None:
            raise MissingToolchain(f'No Chrome or Chromium was found for the checks of the web. Install one, or name it with "{Chrome.VARIABLE}".')
        return found

    @staticmethod
    def status(environment: Mapping[str, str]) -> ToolStatus:
        found = Chrome.find(environment)
        problem = None if found else f'No Chrome or Chromium was found for the checks of the web. Install one, or name it with "{Chrome.VARIABLE}".'
        return ToolStatus("Chrome", ToolStatus.Role.HOST, None, found, problem)
