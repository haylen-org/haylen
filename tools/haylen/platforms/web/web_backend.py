import json
from pathlib import Path
from types import MappingProxyType

from haylen.content.protected_releases import ProtectedReleases
from haylen.content.release_entry import ReleaseEntry
from haylen.content.release_package import ReleasePackage
from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_environment import TargetEnvironment
from haylen.core.target_id import TargetId
from haylen.development.server_options import ServerOptions
from haylen.development.site_run import SiteRun
from haylen.native.cmake_tree import CMakeTree
from haylen.platforms.backend_registry import BackendRegistry
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
from haylen.platforms.verification_check import VerificationCheck
from haylen.platforms.verification_report import VerificationReport
from haylen.platforms.web.web_port import WebPort
from haylen.platforms.web.web_project import WebProject
from haylen.platforms.web.web_site import WebSite
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.emscripten import Emscripten


class WebBackend(PlatformBackend):
    """The backend of the target `web`, which makes the site of an app that a browser runs through WebGPU or WebGL2. A Debug build of a Lua app plays its zipped package in the prebuilt web player of the web SDK and compiles nothing, while a Release build and a C++ app link a runtime of their own against the web SDK with Emscripten, a release with the protected release of the app, whose content streams by range from `app/` of the site, and its bootstrap. The run serves the site, in development with the development server on the same port."""

    NAME = "web"
    VERSION = Repository.engine_version()
    TARGET = "web"
    PLATFORM = "web"
    SOURCE = "web"
    # The runtime of the page and the files that it loads from next to its script, which every site holds.
    SITE_FILES = ("index.html", WebProject.LOADER, WebProject.CONFIG, *WebPort.PLAYER_FILES)
    WEB_MODULE = {
        "type": "object",
        "additionalProperties": False,
        "required": ["path"],
        "properties": {"path": {"type": "string", "minLength": 1, "description": "The ES module of the plugin, relative to the plugin folder, whose folder goes to the site with it."}},
    }
    SECTION = {
        "type": "object",
        "additionalProperties": False,
        "properties": {
            "scripts": {
                "type": "array",
                "items": {"type": "string", "minLength": 1},
                "uniqueItems": True,
                "description": "The page scripts of the app, files of the app folder that load after the loader.",
            }
        },
    }
    # A screen of a plugin opens a popup, which the opener policy `same-origin` cuts off from the page that opened it, and a module registers its screens by name.
    SCREEN_CALL = "registerScreen("
    ISOLATING_OPENER = "same-origin"

    def describe(self) -> PlatformDescription:
        target = TargetDescription(
            name=WebBackend.TARGET,
            system=WebPort.SYSTEM,
            platform=WebBackend.PLATFORM,
            environment=TargetEnvironment.DEVICE,
            abis=WebPort.ABIS,
            default_architectures=(WebPort.ARCHITECTURE,),
            toolchain=WebPort.TOOLCHAIN,
            kinds=frozenset({AppKind.LUA, AppKind.CPP}),
            hosts=frozenset({"macos", "windows", "linux"}),
            host_default=frozenset(),
            requires=frozenset(),
            release_profile=WebBackend.PLATFORM,
            description="A site that runs the app in a browser through WebGPU or WebGL2.",
        )
        platforms = frozenset({WebBackend.PLATFORM})
        module = ContributionType(
            WebProject.CONTRIBUTION, platforms, MappingProxyType(WebBackend.WEB_MODULE), (), MergeMode.ACCUMULATE, "An ES module that the page imports and loads before the app starts."
        )
        section = OverrideSection(WebBackend.PLATFORM, platforms, MappingProxyType(WebBackend.SECTION), MappingProxyType({}), "The page scripts and the files of the site of the app.")
        capabilities = frozenset({Capability.PREPARE, Capability.BUILD, Capability.VERIFY, Capability.RUN})
        return PlatformDescription(
            WebBackend.NAME,
            Contract.BACKEND,
            WebBackend.VERSION,
            (target,),
            capabilities,
            ("cmake", "ninja", "Emscripten"),
            MappingProxyType({WebProject.CONTRIBUTION: module}),
            MappingProxyType({WebBackend.PLATFORM: section}),
        )

    def check_app(self, app: AppManifest, target: str) -> None:
        """Every app suits the targets of this backend, whatever `haylen.json` chooses."""

    def check_environment(self, target: TargetId) -> EnvironmentReport:
        """Needs CMake and Ninja to link a runtime. Emscripten installs on the first build that links one."""
        return EnvironmentReport((BuildTools.CMAKE.status(), BuildTools.NINJA.status()), ())

    @staticmethod
    def cmake_folder(folders: ProductFolders) -> Path:
        return folders.build / "cmake"

    def releases(self) -> ProtectedReleases:
        return ProtectedReleases.locate(self.context, BackendRegistry.discover(self.context))

    def prepare(self, product: ResolvedProduct, folders: ProductFolders) -> PreparedProject:
        """Generates the site project, after the staged package of a C++ app in Debug, or the protected release with its bootstrap in Release, which the content tool builds every time, since the keys of the app are no input of the preparation."""
        preparation = Preparation(self.context, product, folders, WebBackend.NAME, WebBackend.VERSION, WebProject.TEMPLATE)
        release = product.configuration is Configuration.RELEASE
        content = None
        bootstrap = None
        if release:
            releases = self.releases()
            content = releases.build(ReleasePackage.of(product), WebBackend.PLATFORM, product.request.previous_release).shipped
            bootstrap = releases.bootstrap_source(product.app, WebBackend.PLATFORM)
        elif preparation.is_current() and (product.app.kind is AppKind.LUA or folders.package.is_dir()):
            return preparation.unchanged(folders.project)
        elif product.app.kind is AppKind.CPP:
            content = product.package.stage(folders.package)

        tree = GeneratedTree(folders.project)
        WebProject(product, content, bootstrap).write(tree)
        return preparation.finish(tree, folders.project)

    def build(self, prepared: PreparedProject) -> BuildProduct:
        """Makes the site in the products of the product: with the prebuilt web player, or with the runtime that Emscripten links from the CMake project of the site."""
        resolved = prepared.product
        folders = prepared.folders
        release = None
        release_files: tuple[str, ...] = ()
        runtime = self.link(prepared) if WebProject.links_runtime(resolved) else resolved.primary.sdk.file("player")
        if resolved.configuration is Configuration.RELEASE:
            releases = self.releases()
            source = ReleasePackage.of(resolved)
            release = ProtectedReleases.last(resolved.app, WebBackend.PLATFORM)
            release_files = releases.app_files(source, release)

        site = WebSite(self.context, prepared.project, folders.products)
        site.assemble(runtime, release, release_files)
        if release is not None:
            site.minify()
        components = tuple(ProductComponent(project.target, folders.products / "haylen.wasm", "runtime") for project in resolved.projects)
        symbols = WebBackend.cmake_folder(folders) / "symbols"
        auxiliary = (symbols,) if symbols.is_dir() else ()
        return BuildProduct(prepared, folders.products, "site", components, auxiliary)

    def link(self, prepared: PreparedProject) -> Path:
        """Links the runtime of the page with Emscripten and returns the folder of its files."""
        configuration = prepared.product.configuration.value
        tree = CMakeTree(self.context, WebBackend.cmake_folder(prepared.folders))
        if not tree.configured or Emscripten.is_stale(tree.folder):
            command = ["cmake", "-S", prepared.project / WebProject.RUNTIME, "-B", tree.folder, "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={configuration}", CMakeTree.WITHOUT_MODULE_SCAN]
            Emscripten.configure(self.context, tree, command)
        self.context.terminal.step(f'Linking the runtime of the page of "{prepared.product.app.name}"')
        tree.build(["haylen"], configuration)
        return tree.folder / "bin" / "haylen"

    def verify(self, product: BuildProduct) -> VerificationReport:
        """Checks the site: the page with its runtime, every plugin with a web module in `config.json` with its module, and in the Release configuration anything that a release never holds, such as Lua text, files of the package as they are, a zipped package or a key of the app."""
        site = product.path
        resolved = product.prepared.product
        failed = [f'The site lacks "{name}", which a page of the app loads.' for name in WebBackend.SITE_FILES if not (site / name).is_file()]
        listed = {}
        if (site / WebProject.CONFIG).is_file():
            listed = {entry["id"]: entry for entry in json.loads((site / WebProject.CONFIG).read_text(encoding="utf-8"))["plugins"]}
        for contribution in resolved.primary.of_type(WebProject.CONTRIBUTION):
            owner = contribution.owners[0]
            entry = listed.get(owner)
            if entry is None or not (site / entry["module"]).is_file():
                failed.append(f'The site lacks the web module of the plugin "{owner}" that "config.json" lists. Prepare the app again.')

        checks = [VerificationCheck.failed("site", problem, WebBackend.SOURCE) for problem in failed]
        if not checks:
            checks.append(VerificationCheck.passed("site", f"The site `{Repository.shown(site)}` holds the page, its runtime and the web modules of the plugins.", WebBackend.SOURCE))
        if resolved.configuration is Configuration.RELEASE:
            checks += self.releases().inspection(ReleasePackage.of(resolved)).checks(ReleaseEntry.of_folder(site), "app")
            checks += [
                VerificationCheck.failed("release", f'The site holds the zipped package "{path.relative_to(site).as_posix()}", while a release ships only its protected release.', WebBackend.SOURCE)
                for path in sorted(site.rglob("*.zip"))
            ]
        return VerificationReport(tuple(checks))

    def run(self, product: BuildProduct, launch: LaunchRequest) -> None:
        """Serves the site, in development with the Debug configuration, where the development server pushes every saved file of the app to the open pages, and as it ships with the Release configuration."""
        screens = WebBackend.screen_plugins(product.prepared.product)
        if launch.options.get(ServerOptions.COOP) == WebBackend.ISOLATING_OPENER and screens:
            names = ", ".join(f'"{plugin}"' for plugin in screens)
            raise InvalidOption(
                f'The plugins {names} open screens, whose popups the header "Cross-Origin-Opener-Policy: same-origin" cuts off from the app. Serve the page with "--coop same-origin-allow-popups" or "--coop off".'
            )
        with SiteRun(self.context, product, launch, product.path) as run:
            run.serve()

    @staticmethod
    def screen_plugins(product: ResolvedProduct) -> list[str]:
        """The plugins whose web modules open screens."""
        found = []
        for contribution in product.primary.of_type(WebProject.CONTRIBUTION):
            folder = (contribution.folders[0] / str(contribution.field("path"))).parent
            scripts = [path for path in folder.rglob("*") if path.suffix in WebSite.MODULE_SUFFIXES]
            if any(WebBackend.SCREEN_CALL in path.read_text(encoding="utf-8") for path in scripts):
                found.append(contribution.owners[0])
        return found
