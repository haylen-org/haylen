import subprocess
from pathlib import Path

from haylen.platforms.tool_status import ToolStatus
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.xcodegen import XcodeGen


class XcodeTools:
    """Finds the tools of Xcode that the Apple backend and engine port need on this Mac without changing anything: xcodebuild with its version, the SDK of a platform, which Xcode installs with the platform, CMake and Ninja for the engine and native libraries, and the pinned XcodeGen once a preparation downloaded it."""

    XCODE_HINT = 'Install Xcode from the App Store and select it with "sudo xcode-select --switch /Applications/Xcode.app".'

    @staticmethod
    def capture(command: list[str]) -> subprocess.CompletedProcess:
        try:
            return subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            return subprocess.CompletedProcess(command, 1, "", str(error))

    @staticmethod
    def xcode() -> ToolStatus:
        completed = XcodeTools.capture(["xcodebuild", "-version"])
        if completed.returncode != 0:
            return ToolStatus("Xcode", ToolStatus.Role.HOST, None, None, f'The command "xcodebuild" does not run. {XcodeTools.XCODE_HINT}')
        lines = completed.stdout.strip().splitlines()
        developer = XcodeTools.capture(["xcode-select", "--print-path"]).stdout.strip()
        return ToolStatus("Xcode", ToolStatus.Role.HOST, lines[0] if lines else None, Path(developer) if developer else None, None)

    @staticmethod
    def sdk(name: str, platform: str) -> ToolStatus:
        """The SDK of Xcode that builds for a platform, such as `iphonesimulator`, which Xcode installs with the platform."""
        completed = XcodeTools.capture(["xcrun", "--sdk", name, "--show-sdk-version"])
        path = XcodeTools.capture(["xcrun", "--sdk", name, "--show-sdk-path"]).stdout.strip()
        if completed.returncode != 0 or not path:
            problem = f'Xcode has no SDK "{name}". Install the platform "{platform}" in the components of the settings of Xcode, or with "xcodebuild -downloadPlatform {platform}".'
            return ToolStatus(f"SDK {name}", ToolStatus.Role.TARGET, None, None, problem)
        return ToolStatus(f"SDK {name}", ToolStatus.Role.TARGET, completed.stdout.strip(), Path(path), None)

    @staticmethod
    def host_tools() -> list[ToolStatus]:
        """CMake and Ninja, and XcodeGen once it is in `.tools`, since the first preparation of an Apple project downloads the pinned release."""
        tools = [BuildTools.CMAKE.status(), BuildTools.NINJA.status()]
        xcodegen = XcodeGen.status()
        if xcodegen.ready:
            tools.append(xcodegen)
        return tools
