from pathlib import Path
from types import MappingProxyType

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.target_environment import TargetEnvironment
from haylen.native.cmake_tree import CMakeTree
from haylen.native.engine_sdk_tree import EngineSdkTree
from haylen.platforms.contract import Contract
from haylen.platforms.engine_port import EnginePort
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.port_description import PortDescription
from haylen.platforms.port_target import PortTarget
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.host_tool import HostTool
from haylen.sdk.sdk_folder import SdkFolder
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_slice import SdkSlice
from haylen.toolchains.build_tools import BuildTools
from testport.port_record import PortRecord


class PortEngine(EnginePort):
    """The engine port of the platform `test-port`, which a developer installs into the environment of the tooling from outside it: it builds the engine with the CMake port of the folder of its project, whose runtime plays apps without a window on the machine that builds them through a device of the API `port`, and installs the SDK of the architecture of this machine with the capabilities, the app function and the shader tool that the CMake port declares. It reads the CMake port from the folder of its project, so it installs in editable mode from its checkout."""

    NAME = "test-port"
    VERSION = "1.0.0"
    TARGET = "test-port-host"
    PLATFORM = "test-port"
    TOOLCHAIN = "test-port-cc"
    RUNTIME = "host-c++"
    MINIMUM_SYSTEM_VERSION = "1.0"
    GRAPHICS_APIS = ("port",)
    SHADER_LANGUAGES = ("spirv",)
    CAPABILITIES = frozenset({"dynamicLibraries"})
    # The runtime of the test port compiles on the desktops, whose machines play its apps.
    HOSTS = frozenset({"macos", "linux", "windows"})
    TOOLS = ("cmake", "ninja", "haylen-shaderc")
    FOLDER = Path(__file__).resolve().parents[1]

    def describe(self) -> PortDescription:
        architecture = Host.architecture()
        target = PortTarget(
            name=PortEngine.TARGET,
            system=PortEngine.PLATFORM,
            environment=TargetEnvironment.HOST,
            abis=MappingProxyType({architecture: f"{PortEngine.PLATFORM}-{architecture}"}),
            toolchain=PortEngine.TOOLCHAIN,
            runtime=PortEngine.RUNTIME,
            minimum_system_version=PortEngine.MINIMUM_SYSTEM_VERSION,
            graphics_apis=PortEngine.GRAPHICS_APIS,
            shader_languages=PortEngine.SHADER_LANGUAGES,
            capabilities=PortEngine.CAPABILITIES,
        )
        return PortDescription(PortEngine.NAME, Contract.ENGINE_PORT, PortEngine.VERSION, (target,), PortEngine.TOOLS, ())

    def check_environment(self, target: str) -> EnvironmentReport:
        problems = []
        if Host.system() not in PortEngine.HOSTS:
            problems.append(f'The engine port "{PortEngine.NAME}" builds on macOS, Windows and Linux, and this machine runs "{Host.system()}".')
        if not (PortEngine.FOLDER / "port.cmake").is_file():
            problems.append(f'The engine port "{PortEngine.NAME}" finds no "port.cmake" in `{Repository.shown(PortEngine.FOLDER)}`. Install the port in editable mode from its checkout.')
        tools = [BuildTools.CMAKE.status()]
        if Host.system() != "windows":
            tools.append(BuildTools.NINJA.status())
        return EnvironmentReport(tuple(tools), tuple(problems))

    def build_sdk(self, request: SdkRequest) -> SdkManifest:
        foreign = [name for name in request.targets if name != PortEngine.TARGET]
        if foreign:
            raise UnsupportedTarget(f'The engine port "{PortEngine.NAME}" builds only "{PortEngine.TARGET}", not {", ".join(f'"{name}"' for name in foreign)}.')
        architecture = Host.architecture()
        if request.architectures is not None and tuple(request.architectures) != (architecture,):
            raise UnsupportedTarget(f'The engine port "{PortEngine.NAME}" builds only for the architecture of this machine, "{architecture}".')

        configuration = request.configuration.value
        tree = EngineSdkTree(self.context, f"{PortEngine.NAME}-{configuration.lower()}")
        self.configure(tree, configuration)
        tree.tree.build(["haylen-sdk"], configuration)

        folder = SdkFolder(request.folder, PortEngine.NAME, PortEngine.VERSION, request.revision, request.configuration)
        folder.begin()
        installed = folder.slice_folder(f"{PortEngine.PLATFORM}-{architecture}")
        tree.tree.install("haylen-sdk", installed, configuration)
        record = PortEngine.checked_record(installed)

        tools = []
        for name in record.host_tools:
            path = installed / "bin" / name
            if not path.is_file():
                raise BuildFailed(f'The SDK installed into `{Repository.shown(installed)}` lacks the host tool "{name}" that its port declares. Build the SDK again.')
            tools.append(HostTool(name, Host.system(), architecture, path.relative_to(request.folder).as_posix()))
        package = EngineSdkTree.package_folder(installed)
        added = SdkSlice(
            system=PortEngine.PLATFORM,
            environment=TargetEnvironment.HOST,
            architectures=(architecture,),
            abi=f"{PortEngine.PLATFORM}-{architecture}",
            toolchain=PortEngine.TOOLCHAIN,
            runtime=PortEngine.RUNTIME,
            minimum_system_version=PortEngine.MINIMUM_SYSTEM_VERSION,
            graphics_apis=tree.graphics_apis(),
            capabilities=record.capabilities,
            libraries=MappingProxyType(EngineSdkTree.libraries(installed, request.folder)),
            files=MappingProxyType({"cmake": package.relative_to(request.folder).as_posix()}),
        )
        return folder.save(added, tuple(tools))

    def configure(self, tree: EngineSdkTree, configuration: str) -> None:
        """Configures the engine tree when it was never configured, its configure failed, the shader compiler of another pin configured it or it was configured with the CMake port of another folder."""
        if tree.is_current() and tree.tree.cache_value("HAYLEN_PORT_DIR") == PortEngine.FOLDER.as_posix():
            return
        options = [*CMakeTree.generator(), f"-DHAYLEN_PORT_DIR={PortEngine.FOLDER.as_posix()}", "-DHAYLEN_BUILD_TOOLS=OFF"]
        self.context.terminal.step(f"Configuring the engine of the test port in `{Repository.shown(tree.folder)}`")
        tree.tree.configure(tree.command(configuration, *options))

    @staticmethod
    def checked_record(installed: Path) -> PortRecord:
        """Reads what the CMake port declared into the SDK and refuses an SDK whose port is not the one that this engine port describes."""
        record = PortRecord.load(installed)
        if record.platform != PortEngine.PLATFORM or record.capabilities != PortEngine.CAPABILITIES or not record.app_function:
            raise BuildFailed(
                f'The CMake port of `{Repository.shown(PortEngine.FOLDER)}` declares the platform "{record.platform}" with the capabilities {sorted(record.capabilities)} and the app function "{record.app_function}", while its engine port declares "{PortEngine.PLATFORM}" with {sorted(PortEngine.CAPABILITIES)} and needs an app function. Make "port.cmake" and the engine port agree.'
            )
        return record
