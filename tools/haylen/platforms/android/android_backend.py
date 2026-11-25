import dataclasses
import hashlib
import shutil
from types import MappingProxyType

from haylen.content.protected_release import ProtectedRelease
from haylen.content.protected_releases import ProtectedReleases
from haylen.content.release_package import ReleasePackage
from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_environment import TargetEnvironment
from haylen.core.target_id import TargetId
from haylen.platforms.android.android_asset_packs import AndroidAssetPacks
from haylen.platforms.android.android_bundle_install import AndroidBundleInstall
from haylen.platforms.android.android_contributions import AndroidContributions
from haylen.platforms.android.android_gradle_build import AndroidGradleBuild
from haylen.platforms.android.android_launcher import AndroidLauncher
from haylen.platforms.android.android_native_build import AndroidNativeBuild
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.android.android_project import AndroidProject
from haylen.platforms.android.android_signing_key import AndroidSigningKey
from haylen.platforms.android.android_template import AndroidTemplate
from haylen.platforms.android.android_verifier import AndroidVerifier
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.capability import Capability
from haylen.platforms.contract import Contract
from haylen.platforms.device import Device
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.package_request import PackageRequest
from haylen.platforms.platform_backend import PlatformBackend
from haylen.platforms.platform_description import PlatformDescription
from haylen.platforms.preparation import Preparation
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.signing_request import SigningRequest
from haylen.platforms.target_description import TargetDescription
from haylen.platforms.verification_report import VerificationReport
from haylen.toolchains.android_debug_bridge import AndroidDebugBridge
from haylen.toolchains.android_ndk import AndroidNdk
from haylen.toolchains.android_sdk import AndroidSdk
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.bundletool import Bundletool
from haylen.toolchains.gradle import Gradle
from haylen.toolchains.java import Java


