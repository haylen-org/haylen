from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.target_environment import TargetEnvironment
from haylen.platforms.apple.apple_target import AppleTarget
from haylen.toolchains.apple_sdk import AppleSdk


class AppleTargets:
    """The targets of the Apple backend and the engine port, one per slice of `Haylen.xcframework`. The visionOS simulator runs arm64 alone, since the OpenSSL that the engine builds for visionOS takes no architecture and comes out arm64."""

    # Mac Catalyst counts its versions like iOS, and its first version whose C++ library formats floating point is 16.4, the one of macOS 13.3, which `engine/cmake/haylen-catalyst.toolchain.cmake` names too.
    CATALYST_MINIMUM = "16.4"
    MAC_CAPABILITIES = frozenset({"dynamicLibraries", "executableMemory", "processes"})
    CATALYST_CAPABILITIES = frozenset({"dynamicLibraries", "processes"})
    MOBILE_CAPABILITIES = frozenset({"dynamicLibraries"})

    ALL = (
        AppleTarget(
            name="macos",
            system="macos",
            platform="macos",
            environment=TargetEnvironment.HOST,
            slice="macos",
            sdk="macosx",
            architectures=("arm64", "x86_64"),
            minimum_version=AppleSdk.MINIMUM_VERSIONS["macOS"],
            capabilities=MAC_CAPABILITIES,
            shader_languages=("msl-macos",),
            xcframework_platform="macos",
            xcframework_variant=None,
            xcode_platform="macOS",
            folder="macos",
            frameworks="macOS",
            device_family="",
            products="",
            destination="generic/platform=macOS",
            family=None,
            description="A macOS app for this Mac and other Macs.",
        ),
        AppleTarget(
            name="catalyst",
            system="catalyst",
            platform="catalyst",
            environment=TargetEnvironment.HOST,
            slice="ios-maccatalyst",
            sdk="macosx",
            architectures=("arm64", "x86_64"),
            minimum_version=CATALYST_MINIMUM,
            capabilities=CATALYST_CAPABILITIES,
            shader_languages=("msl-ios",),
            xcframework_platform="ios",
            xcframework_variant="maccatalyst",
            xcode_platform="iOS",
            folder="ios",
            frameworks="macCatalyst",
            device_family="",
            products="-maccatalyst",
            destination="generic/platform=macOS,variant=Mac Catalyst",
            family=None,
            description="The iPad app as a Mac app with Mac Catalyst.",
        ),
        AppleTarget(
            name="ios",
            system="ios",
            platform="ios",
            environment=TargetEnvironment.DEVICE,
            slice="ios",
            sdk="iphoneos",
            architectures=("arm64",),
            minimum_version=AppleSdk.MINIMUM_VERSIONS["iOS"],
            capabilities=MOBILE_CAPABILITIES,
            shader_languages=("msl-ios",),
            xcframework_platform="ios",
            xcframework_variant=None,
            xcode_platform="iOS",
            folder="ios",
            frameworks="iOS",
            device_family="",
            products="-iphoneos",
            destination="generic/platform=iOS",
            family="iOS",
            description="An app for iPhones and iPads.",
        ),
        AppleTarget(
            name="ios-simulator",
            system="ios",
            platform="ios",
            environment=TargetEnvironment.SIMULATOR,
            slice="ios-simulator",
            sdk="iphonesimulator",
            architectures=("arm64", "x86_64"),
            minimum_version=AppleSdk.MINIMUM_VERSIONS["iOS"],
            capabilities=MOBILE_CAPABILITIES,
            shader_languages=("msl-ios-simulator",),
            xcframework_platform="ios",
            xcframework_variant="simulator",
            xcode_platform="iOS",
            folder="ios",
            frameworks="iOS",
            device_family="",
            products="-iphonesimulator",
            destination="generic/platform=iOS Simulator",
            family="iOS",
            description="The iPhone and iPad app on the simulators of Xcode.",
        ),
        AppleTarget(
            name="tvos",
            system="tvos",
            platform="tvos",
            environment=TargetEnvironment.DEVICE,
            slice="tvos",
            sdk="appletvos",
            architectures=("arm64",),
            minimum_version=AppleSdk.MINIMUM_VERSIONS["tvOS"],
            capabilities=MOBILE_CAPABILITIES,
            shader_languages=("msl-ios",),
            xcframework_platform="tvos",
            xcframework_variant=None,
            xcode_platform="tvOS",
            folder="tvos",
            frameworks="tvOS",
            device_family="3",
            products="-appletvos",
            destination="generic/platform=tvOS",
            family="tvOS",
            description="An app for the Apple TV.",
        ),
        AppleTarget(
            name="tvos-simulator",
            system="tvos",
            platform="tvos",
            environment=TargetEnvironment.SIMULATOR,
            slice="tvos-simulator",
            sdk="appletvsimulator",
            architectures=("arm64", "x86_64"),
            minimum_version=AppleSdk.MINIMUM_VERSIONS["tvOS"],
            capabilities=MOBILE_CAPABILITIES,
            shader_languages=("msl-ios-simulator",),
            xcframework_platform="tvos",
            xcframework_variant="simulator",
            xcode_platform="tvOS",
            folder="tvos",
            frameworks="tvOS",
            device_family="3",
            products="-appletvsimulator",
            destination="generic/platform=tvOS Simulator",
            family="tvOS",
            description="The Apple TV app on the simulators of Xcode.",
        ),
        AppleTarget(
            name="visionos",
            system="visionos",
            platform="visionos",
            environment=TargetEnvironment.DEVICE,
            slice="xros",
            sdk="xros",
            architectures=("arm64",),
            minimum_version=AppleSdk.MINIMUM_VERSIONS["visionOS"],
            capabilities=MOBILE_CAPABILITIES,
            shader_languages=("msl-ios",),
            xcframework_platform="xros",
            xcframework_variant=None,
            xcode_platform="visionOS",
            folder="visionos",
            frameworks="visionOS",
            device_family="7",
            products="-xros",
            destination="generic/platform=visionOS",
            family="visionOS",
            description="An app for the Apple Vision Pro.",
        ),
        AppleTarget(
            name="visionos-simulator",
            system="visionos",
            platform="visionos",
            environment=TargetEnvironment.SIMULATOR,
            slice="xros-simulator",
            sdk="xrsimulator",
            architectures=("arm64",),
            minimum_version=AppleSdk.MINIMUM_VERSIONS["visionOS"],
            capabilities=MOBILE_CAPABILITIES,
            shader_languages=("msl-ios-simulator",),
            xcframework_platform="xros",
            xcframework_variant="simulator",
            xcode_platform="visionOS",
            folder="visionos",
            frameworks="visionOS",
            device_family="7",
            products="-xrsimulator",
            destination="generic/platform=visionOS Simulator",
            family="xrOS",
            description="The Apple Vision Pro app on the simulators of Xcode.",
        ),
    )

    @staticmethod
    def named(name: str) -> AppleTarget:
        for target in AppleTargets.ALL:
            if target.name == name:
                return target
        raise UnsupportedTarget(f'The Apple backend has no target "{name}".')

    @staticmethod
    def of_slice(slice_name: str) -> AppleTarget:
        for target in AppleTargets.ALL:
            if target.slice == slice_name:
                return target
        raise UnsupportedTarget(f'The Apple engine port has no slice "{slice_name}".')

    @staticmethod
    def of_system(system: str, environment: TargetEnvironment) -> AppleTarget:
        """The target of the system and environment of a resolved target, which name exactly one Apple target."""
        for target in AppleTargets.ALL:
            if (target.system, target.environment) == (system, environment):
                return target
        raise UnsupportedTarget(f'The Apple backend has no target for "{system}" on "{environment.value}".')
