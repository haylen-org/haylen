from pathlib import Path
from types import MappingProxyType

from haylen.core.app_kind import AppKind
from haylen.core.configuration import Configuration
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.package_layout import PackageLayout
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_environment import TargetEnvironment
from haylen.core.target_id import TargetId
from haylen.development.app_run import AppRun
from haylen.development.development_reach import DevelopmentReach
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.capability import Capability
from haylen.platforms.contract import Contract
from haylen.platforms.desktop.desktop_system import DesktopSystem
from haylen.platforms.desktop.host_native_libraries import HostNativeLibraries
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.platform_backend import PlatformBackend
from haylen.platforms.platform_description import PlatformDescription
from haylen.platforms.preparation import Preparation
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_component import ProductComponent
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.target_description import TargetDescription
from haylen.platforms.verification_check import VerificationCheck
from haylen.platforms.verification_report import VerificationReport


class PlayerBackend(PlatformBackend):
    """The backend of the target `player`, which runs a Lua app in the desktop player of the SDK of this machine in development, with hot reload of the app and its plugins, and compiles nothing of the engine or of the app. Its preparation stages the package with every plugin at `plugins/<id>/` and places the native libraries of the app and its plugins for this machine in the folder that the player searches first. It is the default target of Lua apps on macOS, Windows and Linux, and it ships no release, which the targets of each platform build."""

    NAME = "player"
    VERSION = Repository.engine_version()
    TARGET = "player"
    SOURCE = "player"
    # The Metal device of macOS records a frame capture only with this variable set when the process starts.
    METAL_CAPTURE = {"MTL_CAPTURE_ENABLED": "1"}
    DEVICES = {"macos": "This Mac", "windows": "This PC", "linux": "This computer"}

    def describe(self) -> PlatformDescription:
        system = DesktopSystem.host()
        target = TargetDescription(
            name=PlayerBackend.TARGET,
            system=system.name,
            platform=system.name,
            environment=TargetEnvironment.HOST,
            abis=system.abis,
            default_architectures=system.default_architectures,
            toolchain=system.toolchain,
            kinds=frozenset({AppKind.LUA}),
            hosts=DesktopSystem.names(),
            host_default=frozenset({AppKind.LUA}),
            requires=frozenset({"dynamicLibraries"}),
            release_profile="",
            description="The desktop player of this machine, which runs a Lua app in development with hot reload.",
        )
        capabilities = frozenset({Capability.PREPARE, Capability.BUILD, Capability.VERIFY, Capability.RUN})
        return PlatformDescription(PlayerBackend.NAME, Contract.BACKEND, PlayerBackend.VERSION, (target,), capabilities, ("cmake",), MappingProxyType({}), MappingProxyType({}))

    def check_environment(self, target: TargetId) -> EnvironmentReport:
        if Host.system() in DesktopSystem.names():
            return EnvironmentReport((), ())
        return EnvironmentReport((), (f'The player runs on macOS, Windows and Linux, and this machine runs "{Host.system()}".',))

    @staticmethod
    def native_folder(folders: ProductFolders) -> Path:
        return folders.build / "native"

    def prepare(self, product: ResolvedProduct, folders: ProductFolders) -> PreparedProject:
        if product.configuration is Configuration.RELEASE:
            raise UnsupportedTarget(
                f'The player runs apps in development alone, so it has no "Release" configuration. Build the release of `{Repository.shown(product.app.folder)}` for the target of a platform, such as "windows" or "ios".'
            )
        preparation = Preparation(product, folders, PlayerBackend.NAME, PlayerBackend.VERSION, None)
        if preparation.is_current() and folders.package.is_dir() and PlayerBackend.native_folder(folders).is_dir():
            return preparation.unchanged(folders.project)

        product.package.stage(folders.package)
        project = product.primary
        HostNativeLibraries(self.context).place(project, project.sdk.file("cmake"), folders.build / "libraries", PlayerBackend.native_folder(folders))
        return preparation.finish(GeneratedTree(folders.project), folders.project)

    def build(self, prepared: PreparedProject) -> BuildProduct:
        """Checks that the SDK holds the player, which every app of this machine shares, and builds nothing."""
        project = prepared.product.primary
        player = project.sdk.file("player")
        if not player.is_file():
            raise IncompatibleSdk(
                f'The SDK `{Repository.shown(project.sdk.manifest.folder)}` lists the player `{Repository.shown(player)}`, which does not exist. Build it again with "haylen sdk build --target {PlayerBackend.TARGET}".'
            )
        package = prepared.folders.package
        components = tuple(ProductComponent(entry.target, package, "package") for entry in prepared.product.projects)
        return BuildProduct(prepared, package, "package", components, (player, PlayerBackend.native_folder(prepared.folders)))

    def verify(self, product: BuildProduct) -> VerificationReport:
        """Checks the staged package: its manifest, its entry point and every plugin at `plugins/<id>/` with its manifest and Lua modules."""
        package = product.path
        resolved = product.prepared.product
        checks = []
        for required in (PackageLayout.MANIFEST, "source/main.lua"):
            if not (package / required).is_file():
                checks.append(VerificationCheck.failed("package", f'The staged package `{Repository.shown(package)}` lacks "{required}". Prepare the app again.', PlayerBackend.SOURCE))
        for plugin in resolved.plugins:
            if not (package / "plugins" / plugin.id / PackageLayout.PLUGIN_MANIFEST).is_file():
                checks.append(VerificationCheck.failed("plugins", f'The staged package lacks the plugin "{plugin.id}" at "plugins/{plugin.id}". Prepare the app again.', PlayerBackend.SOURCE))
        if not checks:
            checks.append(VerificationCheck.passed("package", f"The staged package holds the app and its {len(resolved.plugins)} plugins.", PlayerBackend.SOURCE))
        return VerificationReport(tuple(checks))

    def run(self, product: BuildProduct, launch: LaunchRequest) -> None:
        """Runs the staged package in the player in development, which connects to the development server of the run through the loopback address of this machine."""
        player = product.prepared.product.primary.sdk.file("player")
        package = product.prepared.folders.package
        native = PlayerBackend.native_folder(product.prepared.folders)
        options = ["--native", native] if any(native.iterdir()) else []
        with AppRun(self.context, product, launch, DevelopmentReach.LOOPBACK, package) as run:
            environment = PlayerBackend.METAL_CAPTURE if run.developing and Host.system() == "macos" else None
            run.show(f"{PlayerBackend.DEVICES[Host.system()]}, in the player")
            run.follow([player, *run.arguments, *options, package], environment=environment)
