from types import MappingProxyType

from haylen.core.repository import Repository
from haylen.toolchains.android_ndk import AndroidNdk


class AndroidPlatform:
    """What the Android backend and the Android engine port both declare, so the registry matches the target of one with the target of the other: the system, the binary interfaces, the toolchain and the C++ runtime, the oldest API level, the graphics APIs and the capabilities of the system that the engine uses there."""

    SYSTEM = "android"
    PLATFORM = "android"
    TARGET = "android"
    # The architectures of Android are its binary interfaces, which the commands take by their names.
    ABIS = MappingProxyType({abi: abi for abi in AndroidNdk.ABIS})
    TOOLCHAIN = f"ndk-{AndroidNdk.VERSION}"
    RUNTIME = "c++_static"
    MINIMUM_API = AndroidNdk.MINIMUM_API
    GRAPHICS_APIS = ("vulkan", "openGlEs")
    SHADER_LANGUAGES = ("spirv", "glsl300es")
    CAPABILITIES = frozenset({"dynamicLibraries", "executableMemory", "processes"})
    RELEASE_PROFILE = "android"
    # The Gradle project of the Android libraries of the engine, and the libraries that it publishes as `dev.haylen:<module>`.
    LIBRARY_PROJECT = Repository.ENGINE / "platform" / "android"
    LIBRARIES = ("haylen", "haylen-plugins", "haylen-links", "haylen-coroutines")
    # The names of the files of an Android SDK in its manifest: the CMake package of each ABI, the player library of each ABI and the local Maven repository of the Android libraries.
    CMAKE_FILE = "cmake"
    PLAYER_FILE = "player"
    MAVEN_FILE = "maven"
