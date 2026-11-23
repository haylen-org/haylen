from pathlib import Path

from haylen.core.app_kind import AppKind
from haylen.core.configuration import Configuration
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.schema_validator import SchemaValidator
from haylen.core.target_id import TargetId
from haylen.core.tooling_context import ToolingContext
from haylen.native.cmake_tree import CMakeTree
from haylen.platforms.backend_registry import BackendRegistry
from haylen.sdk.sdk_catalog import SdkCatalog
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_selector import SdkSelector
from haylen.toolchains.apple_sdk import AppleSdk
from haylen.toolchains.shader_compiler import ShaderCompiler


class Consumers:
    """Builds the consumer project of the engine tests, which adds the engine the way another repository does and compiles a C++ app without Lua against its public headers: through `add_subdirectory` and CPM from the sources of the engine, and through `find_package` from an installed SDK of this machine. The build of the project fails when a program of the app holds a symbol of Lua, and a project that adds the engine from its sources also runs the app on the headless host with the application of its own sources, until it quits once it drew its first frame."""

    PROJECT = Repository.ENGINE / "tests" / "consumer"
    MODES = {"subdirectory": '"add_subdirectory"', "cpm": "CPM", "package": 'an installed SDK and "find_package"'}
    HEADLESS = "haylen-consumer-headless"
    # The app quits in its first frame, so the bound only stops a run that never quits.
    HEADLESS_FRAMES = 600

    def __init__(self, context: ToolingContext, registry: BackendRegistry) -> None:
        self.context = context
        self.registry = registry

    def sdk(self, named: str | None, configuration: Configuration) -> Path:
        """The folder of the CMake package that the package mode finds: the one of the slice of this machine in the SDK folder that `--sdk` names, or else in the installed desktop SDK of the current engine sources and of the configuration of the project, the SDK of the default target of Lua apps here, whose players run on this machine, and never an SDK of a device platform that also holds a slice of this system."""
        target = self.registry.default_target(AppKind.LUA).description
        architecture = Host.architecture()
        if architecture not in target.abis:
            raise UnsupportedTarget(f'The target "{target.name}" of this machine builds for {SchemaValidator.choices(target.abis)}, not for "{architecture}".')
        target_id = TargetId(target.system, architecture, target.abis[architecture], target.environment, configuration, target.toolchain)
        manifests = (SdkManifest.load(Path(named).expanduser().resolve()),) if named else tuple(SdkCatalog(self.context.environment).manifests())
        selected = SdkSelector(manifests, Repository.engine_revision()).select(target_id, target.name, configuration, None, ())
        return selected.file("cmake")

    def run(self, modes: list[str], configuration: str, named_sdk: str | None) -> dict[str, object]:
        sdk = self.sdk(named_sdk, Configuration(configuration)) if "package" in modes else None
        built = []
        for mode in modes:
            tree = CMakeTree(self.context, Repository.ENGINE_BUILDS / "consumers" / f"{mode}-{configuration.lower()}")
            command: list[object] = ["cmake", "-S", Consumers.PROJECT, "-B", tree.folder, *CMakeTree.generator(), f"-DCMAKE_BUILD_TYPE={configuration}", f"-DHAYLEN_CONSUMER_MODE={mode}"]
            # A project that builds the engine from its sources shares the dependency cache of the trees of the engine, and the installed SDK needs neither.
            if mode == "package":
                command.append(f"-Dhaylen_DIR={sdk.as_posix()}")
            else:
                command += [f"-DHAYLEN_SHADERC={ShaderCompiler.path(self.context).as_posix()}", f"-DCPM_SOURCE_CACHE={CMakeTree.cpm_cache(self.context.environment)}"]
            if Host.system() == "macos":
                command.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={AppleSdk.MINIMUM_VERSIONS['macOS']}")

            self.context.terminal.step(f"Building a project that adds the engine through {Consumers.MODES[mode]}")
            tree.configure(command)
            self.context.process.run(["cmake", "--build", tree.folder, "--config", configuration, "--parallel", str(self.context.jobs)])
            if mode != "package":
                self.context.terminal.step("Running the C++ app of the project on the headless host")
                program = tree.folder / "bin" / Host.executable(Consumers.HEADLESS)
                self.context.process.run([program, tree.folder / "package", "--frames", str(Consumers.HEADLESS_FRAMES), "--", "--quit"])
            self.context.terminal.success(f"A project adds the engine through {Consumers.MODES[mode]}.")
            built.append({"mode": mode, "tree": str(tree.folder)})
        return {"consumers": built, "sdk": str(sdk) if sdk else None}
