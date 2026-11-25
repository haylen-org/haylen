from abc import ABC, abstractmethod

from haylen.core.app_manifest import AppManifest
from haylen.core.errors.missing_capability import MissingCapability
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_id import TargetId
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.device import Device
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.package_request import PackageRequest
from haylen.platforms.platform_description import PlatformDescription
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.signing_request import SigningRequest
from haylen.platforms.verification_report import VerificationReport


class PlatformBackend(ABC):
    """The contract of a backend, which turns a resolved product into a native project, builds it with the native tools of its platform, verifies what it built and, where it declares the capabilities, lists devices, runs, packages and signs apps. A backend registers itself through the entry point group `haylen.backends`, the internal ones like any other, and the core reaches it only through this contract. Its failures are the typed errors of `haylen.core.errors`, and none of them means that another backend should try."""

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    @abstractmethod
    def describe(self) -> PlatformDescription:
        """Declares the targets, capabilities, tools, contribution types and settings of the backend. It never touches the machine."""

    @abstractmethod
    def check_environment(self, target: TargetId) -> EnvironmentReport:
        """Finds the tools that a target needs on this machine, without downloading proprietary SDKs or accepting licenses."""

    @abstractmethod
    def check_app(self, app: AppManifest, target: str) -> None:
        """Fails when a choice of `haylen.json` contradicts a target of the backend, such as an app for iPhones alone and the Mac Catalyst app, before the SDK or the app builds."""

    @abstractmethod
    def prepare(self, product: ResolvedProduct, folders: ProductFolders) -> PreparedProject:
        """Generates the native project of a product into its folders, the same way for the same inputs, writing only what changed."""

    @abstractmethod
    def build(self, prepared: PreparedProject) -> BuildProduct:
        """Builds a prepared project with the native tools of the platform."""

    @abstractmethod
    def verify(self, product: BuildProduct) -> VerificationReport:
        """Examines a built product for what only this platform knows, such as its architectures, metadata, bundles and modules."""

    def devices(self) -> tuple[Device, ...]:
        raise MissingCapability(f'The backend "{self.describe().name}" lists no devices.')

    def run(self, product: BuildProduct, launch: LaunchRequest) -> None:
        raise MissingCapability(f'The backend "{self.describe().name}" does not run apps.')

    def package(self, product: BuildProduct, request: PackageRequest) -> tuple[str, ...]:
        raise MissingCapability(f'The backend "{self.describe().name}" does not package apps.')

    def sign(self, request: SigningRequest) -> tuple[str, ...]:
        raise MissingCapability(f'The backend "{self.describe().name}" does not create signing keys.')
