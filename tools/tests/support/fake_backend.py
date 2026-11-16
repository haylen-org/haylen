from types import MappingProxyType

from haylen.core.app_kind import AppKind
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_environment import TargetEnvironment
from haylen.core.target_id import TargetId
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.capability import Capability
from haylen.platforms.contract import Contract
from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.merge_mode import MergeMode
from haylen.platforms.override_section import OverrideSection
from haylen.platforms.platform_backend import PlatformBackend
from haylen.platforms.platform_description import PlatformDescription
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.target_description import TargetDescription
from haylen.platforms.verification_report import VerificationReport
from haylen.plugins.core_contributions import CoreContributions


class FakeBackend(PlatformBackend):
    """A backend of the tests that declares the platform `fake` with a device target of two architectures and a simulator target, a few contribution types of each merge mode and the section `fake`. It never builds, since the tests of the core check only what reaches a backend."""

    PLATFORM = "fake"
    CONTRACT = Contract.BACKEND
    FAILURE: str | None = None

    @staticmethod
    def variant(name: str, **attributes: object) -> type:
        """Returns a backend class of another name with other class attributes, such as another `CONTRACT` or a `FAILURE` that its description raises."""
        return type(name, (FakeBackend,), attributes)

    def describe(self) -> PlatformDescription:
        if self.FAILURE is not None:
            raise RuntimeError(self.FAILURE)
        platforms = frozenset({FakeBackend.PLATFORM})
        package = {
            "type": "object",
            "additionalProperties": False,
            "required": ["url", "version"],
            "properties": {"url": {"type": "string"}, "version": {"type": "string"}, "products": {"type": "array", "items": {"type": "string"}}},
        }
        dictionary = {"type": "object", "additionalProperties": False, "required": ["values"], "properties": {"values": {"type": "object"}}}
        exclusive = {"type": "object", "properties": {"packs": {"type": "array"}}}
        types = {
            "package": ContributionType("package", platforms, MappingProxyType(package), ("url",), MergeMode.UNIQUE, "A package.", ("products",)),
            "settings": ContributionType("settings", platforms, MappingProxyType(dictionary), (), MergeMode.DICTIONARY, "Settings."),
            "packs": ContributionType("packs", platforms, MappingProxyType(exclusive), (), MergeMode.EXCLUSIVE, "Packs."),
            CoreContributions.NATIVE_LIBRARY: CoreContributions.native_library(platforms),
        }
        section = {"type": "object", "additionalProperties": False, "properties": {"settings": {"type": "object"}, "signing": {"type": "string"}}}
        sections = {"fake": OverrideSection("fake", platforms, MappingProxyType(section), MappingProxyType({"settings": "settings"}), "The fake platform.")}
        targets = (
            FakeBackend.target("fake", TargetEnvironment.DEVICE, {"arm64": "fake-arm64", "x86_64": "fake-x86_64"}, ("arm64",)),
            FakeBackend.target("fake-simulator", TargetEnvironment.SIMULATOR, {"arm64": "fake-arm64"}, ("arm64",)),
        )
        capabilities = frozenset({Capability.PREPARE, Capability.BUILD, Capability.VERIFY})
        return PlatformDescription("fake", self.CONTRACT, "1.0.0", targets, capabilities, (), MappingProxyType(types), MappingProxyType(sections))

    @staticmethod
    def target(name: str, environment: TargetEnvironment, abis: dict[str, str], defaults: tuple[str, ...]) -> TargetDescription:
        return TargetDescription(
            name=name,
            system="fakeos",
            platform=FakeBackend.PLATFORM,
            environment=environment,
            abis=MappingProxyType(abis),
            default_architectures=defaults,
            toolchain="fakecc",
            kinds=frozenset({AppKind.LUA, AppKind.CPP}),
            hosts=frozenset({"macos", "linux", "windows"}),
            host_default=frozenset(),
            requires=frozenset({"dynamicLibraries"}),
            release_profile="fake",
            description=f"The fake {environment.value}.",
        )

    def check_environment(self, target: TargetId) -> EnvironmentReport:
        return EnvironmentReport((), ())

    def prepare(self, product: ResolvedProduct, folders: ProductFolders) -> PreparedProject:
        raise NotImplementedError

    def build(self, prepared: PreparedProject) -> BuildProduct:
        raise NotImplementedError

    def verify(self, product: BuildProduct) -> VerificationReport:
        raise NotImplementedError
