from dataclasses import dataclass

from haylen.assets.app_shaders import AppShaders
from haylen.content.content_tool import ContentTool
from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
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
from haylen.sdk.sdk_builder import SdkBuilder
from haylen.sdk.sdk_catalog import SdkCatalog
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_selector import SdkSelector


@dataclass(frozen=True)
class AppPipeline:
    """The one pipeline of every app, Lua or C++, on every target: the manifest, the target and its capabilities, the plugins of the target, the SDK, the resolved product, then the preparation, the build, the verification and the launch by the backend of the target. Each step checks that the backend declares the capability before it calls it, and the commands call these steps instead of any backend directly."""

    # What the step of an SDK that an app command builds tells the developer, since its first build compiles the whole engine.
    SDK_BUILD_NOTE = "No installed SDK holds the engine of these sources for the target, so the command builds it once before the app. A first build compiles the whole engine, which takes about ten minutes with a warm compiler cache and up to an hour without one, and a later build compiles only what changed."

    # What the step of the SDK of this machine that a Release build builds tells the developer.
    HOST_TOOLS_NOTE = "No installed SDK of these sources holds the content tool of this machine, which builds the protected release of a Release build, so the command builds the SDK that carries it once before the app. A first build compiles the whole engine, which takes about ten minutes with a warm compiler cache and up to an hour without one, and a later build compiles only what changed."

    context: ToolingContext
    registry: BackendRegistry

    def resolve(self, request: ProductRequest, *, build_sdk: bool) -> ResolvedProduct:
        """Resolves the product of a request, its target, plugins and contributions, before anything builds, so a project with a problem fails at once. When only the SDK stops the resolution, a command that builds the app builds the SDK of the architectures that no installed SDK of the current engine sources holds, through the engine port, as `haylen sdk build` does, and resolves again, while a command that only reads the product, such as `project inspect`, stops at the missing SDK. A command that builds a Release also builds the SDK of this machine that carries the content tool when no installed SDK of the current engine sources holds it."""
        product = self.resolve_with_sdk(request, build_sdk)
        if build_sdk and request.configuration is Configuration.RELEASE:
            self.build_host_tools(request)
        return product

    def resolve_with_sdk(self, request: ProductRequest, build_sdk: bool) -> ResolvedProduct:
        app = AppManifest.load(request.app_folder)
        try:
            return self.resolver(app).resolve(request)
        except IncompatibleSdk:
            missing = self.missing_architectures(request) if build_sdk else ()
            if not missing:
                raise

        SdkBuilder(self.context, self.registry).build((request.target,), request.engine_configuration, missing, None, AppPipeline.SDK_BUILD_NOTE)
        return self.resolver(app).resolve(request)

    def build_host_tools(self, request: ProductRequest) -> None:
        """Builds the SDK of this machine whose engine port carries the content tool, in the configuration of the engine, when no installed SDK of the current engine sources holds the tool, and fails with the sentence of the tool when no installed port carries it."""
        try:
            ContentTool.locate(self.context, self.registry)
            return
        except IncompatibleSdk:
            hosts = ContentTool.host_targets(self.registry)
            if not hosts:
                raise

        SdkBuilder(self.context, self.registry).build(hosts[:1], request.engine_configuration, None, None, AppPipeline.HOST_TOOLS_NOTE)

    def resolver(self, app: AppManifest) -> ProjectResolver:
        source = PluginSource(app.folder, PluginLock.load(app.folder), self.context.process, self.context.terminal)
        selector = SdkSelector(self.installed_sdks(), Repository.engine_revision())
        return ProjectResolver(self.registry, PluginResolver(app, source), selector)

    def installed_sdks(self) -> tuple[SdkManifest, ...]:
        """The installed SDKs that this tooling uses. One that it cannot use, such as one that an older version of Haylen built, serves no target, so a build builds its targets again."""
        return tuple(SdkCatalog(self.context.environment).manifests())

    def missing_architectures(self, request: ProductRequest) -> tuple[str, ...]:
        """Returns the architectures of the request that no installed SDK of the current engine sources holds in the configuration of the engine, which only a build of the SDK makes."""
        description = self.registry.target(request.target).description
        selector = SdkSelector(self.installed_sdks(), Repository.engine_revision())
        return tuple(target.architecture for target in ProjectResolver.target_ids(description, request) if selector.lacks(target, request.engine_configuration))

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
