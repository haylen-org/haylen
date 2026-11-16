from haylen.core.repository import Repository


class AppleSdk:
    """The SDKs of Xcode that the engine builds the slices of Apple platforms with, by the names of the slices of `Haylen.xcframework`, and the oldest system of each platform."""

    # The engine formats text and logs with `std::format` with floating point, which reaches the C++ library of the system in macOS 13.3 and in iOS and tvOS 16.3. Mac Catalyst takes its minimum, the iOS version, from `engine/cmake/haylen-catalyst.toolchain.cmake`.
    MINIMUM_VERSIONS = {"iOS": "16.3", "tvOS": "16.3", "visionOS": "1.0", "macOS": "13.3"}
    SLICE_SYSTEMS = {"macos": "macOS", "ios": "iOS", "ios-simulator": "iOS", "tvos": "tvOS", "tvos-simulator": "tvOS", "xros": "visionOS", "xros-simulator": "visionOS"}
    SLICE_SDKS = {
        "macos": "macosx",
        "ios": "iphoneos",
        "ios-simulator": "iphonesimulator",
        "ios-maccatalyst": "macosx",
        "tvos": "appletvos",
        "tvos-simulator": "appletvsimulator",
        "xros": "xros",
        "xros-simulator": "xrsimulator",
    }
    CATALYST = "ios-maccatalyst"
    CATALYST_TOOLCHAIN = Repository.ENGINE / "cmake" / "haylen-catalyst.toolchain.cmake"

    @staticmethod
    def cmake_options(slice_name: str, architecture: str) -> list[str]:
        """The CMake options that build the engine for one slice and architecture."""
        if slice_name == AppleSdk.CATALYST:
            return [f"-DCMAKE_TOOLCHAIN_FILE={AppleSdk.CATALYST_TOOLCHAIN}", f"-DCMAKE_OSX_ARCHITECTURES={architecture}"]
        system = AppleSdk.SLICE_SYSTEMS[slice_name]
        options = [f"-DCMAKE_OSX_ARCHITECTURES={architecture}", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={AppleSdk.MINIMUM_VERSIONS[system]}", f"-DCMAKE_OSX_SYSROOT={AppleSdk.SLICE_SDKS[slice_name]}"]
        if system != "macOS":
            options.append(f"-DCMAKE_SYSTEM_NAME={system}")
        return options
