import shutil
from types import MappingProxyType

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.target_environment import TargetEnvironment
from haylen.native.cmake_tree import CMakeTree
from haylen.native.engine_sdk_tree import EngineSdkTree
from haylen.platforms.contract import Contract
from haylen.platforms.desktop.desktop_system import DesktopSystem
from haylen.platforms.engine_port import EnginePort
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.port_description import PortDescription
from haylen.platforms.port_target import PortTarget
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.host_tool import HostTool
from haylen.sdk.sdk_folder import SdkFolder
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_slice import SdkSlice
from haylen.toolchains.apple_sdk import AppleSdk
from haylen.toolchains.build_tools import BuildTools


class DesktopPort(EnginePort):
    """The engine port of the desktops, which compiles the engine of this machine in one build tree into the SDK of its system and architecture: the CMake package that `find_package(haylen)` imports, with the Lua player object that defines the app of Lua apps, the desktop player that runs any package in development and the content tool `haylen-content`, both host tools of this machine. It builds only for the machine it runs on, so the SDK of Windows builds on Windows and the SDK of Linux on Linux, and its slice serves the targets `windows`, `linux` and `player`, whatever their names, by their system, environment, binary interface and toolchain."""

    NAME = "desktop"
    VERSION = Repository.engine_version()
    PLAYER = "haylen"
    CONTENT_TOOL = "haylen-content"
    TOOLS = ("cmake", "ninja", "haylen-shaderc")

    def describe(self) -> PortDescription:
        targets = tuple(DesktopPort.port_target(system) for system in DesktopSystem.all())
        return PortDescription(DesktopPort.NAME, Contract.ENGINE_PORT, DesktopPort.VERSION, targets, DesktopPort.TOOLS, (DesktopPort.PLAYER, DesktopPort.CONTENT_TOOL))

    @staticmethod
    def port_target(system: DesktopSystem) -> PortTarget:
        return PortTarget(
            name=f"desktop-{system.name}",
            system=system.name,
            environment=TargetEnvironment.HOST,
            abis=system.abis,
            toolchain=system.toolchain,
            runtime=system.runtime,
            minimum_system_version=system.minimum_system_version,
            graphics_apis=system.graphics_apis,
            shader_languages=system.shader_languages,
            capabilities=DesktopSystem.CAPABILITIES,
        )

    def check_environment(self, target: str) -> EnvironmentReport:
        port_target = self.describe().target(target)
        problems = []
        if port_target is not None and port_target.system != Host.system():
            problems.append(f'The engine port "{DesktopPort.NAME}" builds the SDK of "{port_target.system}" only on a "{port_target.system}" machine, and this one runs "{Host.system()}".')
        tools = [BuildTools.CMAKE.status()]
        # The build of Windows runs through Visual Studio, and the OpenSSL of its network configures itself with Perl and assembles with NASM, which other systems do not need.
        if Host.system() == "windows":
            tools.extend((BuildTools.PERL.status(), BuildTools.NASM.status()))
        else:
            tools.append(BuildTools.NINJA.status())
        return EnvironmentReport(tuple(tools), tuple(problems))

    def build_sdk(self, request: SdkRequest) -> SdkManifest:
        system = DesktopSystem.host()
        description = self.describe()
        foreign = [name for name in request.targets if description.target(name) is None or description.target(name).system != Host.system()]
        if foreign:
            raise UnsupportedTarget(f'The engine port "{DesktopPort.NAME}" builds only the SDK of this machine, "{Host.system()}", so it cannot build {", ".join(f'"{name}"' for name in foreign)}.')
        architectures = request.architectures or system.default_architectures
        if tuple(architectures) != (Host.architecture(),):
            raise UnsupportedTarget(
                f'The engine port "{DesktopPort.NAME}" builds only for the architecture of this machine, "{Host.architecture()}". Build the SDK of another architecture on a machine of it.'
            )

        configuration = request.configuration.value
        tree = EngineSdkTree(self.context, f"desktop-{configuration.lower()}")
        self.configure(tree, configuration)
        tree.tree.build([DesktopPort.PLAYER, DesktopPort.CONTENT_TOOL, "haylen-sdk"], configuration)

        folder = SdkFolder(request.folder, DesktopPort.NAME, DesktopPort.VERSION, request.revision, request.configuration)
        folder.begin()
        name = f"{system.name}-{Host.architecture()}"
        installed = folder.slice_folder(name)
        tree.tree.install("haylen-sdk", installed, configuration)
        player = installed / "bin" / Host.executable(DesktopPort.PLAYER)
        content_tool = installed / "bin" / Host.executable(DesktopPort.CONTENT_TOOL)
        player.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(tree.folder / "bin" / "haylen" / Host.executable(DesktopPort.PLAYER), player)
        shutil.copy2(tree.folder / "bin" / Host.executable(DesktopPort.CONTENT_TOOL), content_tool)

        files = {
            "cmake": EngineSdkTree.package_folder(installed).relative_to(request.folder).as_posix(),
            "player": player.relative_to(request.folder).as_posix(),
        }
        added = SdkSlice(
            system=system.name,
            environment=TargetEnvironment.HOST,
            architectures=(Host.architecture(),),
            abi=system.abis[Host.architecture()],
            toolchain=system.toolchain,
            runtime=system.runtime,
            minimum_system_version=system.minimum_system_version,
            graphics_apis=tree.graphics_apis(),
            capabilities=DesktopSystem.CAPABILITIES,
            files=MappingProxyType(files),
        )
        tools = (
            HostTool(DesktopPort.CONTENT_TOOL, system.name, Host.architecture(), content_tool.relative_to(request.folder).as_posix()),
            HostTool(DesktopPort.PLAYER, system.name, Host.architecture(), player.relative_to(request.folder).as_posix()),
        )
        return folder.save(added, tools)

    def configure(self, tree: EngineSdkTree, configuration: str) -> None:
        """Configures the engine tree when it was never configured, its configure failed or the shader compiler of another pin configured it."""
        if tree.is_current():
            return
        options: list[object] = [*CMakeTree.generator(), "-DHAYLEN_BUILD_TOOLS=ON"]
        if Host.system() == "macos":
            options.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={AppleSdk.MINIMUM_VERSIONS['macOS']}")
        self.context.terminal.step(f"Configuring the engine of this machine in `{Repository.shown(tree.folder)}`")
        tree.tree.configure(tree.command(configuration, *options))
