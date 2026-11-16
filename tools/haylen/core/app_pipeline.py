from dataclasses import dataclass

from haylen.assets.app_shaders import AppShaders
from haylen.core.app_manifest import AppManifest
from haylen.core.errors.missing_capability import MissingCapability
from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.generic_verifier import GenericVerifier
from haylen.core.product_record import ProductRecord
from haylen.core.product_request import ProductRequest
from haylen.core.project_resolver import ProjectResolver
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.capability import Capability
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.package_request import PackageRequest
from haylen.platforms.platform_backend import PlatformBackend
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.signing_request import SigningRequest
from haylen.platforms.verification_report import VerificationReport
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_resolver import PluginResolver
from haylen.plugins.plugin_source import PluginSource
from haylen.sdk.sdk_catalog import SdkCatalog
from haylen.sdk.sdk_selector import SdkSelector


@dataclass(frozen=True)
class AppPipeline:
    """The one pipeline of every app, Lua or C++, on every target: the manifest, the target and its capabilities, the plugins of the target, the SDK, the resolved product, then the preparation, the build, the verification and the launch by the backend of the target. Each step checks that the backend declares the capability before it calls it, and the commands call these steps instead of any backend directly."""

    context: ToolingContext
    registry: BackendRegistry

    def resolve(self, request: ProductRequest) -> ResolvedProduct:
        app = AppManifest.load(request.app_folder)
        source = PluginSource(app.folder, PluginLock.load(app.folder), self.context.process, self.context.terminal)
        selector = SdkSelector(tuple(SdkCatalog(self.context.environment).manifests()), Repository.engine_revision())
        return ProjectResolver(self.registry, PluginResolver(app, source), selector).resolve(request)

    def backend(self, product: ResolvedProduct, capability: Capability) -> PlatformBackend:
        target = self.registry.target(product.target_name)
        if capability not in target.platform.capabilities:
            raise MissingCapability(f'The backend "{target.platform.name}" of "{product.target_name}" does not {capability.value} apps.')
        return target.backend

    def require(self, product: ResolvedProduct, capability: Capability) -> None:
        """Fails when the backend of a resolved product lacks a capability that a command ends with, such as running or packaging, before the command prepares or builds anything."""
        self.backend(product, capability)

    def check(self, product: ResolvedProduct) -> None:
        """Fails before any native tool runs when this machine lacks a tool that the targets of the product need."""
        backend = self.registry.target(product.target_name).backend
        for project in product.projects:
            report = backend.check_environment(project.target)
            if not report.ready:
                lines = "\n".join(f"  {problem}" for problem in report.missing)
                raise MissingToolchain(f'This machine lacks what "{product.target_name}" needs. The command "haylen doctor --target {product.target_name}" shows it too:\n{lines}')

    def prepare(self, product: ResolvedProduct) -> PreparedProject:
        backend = self.backend(product, Capability.PREPARE)
        self.check(product)
        AppShaders(self.context, product.primary.sdk).update(product.app.folder)
        self.context.terminal.step(f'Preparing the project of "{product.app.name}" for "{product.target_name}" in the "{product.configuration.value}" configuration')
        return backend.prepare(product, ProductFolders.of(product.app.folder, product.identity))

    def build(self, prepared: PreparedProject) -> BuildProduct:
        backend = self.backend(prepared.product, Capability.BUILD)
        self.context.terminal.step(f'Building "{prepared.product.app.name}" for "{prepared.product.target_name}"')
        product = backend.build(prepared)
        ProductRecord.save(product)
        return product

    def last_build(self, product: ResolvedProduct) -> BuildProduct:
        """Returns the product that the last build of a resolved product made, which `app verify` examines without building again."""
        return ProductRecord.load(product, self.registry.target(product.target_name).platform.name)

    def verify(self, product: BuildProduct) -> VerificationReport:
        backend = self.backend(product.prepared.product, Capability.VERIFY)
        report = GenericVerifier.verify(product)
        if report.passed:
            report = report.joined(backend.verify(product))
        return report

    def run(self, product: BuildProduct, launch: LaunchRequest) -> None:
        self.backend(product.prepared.product, Capability.RUN).run(product, launch)

    def package(self, product: BuildProduct, request: PackageRequest) -> tuple[str, ...]:
        return self.backend(product.prepared.product, Capability.PACKAGE).package(product, request)

    def sign(self, target_name: str, request: SigningRequest) -> tuple[str, ...]:
        """Creates a signing key of an app for a target, which needs no resolved project, through the backend of the target when it declares the capability."""
        target = self.registry.target(target_name)
        if Capability.SIGN not in target.platform.capabilities:
            raise MissingCapability(f'The backend "{target.platform.name}" of "{target_name}" does not create signing keys.')
        return target.backend.sign(request)
