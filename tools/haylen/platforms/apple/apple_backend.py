from haylen.content.protected_releases import ProtectedReleases
from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_id import TargetId
from haylen.platforms.apple.app_bundle import AppBundle
from haylen.platforms.apple.apple_contributions import AppleContributions
from haylen.platforms.apple.apple_devices import AppleDevices
from haylen.platforms.apple.apple_launcher import AppleLauncher
from haylen.platforms.apple.apple_project import AppleProject
from haylen.platforms.apple.apple_target import AppleTarget
from haylen.platforms.apple.apple_targets import AppleTargets
from haylen.platforms.apple.apple_verifier import AppleVerifier
from haylen.platforms.apple.background_asset_packs import BackgroundAssetPacks
from haylen.platforms.apple.xcode_tools import XcodeTools
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.capability import Capability
from haylen.platforms.contract import Contract
from haylen.platforms.device import Device
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.platform_backend import PlatformBackend
from haylen.platforms.platform_description import PlatformDescription
from haylen.platforms.preparation import Preparation
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_component import ProductComponent
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.verification_report import VerificationReport


class AppleBackend(PlatformBackend):
    """The backend of macOS, Mac Catalyst, iOS and iPadOS, tvOS and visionOS on their devices and simulators. Lua and C++ apps go through one generated XcodeGen project with one app target, which compiles the Swift, Objective-C, C and C++ sources of the app and its plugins, links `Haylen.xcframework` of the Apple SDK, and the Lua layer of `HaylenLua.xcframework` for an app that runs Lua, and resolves the Swift packages of the plugins with Xcode. It builds with xcodebuild into the build folder of the product, verifies the built app and runs it on this Mac, a simulator or a device. The target `macos` is the default target of C++ apps on macOS."""

    NAME = "apple"
    VERSION = Repository.engine_version()
    TOOLS = ("xcodebuild", "xcodegen", "cmake")
    SCHEME = AppleProject.TARGET

    def describe(self) -> PlatformDescription:
        host = Host.architecture()
        capabilities = frozenset({Capability.PREPARE, Capability.BUILD, Capability.VERIFY, Capability.RUN, Capability.DEVICES})
        targets = tuple(target.describe(host) for target in AppleTargets.ALL)
        return PlatformDescription(
            AppleBackend.NAME,
            Contract.BACKEND,
            AppleBackend.VERSION,
            targets,
            capabilities,
            AppleBackend.TOOLS,
            AppleContributions.types(),
            AppleContributions.sections(),
        )

    def check_environment(self, target: TargetId) -> EnvironmentReport:
        if Host.system() != "macos":
            return EnvironmentReport((), (f'Xcode builds the apps of Apple platforms only on macOS, and this machine runs "{Host.system()}".',))
        apple = AppleTargets.of_system(target.system, target.environment)
        return EnvironmentReport((XcodeTools.xcode(), XcodeTools.sdk(apple.sdk, apple.xcode_platform), *XcodeTools.host_tools()), ())

    @staticmethod
    def target_of(product: ResolvedProduct) -> AppleTarget:
        return AppleTargets.named(product.target_name)

    def prepare(self, product: ResolvedProduct, folders: ProductFolders) -> PreparedProject:
        """Generates the project, with the protected release and its bootstrap in Release, which the content tool builds every time, since the keys of the app and the release it follows are no inputs of the preparation. An app for a device names its identifier, which Apple registers for the team that signs it."""
        target = AppleBackend.target_of(product)
        if target.device:
            product.app.require_identifier(f'Signing an app for a device of "{target.name}" needs an identifier that Apple registers for your team')
        project = AppleProject(self.context, product, target, folders)
        preparation = Preparation(product, folders, AppleBackend.NAME, AppleBackend.VERSION, AppleProject.TEMPLATE)
        current = product.configuration is Configuration.DEBUG and preparation.is_current()
        if current and (project.xcode_project / "project.pbxproj").is_file():
            return preparation.unchanged(project.xcode_project)
        tree = project.generate()
        return preparation.finish(tree, project.xcode_project)

    def build(self, prepared: PreparedProject) -> BuildProduct:
        product = prepared.product
        target = AppleBackend.target_of(product)
        configuration = product.configuration.value
        derived = prepared.folders.build / "derived"
        command: list[object] = [
            "xcodebuild",
            "-project",
            prepared.project,
            "-scheme",
            AppleBackend.SCHEME,
            "-configuration",
            configuration,
            "-destination",
            target.destination,
            "-derivedDataPath",
            derived,
            "-jobs",
            self.context.jobs,
            "build",
        ]
        if target.device:
            command.append("-allowProvisioningUpdates")
        self.context.process.run(command)

        products = derived / "Build" / "Products" / f"{configuration}{target.products}"
        path = products / f"{product.app.name}.app"
        if not path.is_dir():
            raise BuildFailed(f'Xcode built no app `{Repository.shown(path)}` for "{target.name}", although it succeeded. Prepare the app again with "haylen app prepare".')
        executable = AppBundle(self.context, path, target.runs_on_mac).executable
        components = tuple(ProductComponent(project.target, executable, "code") for project in product.projects)

        auxiliary = []
        if product.configuration is Configuration.RELEASE:
            auxiliary.append(products / f"{product.app.name}.app.dSYM")
            release = ProtectedReleases.last(product.app, AppleTarget.RELEASE_PROFILE)
            auxiliary += BackgroundAssetPacks(self.context).package(product.primary, target, release, prepared.folders.products / "asset-packs")
        return BuildProduct(prepared, path, "app", components, tuple(auxiliary))

    def verify(self, product: BuildProduct) -> VerificationReport:
        return AppleVerifier(self.context).verify(product, AppleBackend.target_of(product.prepared.product))

    def devices(self) -> tuple[Device, ...]:
        return AppleDevices(self.context).all()

    def run(self, product: BuildProduct, launch: LaunchRequest) -> None:
        AppleLauncher(self.context).run(product, launch, AppleBackend.target_of(product.prepared.product))
