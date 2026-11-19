from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.host import Host
from haylen.platforms.tool_status import ToolStatus
from haylen.toolchains.android_sdk import AndroidSdk


class AndroidNdk:
    """The pinned NDK inside the Android SDK, which the engine, its dependencies and the native libraries of apps build with, whatever other NDK the environment names, since the engine builds against its headers."""

    VERSION = "30.0.16248370"
    # The miniaudio library plays through AAudio from Android 8.1 on, and the engine builds it without OpenSL ES, like the `minSdk` of the Android library.
    MINIMUM_API = 27
    # The ABIs of the engine: 32-bit ARM keeps the Android TV devices that still run it, and `x86_64` serves emulators.
    ABIS = ("arm64-v8a", "armeabi-v7a", "x86_64")

    def __init__(self, sdk: AndroidSdk) -> None:
        self.sdk = sdk

    @property
    def folder(self) -> Path:
        candidate = self.sdk.folder / "ndk" / AndroidNdk.VERSION
        if not candidate.is_dir():
            raise MissingToolchain(f"The Android NDK {AndroidNdk.VERSION} was not found in `{self.sdk.folder / 'ndk'}`. Install it with \"sdkmanager 'ndk;{AndroidNdk.VERSION}'\".")
        return candidate

    def cmake_options(self, abi: str) -> list[str]:
        """The CMake options that build for one ABI with the NDK, which the engine and the native libraries of apps share."""
        toolchain = self.folder / "build" / "cmake" / "android.toolchain.cmake"
        return [
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
            f"-DANDROID_ABI={abi}",
            f"-DANDROID_PLATFORM=android-{AndroidNdk.MINIMUM_API}",
            "-DANDROID_STL=c++_static",
            "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
        ]

    def tool(self, name: str) -> Path:
        """A tool of the LLVM toolchain of the NDK, such as `llvm-readelf`, from the prebuilt folder of this host, the only one that the NDK of a host holds."""
        tools = sorted((self.folder / "toolchains" / "llvm" / "prebuilt").glob(f"*/bin/{Host.executable(name)}"))
        if not tools:
            raise MissingToolchain(f'The Android NDK {AndroidNdk.VERSION} in `{self.folder}` has no "{name}". Install it again with "sdkmanager \'ndk;{AndroidNdk.VERSION}\'".')
        return tools[0]

    def status(self) -> ToolStatus:
        sdk = self.sdk.find()
        candidate = sdk / "ndk" / AndroidNdk.VERSION if sdk else None
        if candidate is not None and candidate.is_dir():
            return ToolStatus("Android NDK", ToolStatus.Role.HOST, AndroidNdk.VERSION, candidate, None)
        problem = f"The Android NDK {AndroidNdk.VERSION} is not installed. Install it with \"sdkmanager 'ndk;{AndroidNdk.VERSION}'\"."
        return ToolStatus("Android NDK", ToolStatus.Role.HOST, None, None, problem)
