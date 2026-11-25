import shutil
from pathlib import Path
from types import MappingProxyType

from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_environment import TargetEnvironment
from haylen.core.target_id import TargetId
from haylen.development.app_run import AppRun
from haylen.development.development_reach import DevelopmentReach
from haylen.native.cmake_tree import CMakeTree
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.capability import Capability
from haylen.platforms.contract import Contract
from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.merge_mode import MergeMode
from haylen.platforms.override_section import OverrideSection
from haylen.platforms.platform_backend import PlatformBackend
from haylen.platforms.platform_description import PlatformDescription
from haylen.platforms.preparation import Preparation
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_component import ProductComponent
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.target_description import TargetDescription
from haylen.platforms.verification_report import VerificationReport
from haylen.toolchains.build_tools import BuildTools
from testportbackend.linked_parts import LinkedParts
from testportbackend.port_app_project import PortAppProject
from testportbackend.port_verifier import PortVerifier


class PortBackend(PlatformBackend):
    """The backend of the target `test-port`, which a developer installs into the environment of the tooling from outside it, like the backend of a platform whose SDK is private. It builds a Lua app against the SDK of whichever engine port builds the engine of the system `test-port`, through a CMake project that calls `haylen_add_app` and links the parts of the plugins of the type `testPortPart`, copies the executable with its content into the products of the app, verifies the executable, its content and its parts, and runs it on this machine, where the test port plays it without a window. It builds the Debug configuration, which ships the package as it is."""

    NAME = "test-port"
    VERSION = "1.0.0"
    TARGET = "test-port"
    SYSTEM = "test-port"
    TOOLCHAIN = "test-port-cc"
    RELEASE_PROFILE = "test-port"
    # The runtime of the test port compiles on the desktops, whose machines play its apps.
    HOSTS = frozenset({"macos", "linux", "windows"})
    DEVICE = "This machine, on the test port without a window"
    SECTION = {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "executable": {
                "type": "string",
                "pattern": "^[a-z][a-z0-9]*(-[a-z0-9]+)*$",
                "description": "The name of the executable of the app, the last word of its identifier by default.",
            },
        },
    }
    # The generator of the projects is the code of this package, so a change of it prepares the projects again.
    TEMPLATE = Path(__file__).resolve().parent

    def describe(self) -> PlatformDescription:
        architecture = Host.architecture()
        target = TargetDescription(
            name=PortBackend.TARGET,
            system=PortBackend.SYSTEM,
            platform=PortBackend.SYSTEM,
            environment=TargetEnvironment.HOST,
            abis=MappingProxyType({architecture: f"{PortBackend.SYSTEM}-{architecture}"}),
            default_architectures=(architecture,),
            toolchain=PortBackend.TOOLCHAIN,
            kinds=frozenset({AppKind.LUA}),
            hosts=PortBackend.HOSTS,
            host_default=frozenset(),
            requires=frozenset(),
            release_profile=PortBackend.RELEASE_PROFILE,
            description="A Lua app of the test port, built with CMake against the SDK of its engine port and played on this machine without a window.",
        )
        part = ContributionType(
            LinkedParts.TYPE,
            frozenset({PortBackend.SYSTEM}),
            MappingProxyType(LinkedParts.SCHEMA),
            (),
            MergeMode.ACCUMULATE,
            "The C++ sources of a plugin that the app links, with the C functions that Lua reaches through native.load.",
        )
        section = OverrideSection(
            PortAppProject.SECTION,
            frozenset({PortBackend.SYSTEM}),
            MappingProxyType(PortBackend.SECTION),
            MappingProxyType({}),
            "The executable of the app of the test port.",
        )
        capabilities = frozenset({Capability.PREPARE, Capability.BUILD, Capability.VERIFY, Capability.RUN})
        return PlatformDescription(
            PortBackend.NAME,
            Contract.BACKEND,
            PortBackend.VERSION,
            (target,),
            capabilities,
            ("cmake", "ninja"),
            MappingProxyType({LinkedParts.TYPE: part}),
            MappingProxyType({PortAppProject.SECTION: section}),
        )

    def check_app(self, app: AppManifest, target: str) -> None:
        """Every app suits the targets of this backend, whatever `haylen.json` chooses."""

    def check_environment(self, target: TargetId) -> EnvironmentReport:
        problems = []
        if Host.system() not in PortBackend.HOSTS:
            problems.append(f'The apps of the test port build on macOS, Windows and Linux, and this machine runs "{Host.system()}".')
        tools = [BuildTools.CMAKE.status()]
        if Host.system() != "windows":
            tools.append(BuildTools.NINJA.status())
        return EnvironmentReport(tuple(tools), tuple(problems))

    @staticmethod
    def cmake_folder(folders: ProductFolders) -> Path:
        return folders.build / "cmake"

    def prepare(self, product: ResolvedProduct, folders: ProductFolders) -> PreparedProject:
        """Generates the project after the staged package of the app, after checking that the SDK holds its CMake package."""
        if product.configuration is not Configuration.DEBUG:
            raise UnsupportedTarget(
                f'The backend "{PortBackend.NAME}" builds the "Debug" configuration alone, which ships the package as it is, and `{Repository.shown(product.app.folder)}` asks for "{product.configuration.value}".'
            )
        PortAppProject.sdk_package(product)
        preparation = Preparation(self.context, product, folders, PortBackend.NAME, PortBackend.VERSION, PortBackend.TEMPLATE)
        if preparation.is_current() and folders.package.is_dir():
            return preparation.unchanged(folders.project)

        content = product.package.stage(folders.package)
        tree = GeneratedTree(folders.project)
        PortAppProject(product, content).write(tree)
        return preparation.finish(tree, folders.project)

    def build(self, prepared: PreparedProject) -> BuildProduct:
        """Builds the project in the configuration of the engine SDK, whose libraries the executable links, and copies the folder of the executable with its content into the products of the app."""
        folders = prepared.folders
        configuration = prepared.product.request.engine_configuration.value
        tree = CMakeTree(self.context, PortBackend.cmake_folder(folders))
        if not tree.configured:
            tree.configure(["cmake", "-S", prepared.project, "-B", tree.folder, *CMakeTree.generator(), f"-DCMAKE_BUILD_TYPE={configuration}"])
        self.context.process.run(["cmake", "--build", tree.folder, "--config", configuration, "--parallel", str(self.context.jobs)])

        name = PortAppProject(prepared.product, folders.package).executable
        Folders.remove(folders.products)
        shutil.copytree(tree.folder / "bin" / name, folders.products / name)
        executable = PortBackend.executable(prepared.product, folders)
        components = tuple(ProductComponent(project.target, executable, "executable") for project in prepared.product.projects)
        return BuildProduct(prepared, executable.parent, "app", components, ())

    @staticmethod
    def executable(product: ResolvedProduct, folders: ProductFolders) -> Path:
        name = PortAppProject(product, folders.package).executable
        return folders.products / name / Host.executable(name)

    def verify(self, product: BuildProduct) -> VerificationReport:
        executable = PortBackend.executable(product.prepared.product, product.prepared.folders)
        return VerificationReport(PortVerifier.checks(product, executable))

    def run(self, product: BuildProduct, launch: LaunchRequest) -> None:
        """Plays the built app on this machine in development with its log in the terminal, and fails when the app quits with a status other than 0."""
        executable = PortBackend.executable(product.prepared.product, product.prepared.folders)
        with AppRun(self.context, product, launch, DevelopmentReach.LOOPBACK, executable.parent / PortVerifier.CONTENT) as run:
            run.show(PortBackend.DEVICE)
            run.follow([executable, *run.arguments], cwd=executable.parent)
