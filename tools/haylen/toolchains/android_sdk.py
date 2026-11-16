import re
from collections.abc import Mapping
from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.host import Host
from haylen.platforms.tool_status import ToolStatus


class AndroidSdk:
    """The Android SDK of this machine, which `ANDROID_HOME` or `ANDROID_SDK_ROOT` names, with its platform tools, emulator, build tools and platforms. The tooling never installs it or accepts its licenses."""

    VARIABLES = ("ANDROID_HOME", "ANDROID_SDK_ROOT")
    PLATFORM = re.compile(r"android-(\d+)(\.\d+)?")

    def __init__(self, environment: Mapping[str, str]) -> None:
        self.environment = environment

    def find(self) -> Path | None:
        for variable in AndroidSdk.VARIABLES:
            value = self.environment.get(variable)
            if value and Path(value).is_dir():
                return Path(value)
        return None

    @property
    def folder(self) -> Path:
        found = self.find()
        if found is None:
            raise MissingToolchain('The Android SDK was not found. Install it with Android Studio and set "ANDROID_HOME" to its folder.')
        return found

    @property
    def adb(self) -> Path:
        return self.folder / "platform-tools" / Host.executable("adb")

    @property
    def emulator(self) -> Path:
        return self.folder / "emulator" / Host.executable("emulator")

    def build_tool(self, name: str) -> Path:
        """A tool of the newest build tools that hold it, such as `aapt2`, `zipalign` or `apksigner`, which is a script on Windows."""
        candidates = [path for path in (self.folder / "build-tools").glob(f"*/{name}*") if path.stem == name and path.suffix in ("", ".exe", ".bat")]
        tools = sorted(candidates, key=lambda path: [int(part) if part.isdigit() else 0 for part in path.parent.name.split(".")])
        if not tools:
            raise MissingToolchain(f'The Android SDK has no build tools with "{name}". Install them with the SDK Manager of Android Studio or with "sdkmanager".')
        return tools[-1]

    def platform_jar(self) -> tuple[Path, int]:
        """The `android.jar` of the newest platform of the SDK and its API level."""
        platforms = []
        for jar in (self.folder / "platforms").glob("android-*/android.jar"):
            match = AndroidSdk.PLATFORM.fullmatch(jar.parent.name)
            if match:
                platforms.append((int(match.group(1)), jar.parent.name, jar))
        if not platforms:
            raise MissingToolchain('The Android SDK has no platform. Install one with the SDK Manager of Android Studio or with "sdkmanager".')
        level, _, jar = max(platforms)
        return jar, level

    def status(self) -> tuple[ToolStatus, ...]:
        """Tells what this machine has of the SDK and its platform tools, for `doctor`."""
        found = self.find()
        if found is None:
            problem = 'The Android SDK was not found. Install it with Android Studio and set "ANDROID_HOME" to its folder.'
            return (ToolStatus("Android SDK", ToolStatus.Role.HOST, None, None, problem),)
        adb = found / "platform-tools" / Host.executable("adb")
        adb_problem = None if adb.is_file() else 'The Android SDK has no platform tools. Install them with the SDK Manager of Android Studio or with "sdkmanager platform-tools".'
        return (
            ToolStatus("Android SDK", ToolStatus.Role.HOST, None, found, None),
            ToolStatus("adb", ToolStatus.Role.HOST, None, adb if adb.is_file() else None, adb_problem),
        )
