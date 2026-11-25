import re
import shutil
from pathlib import Path
from types import MappingProxyType

from haylen.content.protected_release import ProtectedRelease
from haylen.content.protected_releases import ProtectedReleases
from haylen.content.release_entry import ReleaseEntry
from haylen.content.release_package import ReleasePackage
from haylen.content.steam_depots import SteamDepots
from haylen.core.app_category import AppCategory
from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_environment import TargetEnvironment
from haylen.core.target_id import TargetId
from haylen.development.app_run import AppRun
from haylen.development.development_reach import DevelopmentReach
from haylen.native.cmake_tree import CMakeTree
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.capability import Capability
from haylen.platforms.contract import Contract
from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.desktop.appimage_tools import AppImageTools
from haylen.platforms.desktop.desktop_app import DesktopApp
from haylen.platforms.desktop.desktop_layout import DesktopLayout
from haylen.platforms.desktop.desktop_links import DesktopLinks
from haylen.platforms.desktop.desktop_project import DesktopProject
from haylen.platforms.desktop.desktop_system import DesktopSystem
from haylen.platforms.desktop.desktop_verifier import DesktopVerifier
from haylen.platforms.desktop.host_native_libraries import HostNativeLibraries
from haylen.platforms.desktop.inno_setup import InnoSetup
from haylen.platforms.desktop.linux_entries import LinuxEntries
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.merge_mode import MergeMode
from haylen.platforms.override_section import OverrideSection
from haylen.platforms.package_request import PackageRequest
from haylen.platforms.platform_backend import PlatformBackend
from haylen.platforms.platform_description import PlatformDescription
from haylen.platforms.preparation import Preparation
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_component import ProductComponent
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.target_description import TargetDescription
from haylen.platforms.verification_report import VerificationReport
from haylen.plugins.core_contributions import CoreContributions
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.path_tool import PathTool


