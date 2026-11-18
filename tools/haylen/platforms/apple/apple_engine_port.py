import plistlib
import shutil
from pathlib import Path
from types import MappingProxyType

from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.native.cmake_tree import CMakeTree
from haylen.native.engine_sdk_tree import EngineSdkTree
from haylen.platforms.apple.apple_sdk_files import AppleSdkFiles
from haylen.platforms.apple.apple_target import AppleTarget
from haylen.platforms.apple.apple_targets import AppleTargets
from haylen.platforms.apple.xcode_tools import XcodeTools
from haylen.platforms.contract import Contract
from haylen.platforms.engine_port import EnginePort
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.port_capability import PortCapability
from haylen.platforms.port_description import PortDescription
from haylen.platforms.sdk_join_request import SdkJoinRequest
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.sdk_folder import SdkFolder
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_slice import SdkSlice
from haylen.toolchains.apple_sdk import AppleSdk


class AppleEnginePort(EnginePort):
    """The engine port of the Apple platforms, which builds the slices of the Apple SDK, one per system and environment: the engine and the runtime of every architecture of a slice merged in its `libhaylen.a` and the Lua player in its `libhaylen-player.a`, joined with `lipo` and gathered into `Haylen.xcframework` and `HaylenPlayer.xcframework`, both signed ad hoc so Xcode takes them as signed. The SDK folder keeps the slices that it holds of the same engine sources and configuration, so building one slice later adds it to the frameworks, and SDKs that hold different slices, such as SDKs that several machines built side by side, join into one. The SDK holds the headers, the CMake package of native libraries, the settings of Apple projects, the privacy manifest of the engine and the Swift helpers of the bridge."""

    NAME = "apple"
    VERSION = "1.0.0"
    TOOLS = ("xcodebuild", "cmake", "ninja", "haylen-shaderc")
    BUILD = "haylen-framework"
    FRAMEWORK_COMPONENT = "haylen-framework"
    SDK_COMPONENT = "haylen-sdk"

    def describe(self) -> PortDescription:
        targets = tuple(target.port_target() for target in AppleTargets.ALL)
        return PortDescription(AppleEnginePort.NAME, Contract.ENGINE_PORT, AppleEnginePort.VERSION, targets, AppleEnginePort.TOOLS, frozenset({PortCapability.JOIN}))

    def check_environment(self, target: str) -> EnvironmentReport:
        if Host.system() != "macos":
            return EnvironmentReport((), (f'The engine port "{AppleEnginePort.NAME}" builds the Apple SDK only on macOS, and this machine runs "{Host.system()}".',))
        apple = AppleTargets.of_slice(target)
        tools = [XcodeTools.xcode(), XcodeTools.sdk(apple.sdk, apple.xcode_platform), *XcodeTools.host_tools()]
        return EnvironmentReport(tuple(tools), ())

    def build_sdk(self, request: SdkRequest) -> SdkManifest:
        targets = [AppleTargets.of_slice(name) for name in request.targets]
        folder = SdkFolder(request.folder, AppleEnginePort.NAME, AppleEnginePort.VERSION, request.revision, request.configuration)
        folder.begin()
        kept = folder.kept()
        configuration = request.configuration.value

        built: dict[str, tuple[AppleTarget, Path, Path]] = {}
        component: Path | None = None
        for target in targets:
            architectures = AppleEnginePort.architectures(target, request.architectures)
            components = [self.build_architecture(target, architecture, configuration) for architecture in architectures]
            component = component or components[0]
            joined = Repository.ENGINE_BUILDS / f"apple-{target.slice}-{configuration.lower()}"
            library = self.join([path / "lib" / AppleSdkFiles.LIBRARY_NAME for path in components], joined / AppleSdkFiles.LIBRARY_NAME)
            player = self.join([path / "lib" / AppleSdkFiles.PLAYER_LIBRARY_NAME for path in components], joined / AppleSdkFiles.PLAYER_LIBRARY_NAME)
            built[target.slice] = (target, library, player)
        if component is None:
            raise UnsupportedTarget("The request names no slice of the Apple SDK to build.")
        installed = self.install_headers(component.parent, configuration)

        # The libraries of the slices that stay come out of the frameworks before they are made again.
        staging = request.folder.parent / f"{request.folder.name}.slices"
        Folders.remove(staging)
        slices: dict[str, tuple[AppleTarget, Path, Path, tuple[str, ...]]] = {}
        for entry in kept.slices if kept else ():
            target = AppleTargets.of_system(entry.system, entry.environment)
            if target.slice in built:
                continue
            copy = staging / target.slice
            copy.mkdir(parents=True)
            library = shutil.copy2(request.folder / entry.files[AppleSdkFiles.LIBRARY], copy / AppleSdkFiles.LIBRARY_NAME)
            player = shutil.copy2(request.folder / entry.files[AppleSdkFiles.PLAYER_LIBRARY], copy / AppleSdkFiles.PLAYER_LIBRARY_NAME)
            slices[target.slice] = (target, Path(library), Path(player), entry.architectures)
        for name, (target, library, player) in built.items():
            slices[name] = (target, library, player, AppleEnginePort.architectures(target, request.architectures))

        shared = AppleEnginePort.install_shared(request.folder, installed, component)
        manifest = self.create_sdk(request.folder, request.revision, request.configuration, slices, installed / "include", shared)
        Folders.remove(staging)
        return manifest

    def join_sdks(self, request: SdkJoinRequest) -> SdkManifest:
        """Gathers the slices of the SDKs into the frameworks of a new SDK, with the shared files of the first SDK, which every SDK of the same engine sources holds alike."""
        sources = {manifest.folder for manifest in request.manifests}
        if request.folder.resolve() in sources:
            raise InvalidOption(f"The joined SDK goes into `{Repository.shown(request.folder)}`, which holds one of the SDKs to join. Name another folder.")

        slices: dict[str, tuple[AppleTarget, Path, Path, tuple[str, ...]]] = {}
        for manifest in request.manifests:
            for entry in manifest.slices:
                target = AppleTargets.of_system(entry.system, entry.environment)
                if target.slice in slices:
                    raise UnsupportedTarget(f'More than one of the SDKs to join holds the slice "{target.slice}". Join SDKs that hold different slices.')
                library = manifest.folder / entry.files[AppleSdkFiles.LIBRARY]
                player = manifest.folder / entry.files[AppleSdkFiles.PLAYER_LIBRARY]
                slices[target.slice] = (target, library, player, entry.architectures)

        first = request.manifests[0]
        Folders.remove(request.folder)
        request.folder.mkdir(parents=True)
        shared: dict[str, str] = {}
        for name, relative in first.files.items():
            if name in (AppleSdkFiles.FRAMEWORK, AppleSdkFiles.PLAYER):
                continue
            source = first.folder / relative
            if source.is_dir():
                shutil.copytree(source, request.folder / relative)
            else:
                (request.folder / relative).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, request.folder / relative)
            shared[name] = relative
        shared[AppleSdkFiles.FRAMEWORK] = AppleSdkFiles.FRAMEWORK_NAME
        shared[AppleSdkFiles.PLAYER] = AppleSdkFiles.PLAYER_NAME
        return self.create_sdk(request.folder, first.revision, first.configuration, slices, first.file(AppleSdkFiles.INCLUDE), shared)

    def create_sdk(
        self, folder: Path, revision: str, configuration: Configuration, slices: dict[str, tuple[AppleTarget, Path, Path, tuple[str, ...]]], include: Path, shared: dict[str, str]
    ) -> SdkManifest:
        """Creates the frameworks of an SDK folder from the libraries of its slices and writes its manifest with its shared files."""
        ordered = [slices[target.slice] for target in AppleTargets.ALL if target.slice in slices]
        framework = folder / AppleSdkFiles.FRAMEWORK_NAME
        player_framework = folder / AppleSdkFiles.PLAYER_NAME
        self.create_framework(framework, [(library, include) for _, library, _, _ in ordered])
        self.create_framework(player_framework, [(player, None) for _, _, player, _ in ordered])

        identifiers = AppleEnginePort.identifiers(framework)
        player_identifiers = AppleEnginePort.identifiers(player_framework)
        entries = []
        for target, _, _, architectures in ordered:
            key = (target.xcframework_platform, target.xcframework_variant)
            files = {
                AppleSdkFiles.LIBRARY: f"{AppleSdkFiles.FRAMEWORK_NAME}/{identifiers[key]}/{AppleSdkFiles.LIBRARY_NAME}",
                AppleSdkFiles.PLAYER_LIBRARY: f"{AppleSdkFiles.PLAYER_NAME}/{player_identifiers[key]}/{AppleSdkFiles.PLAYER_LIBRARY_NAME}",
            }
            entries.append(
                SdkSlice(
                    system=target.system,
                    environment=target.environment,
                    architectures=architectures,
                    abi=target.abi,
                    toolchain=AppleTarget.TOOLCHAIN,
                    runtime=AppleTarget.RUNTIME,
                    minimum_system_version=target.minimum_version,
                    graphics_apis=AppleTarget.GRAPHICS_APIS,
                    capabilities=target.capabilities,
                    files=MappingProxyType(files),
                )
            )
        manifest = SdkManifest(
            folder=folder.resolve(),
            port=AppleEnginePort.NAME,
            port_version=AppleEnginePort.VERSION,
            engine_version=Repository.engine_version(),
            revision=revision,
            configuration=configuration,
            slices=tuple(entries),
            host_tools=(),
            files=MappingProxyType(shared),
        )
        manifest.save()
        return manifest

    @staticmethod
    def architectures(target: AppleTarget, requested: tuple[str, ...] | None) -> tuple[str, ...]:
        """The architectures of a slice that a request builds: the ones it names that the slice holds, or every architecture of the slice."""
        if not requested:
            return target.architectures
        chosen = tuple(architecture for architecture in target.architectures if architecture in requested)
        if not chosen:
            names = ", ".join(f'"{name}"' for name in target.architectures)
            raise UnsupportedTarget(f'The slice "{target.slice}" builds {names}, and the request names none of them.')
        return chosen

    def build_architecture(self, target: AppleTarget, architecture: str, configuration: str) -> Path:
        """Builds one architecture of a slice in its engine tree and installs its framework component into the folder `framework` of the tree, which it returns."""
        tree = EngineSdkTree(self.context, f"apple-{target.slice}-{architecture}-{configuration.lower()}")
        if not tree.is_current():
            options = [*CMakeTree.generator(), "-DHAYLEN_BUILD_FRAMEWORK=ON", "-DHAYLEN_BUILD_PLAYER=OFF", "-DHAYLEN_BUILD_TOOLS=OFF", *AppleSdk.cmake_options(target.slice, architecture)]
            self.context.terminal.step(f'Configuring the engine of the slice "{target.slice}" on "{architecture}" in `{Repository.shown(tree.folder)}`')
            tree.tree.configure(tree.command(configuration, *options))
        self.context.terminal.step(f'Building the engine of the slice "{target.slice}" on "{architecture}"')
        tree.tree.build([AppleEnginePort.BUILD], configuration)
        installed = tree.folder / "framework"
        Folders.remove(installed)
        tree.tree.install(AppleEnginePort.FRAMEWORK_COMPONENT, installed, configuration)
        return installed

    def install_headers(self, tree: Path, configuration: str) -> Path:
        """Installs the SDK component of an engine tree beside it, whose headers, the same in every slice, go into the frameworks and the SDK folder."""
        installed = tree / "sdk"
        Folders.remove(installed)
        CMakeTree(self.context, tree).install(AppleEnginePort.SDK_COMPONENT, installed, configuration)
        return installed

    def join(self, libraries: list[Path], output: Path) -> Path:
        """Joins the libraries of the architectures of a slice with `lipo`, or returns the one library of a slice of one architecture."""
        if len(libraries) == 1:
            return libraries[0]
        output.parent.mkdir(parents=True, exist_ok=True)
        self.context.process.run(["lipo", "-create", *libraries, "-output", output])
        return output

    def create_framework(self, framework: Path, libraries: list[tuple[Path, Path | None]]) -> None:
        Folders.remove(framework)
        command: list[object] = ["xcodebuild", "-create-xcframework"]
        for library, headers in libraries:
            command += ["-library", library, *(["-headers", headers] if headers is not None else [])]
        self.context.process.run([*command, "-output", framework])
        self.context.process.run(["codesign", "--sign", "-", "--timestamp=none", "--force", framework])
        self.context.process.run(["codesign", "--verify", "--strict", framework])

    @staticmethod
    def identifiers(framework: Path) -> dict[tuple[str, str | None], str]:
        """The identifier of every library of an xcframework by its platform and variant, such as `ios-arm64_x86_64-simulator`."""
        info = plistlib.loads((framework / "Info.plist").read_bytes())
        return {(entry["SupportedPlatform"], entry.get("SupportedPlatformVariant")): entry["LibraryIdentifier"] for entry in info["AvailableLibraries"]}

    @staticmethod
    def install_shared(folder: Path, installed: Path, component: Path) -> dict[str, str]:
        """Installs the files that every slice shares and returns them by their names: the headers from the SDK component of an engine tree, the CMake package of native libraries and the settings of Apple projects from its framework component, and the privacy manifest and the Swift helpers of the engine."""
        Folders.remove(folder / "include")
        shutil.copytree(installed / "include", folder / "include")
        Folders.remove(folder / "lib" / "cmake")
        shutil.copytree(component / "lib" / "cmake", folder / "lib" / "cmake")
        shutil.copy2(component / "share" / "haylen" / AppleSdkFiles.SETTINGS_NAME, folder / AppleSdkFiles.SETTINGS_NAME)
        shutil.copy2(Repository.ENGINE / "platform" / "apple" / AppleSdkFiles.PRIVACY_NAME, folder / AppleSdkFiles.PRIVACY_NAME)
        shutil.copy2(Repository.ENGINE / "platform" / "apple" / AppleSdkFiles.SWIFT_HELPERS_NAME, folder / AppleSdkFiles.SWIFT_HELPERS_NAME)
        return {
            AppleSdkFiles.FRAMEWORK: AppleSdkFiles.FRAMEWORK_NAME,
            AppleSdkFiles.PLAYER: AppleSdkFiles.PLAYER_NAME,
            AppleSdkFiles.SETTINGS: AppleSdkFiles.SETTINGS_NAME,
            AppleSdkFiles.PRIVACY: AppleSdkFiles.PRIVACY_NAME,
            AppleSdkFiles.SWIFT_HELPERS: AppleSdkFiles.SWIFT_HELPERS_NAME,
            AppleSdkFiles.INCLUDE: "include",
            AppleSdkFiles.CMAKE: "lib/cmake/haylen",
        }
