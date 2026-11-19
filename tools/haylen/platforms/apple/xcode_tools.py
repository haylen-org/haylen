import subprocess
from pathlib import Path

from haylen.platforms.tool_status import ToolStatus
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.xcodegen import XcodeGen


class XcodeTools:
    """Finds the tools of Xcode that the Apple backend and engine port need on this Mac without changing anything: xcodebuild with its version, the SDK of a platform, which Xcode installs with the platform, CMake and Ninja for the engine and native libraries, and the pinned XcodeGen once a preparation downloaded it."""

    XCODE_HINT = 'Install Xcode from the App Store and select it with "sudo xcode-select --switch /Applications/Xcode.app".'

    # The tools of Xcode answer in under a second, so the bound only tells a command that hangs from one that a busy machine starts slowly.
    ANSWER_SECONDS = 60

    @staticmethod
    def capture(command: list[str]) -> subprocess.CompletedProcess | str:
        """Runs a tool of Xcode, or tells in a sentence why it gave no answer: it did not start, or it did not answer within the bound."""
        try:
            return subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=XcodeTools.ANSWER_SECONDS, check=False)
        except subprocess.TimeoutExpired:
            return f'The command "{command[0]}" did not answer "{" ".join(command)}" within {XcodeTools.ANSWER_SECONDS} seconds, so it hangs or the machine is too busy to start it.'
        except OSError as error:
            return f'The command "{command[0]}" does not start, and the system says "{error.strerror or error}".'

    @staticmethod
    def xcode() -> ToolStatus:
        completed = XcodeTools.capture(["xcodebuild", "-version"])
        if isinstance(completed, str):
            return ToolStatus("Xcode", ToolStatus.Role.HOST, None, None, f"{completed} {XcodeTools.XCODE_HINT}")
        if completed.returncode != 0:
            said = completed.stderr.strip().splitlines()
            reason = f', and it says "{said[0]}"' if said else ""
            return ToolStatus("Xcode", ToolStatus.Role.HOST, None, None, f'The command "xcodebuild -version" failed{reason}. {XcodeTools.XCODE_HINT}')

        lines = completed.stdout.strip().splitlines()
        developer = XcodeTools.capture(["xcode-select", "--print-path"])
        folder = developer.stdout.strip() if isinstance(developer, subprocess.CompletedProcess) else ""
        return ToolStatus("Xcode", ToolStatus.Role.HOST, lines[0] if lines else None, Path(folder) if folder else None, None)

    @staticmethod
    def sdk(name: str, platform: str) -> ToolStatus:
        """The SDK of Xcode that builds for a platform, such as `iphonesimulator`, which Xcode installs with the platform."""
        version = XcodeTools.capture(["xcrun", "--sdk", name, "--show-sdk-version"])
        if isinstance(version, str):
            return ToolStatus(f"SDK {name}", ToolStatus.Role.TARGET, None, None, version)
        path = XcodeTools.capture(["xcrun", "--sdk", name, "--show-sdk-path"])
        if isinstance(path, str):
            return ToolStatus(f"SDK {name}", ToolStatus.Role.TARGET, None, None, path)

        folder = path.stdout.strip()
        if version.returncode != 0 or path.returncode != 0 or not folder:
            problem = f'Xcode has no SDK "{name}". Install the platform "{platform}" in the components of the settings of Xcode, or with "xcodebuild -downloadPlatform {platform}".'
            return ToolStatus(f"SDK {name}", ToolStatus.Role.TARGET, None, None, problem)
        return ToolStatus(f"SDK {name}", ToolStatus.Role.TARGET, version.stdout.strip(), Path(folder), None)

    @staticmethod
    def host_tools() -> list[ToolStatus]:
        """CMake and Ninja, and XcodeGen once it is in `.tools`, since the first preparation of an Apple project downloads the pinned release."""
        tools = [BuildTools.CMAKE.status(), BuildTools.NINJA.status()]
        xcodegen = XcodeGen.status()
        if xcodegen.ready:
            tools.append(xcodegen)
        return tools