class DesktopBackend(PlatformBackend):
    """The backend of the targets `windows` and `linux`, which generates the CMake project of a Lua or C++ app from the template of its platform, builds it against the installed desktop SDK of this machine, installs the app into the products of its folder and runs it there. A Release build ships the protected release of the app with its bootstrap. The packages of a built app are the installer of its platform, the Inno Setup installer of Windows or the `.deb` and the AppImage of Linux, and the depots of Steam. It builds only on a machine of its target system and is the default target of C++ apps there."""

    NAME = "desktop"
    VERSION = Repository.engine_version()
    PLATFORMS = ("windows", "linux")
    INSTALLER = "installer"
    STEAM = "steam"
    PACKAGES = (".exe", ".deb", ".AppImage")
    DEVICES = {"windows": "This PC", "linux": "This computer"}
    # The `.deb` lists the libraries of the system that the app links, which CPack finds with these tools of Debian.
    # The AppImage generator of CPack, which the Linux installer packs with, came with CMake 4.2.
    APPIMAGE_CPACK = (4, 2)
    DEBIAN_TOOLS = (
        PathTool("file", 'Install it with the package "file", such as with "apt install file".'),
        PathTool("dpkg-shlibdeps", 'Install it with the package "dpkg-dev", such as with "apt install dpkg-dev".'),
    )
    URL_SCHEME = {
        "type": "object",
        "additionalProperties": False,
        "required": ["scheme"],
        "properties": {"scheme": {"type": "string", "pattern": "^[A-Za-z][A-Za-z0-9+.-]*$", "description": 'A URL scheme as RFC 3986 spells one, such as "camera-scanner".'}},
    }
    EXECUTABLE_NAME = {
        "type": "string",
        "pattern": "^[a-z0-9][a-z0-9-]*$",
        "description": "The name of the executable and of the packages of the app in lowercase dash-case, the name of the app folder in lowercase dash-case by default.",
    }
    # The settings of each desktop, besides the keys that the core reads.
    SECTIONS = {
        "windows": {
            "type": "object",
            "additionalProperties": False,
            "properties": {
                "executableName": EXECUTABLE_NAME,
                "console": {"type": "boolean", "default": False, "description": "Whether the app runs in the console subsystem with a console of its own, false by default."},
                "executionLevel": {
                    "enum": ["asInvoker", "highestAvailable", "requireAdministrator"],
                    "default": "asInvoker",
                    "description": 'The rights that the app asks Windows for when it starts, "asInvoker" by default, "highestAvailable" or "requireAdministrator", which shows the prompt of User Account Control.',
                },
            },
        },
        "linux": {"type": "object", "additionalProperties": False, "properties": {"executableName": EXECUTABLE_NAME}},
    }

    def describe(self) -> PlatformDescription:
        targets = tuple(DesktopBackend.target(DesktopSystem.of(name)) for name in DesktopBackend.PLATFORMS)
        platforms = frozenset(DesktopBackend.PLATFORMS)
        types = {
            CoreContributions.NATIVE_LIBRARY: CoreContributions.native_library(platforms),
            DesktopLinks.CONTRIBUTION: ContributionType(
                DesktopLinks.CONTRIBUTION,
                platforms,
                MappingProxyType(DesktopBackend.URL_SCHEME),
                ("scheme",),
                MergeMode.UNIQUE,
                "A URL scheme whose links open the app, which the installer of Windows and the desktop entry of Linux register.",
            ),
        }
        sections = {
            name: OverrideSection(
                name, frozenset({name}), MappingProxyType(DesktopBackend.SECTIONS[name]), MappingProxyType({}), f"The settings and the files of the {name.capitalize()} project of the app."
            )
            for name in DesktopBackend.PLATFORMS
        }
        capabilities = frozenset({Capability.PREPARE, Capability.BUILD, Capability.VERIFY, Capability.RUN, Capability.PACKAGE})
        return PlatformDescription(DesktopBackend.NAME, Contract.BACKEND, DesktopBackend.VERSION, targets, capabilities, ("cmake", "ninja"), MappingProxyType(types), MappingProxyType(sections))

    @staticmethod
    def target(system: DesktopSystem) -> TargetDescription:
        return TargetDescription(
            name=system.name,
            system=system.name,
            platform=system.name,
            environment=TargetEnvironment.HOST,
            abis=system.abis,
            default_architectures=system.default_architectures,
            toolchain=system.toolchain,
            kinds=frozenset({AppKind.LUA, AppKind.CPP}),
            hosts=frozenset({system.name}),
            host_default=frozenset({AppKind.CPP}),
            requires=frozenset({"dynamicLibraries"}),
            release_profile=system.name,
            description=f"A {system.name.capitalize()} app built with CMake against the desktop SDK, with its installer.",
        )

    def check_app(self, app: AppManifest, target: str) -> None:
        """Refuses an oldest system version of Linux, where an app runs on every distribution whose C library is as new as the one of the SDK that builds it."""
        if "minimumSystemVersion" in app.platforms.get("linux", {}):
            raise InvalidManifest(
                f'A Linux app runs on every distribution whose C library is as new as the one of the SDK that builds it, so "platforms.linux.minimumSystemVersion" of `{app.origin}` has nothing to set. Remove it, and build the SDK on the oldest distribution that the app supports.',
                origin=app.origin,
            )

    def check_environment(self, target: TargetId) -> EnvironmentReport:
        problems = []
        if target.system != Host.system():
            problems.append(f'The apps of "{target.system}" build on a "{target.system}" machine alone, and this one runs "{Host.system()}".')
        tools = [BuildTools.CMAKE.status()]
        if Host.system() != "windows":
            tools.append(BuildTools.NINJA.status())
        return EnvironmentReport(tuple(tools), tuple(problems))

    @staticmethod
    def native_folder(folders: ProductFolders) -> Path:
        return folders.build / "native"

    @staticmethod
    def cmake_folder(folders: ProductFolders) -> Path:
        return folders.build / "cmake"

    def profile(self, product: ResolvedProduct) -> str:
        return self.describe().target(product.target_name).release_profile

    def releases(self) -> ProtectedReleases:
        return ProtectedReleases.locate(self.context, BackendRegistry.discover(self.context))

    def prepare(self, product: ResolvedProduct, folders: ProductFolders) -> PreparedProject:
        """Generates the project, after the staged package of a Debug build, or the protected release with the bootstrap of a Release build, which the content tool builds every time, since the keys of the app are no input of the preparation, and the native libraries of this machine."""
        platform = product.platform
        preparation = Preparation(self.context, product, folders, DesktopBackend.NAME, DesktopBackend.VERSION, DesktopProject.template(platform))
        native = DesktopBackend.native_folder(folders)
        release = product.configuration is Configuration.RELEASE
        if not release and preparation.is_current() and folders.package.is_dir() and native.is_dir():
            return preparation.unchanged(folders.project)

        bootstrap = None
        if release:
            releases = self.releases()
            built = releases.build(ReleasePackage.of(product), self.profile(product), product.request.previous_release)
            content = built.shipped
            bootstrap = releases.bootstrap_source(product.app, self.profile(product))
        else:
            content = product.package.stage(folders.package)

        project = product.primary
        HostNativeLibraries(self.context).place(project, project.sdk.file("cmake"), folders.build / "libraries", native)
        tree = GeneratedTree(folders.project)
        for warning in DesktopProject(product, content, native, bootstrap).write(tree):
            self.context.terminal.warning(warning)
        return preparation.finish(tree, folders.project)

    def build(self, prepared: PreparedProject) -> BuildProduct:
        """Builds the project with CMake in the build folder of the product, outside the project, and installs the app into the products of the product."""
        folders = prepared.folders
        configuration = prepared.product.configuration.value
        tree = CMakeTree(self.context, DesktopBackend.cmake_folder(folders))
        if not tree.configured:
            tree.configure(["cmake", "-S", prepared.project, "-B", tree.folder, *CMakeTree.generator(), f"-DCMAKE_BUILD_TYPE={configuration}", CMakeTree.WITHOUT_MODULE_SCAN])
        self.context.process.run(["cmake", "--build", tree.folder, "--config", configuration, "--parallel", str(self.context.jobs)])
        Folders.remove(folders.products)
        self.context.process.run(["cmake", "--install", tree.folder, "--config", configuration, "--prefix", folders.products])

        app = DesktopApp.of_product(prepared.product)
        layout = DesktopLayout.of(prepared.product.platform, app, folders.products)
        components = tuple(ProductComponent(project.target, layout.executable, "executable") for project in prepared.product.projects)
        symbols = tree.folder / "symbols"
        auxiliary = (symbols,) if symbols.is_dir() else ()
        return BuildProduct(prepared, folders.products, "app", components, auxiliary)

    def verify(self, product: BuildProduct) -> VerificationReport:
        resolved = product.prepared.product
        app = DesktopApp.of_product(resolved)
        layout = DesktopLayout.of(resolved.platform, app, product.path)
        checks = DesktopVerifier.checks(resolved.platform, app, layout, resolved.configuration, DesktopBackend.native_folder(product.prepared.folders))
        if resolved.configuration is Configuration.RELEASE:
            source = ReleasePackage.of(resolved)
            checks += self.releases().inspection(source).checks(ReleaseEntry.of_folder(product.path), layout.package)
        return VerificationReport(tuple(checks))

    def run(self, product: BuildProduct, launch: LaunchRequest) -> None:
        """Registers the links of the URL schemes of the app for the installed app and runs it, in development with the Debug configuration."""
        resolved = product.prepared.product
        app = DesktopApp.of_product(resolved)
        layout = DesktopLayout.of(resolved.platform, app, product.path)
        links = DesktopLinks(self.context)
        schemes = DesktopLinks.schemes(resolved.primary)
        if resolved.platform == "windows":
            links.register_windows(app, schemes, layout.executable)
        else:
            links.register_linux(app, schemes, layout.folder)
        with AppRun(self.context, product, launch, DevelopmentReach.LOOPBACK, layout.folder / layout.package) as run:
            run.show(DesktopBackend.DEVICES[resolved.platform])
            run.follow([layout.executable, *run.arguments], cwd=layout.executable.parent)

    def package(self, product: BuildProduct, request: PackageRequest) -> tuple[str, ...]:
        """Packages a Release build as the installer of its platform with CPack, or lays it out as the depots of Steam."""
        resolved = product.prepared.product
        if resolved.configuration is not Configuration.RELEASE:
            raise InvalidOption(
                f'The packages of an app ship its Release build, and the build of `{Repository.shown(product.path)}` is "{resolved.configuration.value}". Build the app with "--config Release" first.'
            )
        if request.format == DesktopBackend.INSTALLER:
            return self.installer(product, request)
        if request.format == DesktopBackend.STEAM:
            return self.steam(product, request)
        raise InvalidOption(f'The backend "{DesktopBackend.NAME}" packages apps as "{DesktopBackend.INSTALLER}" or "{DesktopBackend.STEAM}", not as "{request.format}".')

    def installer(self, product: BuildProduct, request: PackageRequest) -> tuple[str, ...]:
        if request.options:
            raise InvalidOption(f'The format "{DesktopBackend.INSTALLER}" takes no options.')
        resolved = product.prepared.product
        folders = product.prepared.folders
        if resolved.platform == "linux":
            DesktopBackend.require_categories(product)
            for tool in DesktopBackend.DEBIAN_TOOLS:
                tool.require()
            DesktopBackend.require_appimage_generator(self.context.process.capture(["cpack", "--version"]))
        tools = [f"-DCPACK_INNOSETUP_EXECUTABLE={InnoSetup.compiler(self.context)}"] if resolved.platform == "windows" else AppImageTools.ensure(self.context).cpack_options()
        staging = folders.root / "packages"
        Folders.remove(staging)
        self.context.terminal.step(f"Packaging `{Repository.shown(resolved.app.folder)}` for {resolved.platform.capitalize()} with CPack")
        self.context.process.run(["cpack", "--config", DesktopBackend.cmake_folder(folders) / "CPackConfig.cmake", "-C", "Release", "-B", staging, *tools])

        request.output.mkdir(parents=True, exist_ok=True)
        packaged = []
        for package in sorted(path for path in staging.iterdir() if path.is_file() and path.suffix in DesktopBackend.PACKAGES):
            shutil.copy2(package, request.output / package.name)
            packaged.append(str(request.output / package.name))
            self.context.terminal.success(f"Packaged `{Repository.shown(request.output / package.name)}`.")
        if not packaged:
            raise BuildFailed(f"CPack made no package in `{Repository.shown(staging)}`. Its output in the log of the command shows why.")
        return tuple(packaged)

    @staticmethod
    def require_categories(product: BuildProduct) -> None:
        """Stops the Linux installer of an app whose desktop entry lists no categories, since the AppImage tool refuses such an entry."""
        app = product.prepared.product.app
        entry = (product.prepared.folders.project / LinuxEntries.ENTRY).read_text(encoding="utf-8")
        if LinuxEntries.has_categories(entry):
            return
        values = ", ".join(f'"{category.value}"' for category in AppCategory)
        raise InvalidManifest(
            f'The Linux installer holds an AppImage, which needs the category of the app, and `{app.origin}` names none. Add "category" to it with one of {values}.',
            origin=app.origin,
        )

    @staticmethod
    def require_appimage_generator(version: str) -> None:
        """Stops the Linux installer when the CPack of this machine, whose `--version` answer is the text, comes from a CMake older than the AppImage generator."""
        found = re.search(r"(\d+)\.(\d+)", version)
        if found is not None and (int(found.group(1)), int(found.group(2))) >= DesktopBackend.APPIMAGE_CPACK:
            return
        needed = ".".join(str(part) for part in DesktopBackend.APPIMAGE_CPACK)
        shown = found.group(0) if found is not None else "of an unknown version"
        raise MissingToolchain(
            f'The Linux installer holds an AppImage, which the AppImage generator of CPack makes from CMake {needed} on, and the "cpack" of this machine is {shown}. Install a newer CMake, such as with "pip install cmake", and package the app again.'
        )

    def steam(self, product: BuildProduct, request: PackageRequest) -> tuple[str, ...]:
        """Lays the installed app out as the depot of its platform and its named packs as depots of their own."""
        options = SteamDepots.Options.parse(request.options)
        resolved = product.prepared.product
        source = ReleasePackage.of(resolved)
        profile = self.profile(resolved)
        release: ProtectedRelease = ProtectedReleases.last(resolved.app, profile)
        layout = DesktopLayout.of(resolved.platform, DesktopApp.of_product(resolved), product.path)
        script = SteamDepots(self.context).stage(source, release, depot=resolved.platform, product=product.path, bundle=False, packs_place=layout.home, options=options, output=request.output)
        return (str(script),)