class AndroidBackend(PlatformBackend):
    """The backend of Android, whose target `android` builds one APK for phones, tablets and Android TV with every ABI it is asked for, one resolved project per ABI. It generates the whole Gradle project of the app from its template, which Gradle builds with the Android libraries of the engine SDK, and the external native build of Gradle compiles the native code of the app for each ABI against the engine SDK of that ABI. It lists devices and emulators, runs apps on them, packages bundles and creates the keys that sign apps."""

    NAME = "android"
    VERSION = Repository.engine_version()
    BUNDLE = "bundle"
    APKS = "apks"

    def describe(self) -> PlatformDescription:
        target = TargetDescription(
            name=AndroidPlatform.TARGET,
            system=AndroidPlatform.SYSTEM,
            platform=AndroidPlatform.PLATFORM,
            environment=TargetEnvironment.DEVICE,
            abis=AndroidPlatform.ABIS,
            default_architectures=AndroidNdk.ABIS,
            toolchain=AndroidPlatform.TOOLCHAIN,
            kinds=frozenset({AppKind.LUA, AppKind.CPP}),
            hosts=frozenset({"macos", "linux", "windows"}),
            host_default=frozenset(),
            requires=frozenset({"dynamicLibraries"}),
            release_profile=AndroidPlatform.RELEASE_PROFILE,
            description="Phones, tablets and Android TV, in one APK with every ABI it is built for.",
        )
        capabilities = frozenset(Capability)
        return PlatformDescription(
            AndroidBackend.NAME,
            Contract.BACKEND,
            AndroidBackend.VERSION,
            (target,),
            capabilities,
            ("Android SDK", "Android NDK", "JDK", "Gradle", "bundletool", "cmake", "ninja", "adb"),
            MappingProxyType(AndroidContributions.types()),
            MappingProxyType({AndroidContributions.SECTION: AndroidContributions.section()}),
        )

    def check_app(self, app: AppManifest, target: str) -> None:
        """Every app suits the targets of this backend, whatever `haylen.json` chooses."""

    def check_environment(self, target: TargetId) -> EnvironmentReport:
        sdk = AndroidSdk(self.context.environment)
        tools = (sdk.status()[0], AndroidNdk(sdk).status(), Java(self.context.environment).status(), Gradle.status(), Bundletool.status(), BuildTools.CMAKE.status(), BuildTools.NINJA.status())
        return EnvironmentReport(tools, ())

    def releases(self) -> ProtectedReleases:
        return ProtectedReleases.locate(self.context, BackendRegistry.discover(self.context))

    def prepare(self, product: ResolvedProduct, folders: ProductFolders) -> PreparedProject:
        preparation = Preparation(self.context, product, folders, AndroidBackend.NAME, AndroidBackend.VERSION, AndroidTemplate.FOLDER)
        sdk = AndroidSdk(self.context.environment).folder
        cmake = BuildTools.CMAKE.require().parent.parent
        # The Android SDK and the CMake of this machine reach the project too, so another one prepares it again.
        inputs = hashlib.sha256(f"{preparation.state.inputs}\0{sdk.as_posix()}\0{cmake.as_posix()}".encode()).hexdigest()
        native = AndroidNativeBuild.of(product)
        extra = {AndroidGradleBuild.LIBRARY: native.library}
        preparation.state = dataclasses.replace(preparation.state, inputs=inputs, extra=MappingProxyType(extra))

        release = None
        if product.configuration is Configuration.RELEASE:
            release = self.release(product)
        elif preparation.is_current():
            return preparation.unchanged(folders.project)

        self.context.terminal.step(f"Generating the Android project `{Repository.shown(folders.project)}`")
        tree = GeneratedTree(folders.project)
        packs = AndroidProject(product, folders, sdk, cmake).write(tree, release)
        extra |= {AndroidGradleBuild.INSTALL_PACKS: ",".join(packs.install_time), AndroidGradleBuild.DELIVERED_PACKS: ",".join(packs.delivered)}
        preparation.state = dataclasses.replace(preparation.state, extra=MappingProxyType(extra))
        return preparation.finish(tree, folders.project)

    def release(self, product: ResolvedProduct) -> AndroidProject.Release:
        """Builds the protected release of a release build with the bootstrap that its library compiles in. A release too large for the base module of Google Play keeps the files of its app domain there."""
        releases = self.releases()
        source = ReleasePackage.of(product)
        release = releases.build(source, AndroidPlatform.RELEASE_PROFILE, product.request.previous_release)
        bootstrap = releases.bootstrap_source(product.app, AndroidPlatform.RELEASE_PROFILE)
        kept: tuple[str, ...] = ()
        if ProtectedRelease.size(release.shipped) > AndroidAssetPacks.BASE_RELEASE_LIMIT:
            kept = releases.app_files(source, release)
            self.context.terminal.info(
                "The protected release is larger than the share of the base module of Google Play that the app leaves it, so its content shards go into install-time asset packs."
            )
        return AndroidProject.Release(release, bootstrap, kept)

    def build(self, prepared: PreparedProject) -> BuildProduct:
        return AndroidGradleBuild(self.context).run(prepared)

    def verify(self, product: BuildProduct) -> VerificationReport:
        return AndroidVerifier(self.context, self.releases).verify(product)

    def devices(self) -> tuple[Device, ...]:
        return AndroidLauncher(self.context).devices()

    def run(self, product: BuildProduct, launch: LaunchRequest) -> None:
        AndroidLauncher(self.context).run(product, launch)

    def package(self, product: BuildProduct, request: PackageRequest) -> tuple[str, ...]:
        """Packages a built app: the format `bundle` copies the bundle of Google Play with its asset packs into the output folder, which the build makes when it has packs that Play delivers and which it builds now otherwise, and the format `apks` builds the APK set of a connected device from the bundle in the local testing mode of Play and installs it, with the option `device` naming the device."""
        if request.format not in (AndroidBackend.BUNDLE, AndroidBackend.APKS):
            raise InvalidOption(f'Android packages apps in the formats "{AndroidBackend.BUNDLE}" and "{AndroidBackend.APKS}", not in "{request.format}".')
        unknown = sorted(set(request.options) - ({"device"} if request.format == AndroidBackend.APKS else set()))
        if unknown:
            raise InvalidOption(f'The format "{request.format}" of Android takes no option "{unknown[0]}".')

        bundle = AndroidGradleBuild.bundle(product.prepared)
        if not AndroidGradleBuild.names(product.prepared, AndroidGradleBuild.DELIVERED_PACKS):
            bundle = AndroidGradleBuild(self.context).bundle_only(product.prepared)
        request.output.mkdir(parents=True, exist_ok=True)
        if request.format == AndroidBackend.BUNDLE:
            copied = request.output / bundle.name
            shutil.copy2(bundle, copied)
            self.context.terminal.success(f"Wrote the bundle `{Repository.shown(copied)}`.")
            return (str(copied),)

        device = request.options.get("device")
        serial = AndroidDebugBridge(self.context, AndroidSdk(self.context.environment)).device(str(device) if device is not None else None)
        apks = AndroidBundleInstall(self.context).install(bundle, product.prepared.product.app.folder, serial, request.output / f"{bundle.stem}.apks")
        return (str(apks),)

    def sign(self, request: SigningRequest) -> tuple[str, ...]:
        return AndroidSigningKey.create(self.context, request.app_folder, request.kind, request.options)
