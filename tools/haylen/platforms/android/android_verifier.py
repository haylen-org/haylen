from collections.abc import Callable

from haylen.content.protected_release import ProtectedRelease
from haylen.content.protected_releases import ProtectedReleases
from haylen.content.release_entry import ReleaseEntry
from haylen.content.release_package import ReleasePackage
from haylen.core.configuration import Configuration
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.android.android_apk import AndroidApk
from haylen.platforms.android.android_asset_packs import AndroidAssetPacks
from haylen.platforms.android.android_contributions import AndroidContributions
from haylen.platforms.android.android_gradle_build import AndroidGradleBuild
from haylen.platforms.android.android_manifest_file import AndroidManifestFile
from haylen.platforms.android.android_module_manifest import AndroidModuleManifest
from haylen.platforms.android.android_native_build import AndroidNativeBuild
from haylen.platforms.android.android_native_methods import AndroidNativeMethods
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.android.elf_library import ElfLibrary
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.verification_check import VerificationCheck
from haylen.platforms.verification_report import VerificationReport


class AndroidVerifier:
    """Checks a built Android app against what the engine and every plugin need: the library that the activity loads and the native libraries for every ABI of the product, the JNI functions of every native method of the classes of the engine in the library that the activity loads, the merged manifest with the activity of the engine, the provider of plugins and what every plugin module declares, and in a release the protected release that the APK holds, its shards stored uncompressed and the asset packs of the bundle."""

    SOURCE = "android"
    # The JNI entry that loads the runtime, which registers the natives of the data views of Varn.
    LOAD_ENTRY = "JNI_OnLoad"
    LISTED_NATIVES = 3
    PLUGIN_CLASS_PREFIX = "dev.haylen.plugin."

    def __init__(self, context: ToolingContext, releases: Callable[[], ProtectedReleases]) -> None:
        self.context = context
        self.releases = releases

    def verify(self, product: BuildProduct) -> VerificationReport:
        resolved = product.prepared.product
        apk = AndroidApk(product.path)
        entries = apk.entries()
        checks = [*self.libraries(product, entries), *self.natives(product, apk, entries), *self.manifest(product, apk)]
        if resolved.configuration is Configuration.RELEASE:
            checks += self.release(product, apk, entries)
        return VerificationReport(tuple(checks))

    def libraries(self, product: BuildProduct, entries: set[str]) -> list[VerificationCheck]:
        resolved = product.prepared.product
        library = product.prepared.state.extra.get(AndroidGradleBuild.LIBRARY, AndroidNativeBuild.PLAYER_LIBRARY)
        names = [library, *(native.name for native in resolved.primary.native_libraries)]
        missing = [f"lib/{project.target.abi}/lib{name}.so" for project in resolved.projects for name in names if f"lib/{project.target.abi}/lib{name}.so" not in entries]
        if missing:
            listed = ", ".join(f'"{entry}"' for entry in missing)
            return [VerificationCheck.failed("libraries", f"The APK lacks the native libraries {listed}, which the activity and Lua load.", AndroidVerifier.SOURCE)]
        abis = ", ".join(f'"{project.target.abi}"' for project in resolved.projects)
        return [VerificationCheck.passed("libraries", f'The APK holds "lib{library}.so" and the native libraries of the app for {abis}.', AndroidVerifier.SOURCE)]

    def natives(self, product: BuildProduct, apk: AndroidApk, entries: set[str]) -> list[VerificationCheck]:
        """Checks that the library that the activity loads defines, for every ABI, `JNI_OnLoad` and the JNI function of every native method that the classes of the engine in the Android libraries of the SDK declare, since Java looks a native up only when it first calls it, and a missing one crashes the app then."""
        resolved = product.prepared.product
        library = product.prepared.state.extra.get(AndroidGradleBuild.LIBRARY, AndroidNativeBuild.PLAYER_LIBRARY)
        sdk = resolved.primary.sdk.manifest
        methods = AndroidNativeMethods.of_sdk(sdk.file(AndroidPlatform.MAVEN_FILE), sdk.engine_version)

        checks = []
        for project in resolved.projects:
            entry = f"lib/{project.target.abi}/lib{library}.so"
            if entry not in entries:
                continue
            symbols = ElfLibrary.defined_symbols(apk.read(entry), f'"{entry}" of `{Repository.shown(product.path)}`')
            missing = [native.shown for native in methods.missing(symbols)]
            if AndroidVerifier.LOAD_ENTRY not in symbols:
                missing.insert(0, AndroidVerifier.LOAD_ENTRY)
            if missing:
                listed = ", ".join(f'"{name}"' for name in missing[: AndroidVerifier.LISTED_NATIVES])
                detail = f'The library "{entry}" lacks the JNI functions of {len(missing)} native methods of the engine, such as {listed}, so the app crashes as soon as Java calls one. Build the app again, and the engine SDK with "haylen sdk build --target android" when the library still lacks them.'
                checks.append(VerificationCheck.failed("natives", detail, AndroidVerifier.SOURCE))
        if not checks:
            abis = ", ".join(f'"{project.target.abi}"' for project in resolved.projects)
            checks.append(VerificationCheck.passed("natives", f'The library "lib{library}.so" defines the JNI functions of every native method of the engine for {abis}.', AndroidVerifier.SOURCE))
        return checks

    def manifest(self, product: BuildProduct, apk: AndroidApk) -> list[VerificationCheck]:
        resolved = product.prepared.product
        merged = apk.manifest(self.context)
        removed = AndroidManifestFile.removed_permissions(resolved)
        needs = [("Haylen", "activity", "dev.haylen.HaylenActivity")]
        # The provider loads the plugin classes that meta-data name, so a module without one, such as a manifest that only adds an intent filter, needs no provider.
        classes = sorted(name for name in merged.get("meta-data", set()) if name.startswith(AndroidVerifier.PLUGIN_CLASS_PREFIX))
        if classes:
            listed = ", ".join(f'"{name}"' for name in classes)
            needs.append((f"Loading the plugin classes of the meta-data {listed}", "provider", "dev.haylen.HaylenPluginProvider"))
        for contribution in resolved.primary.of_type(AndroidContributions.GRADLE_MODULE):
            module = contribution.folders[0] / str(contribution.field("path"))
            needs += [(f'The plugin "{contribution.owners[0]}"', tag, name) for tag, name in AndroidModuleManifest.needs(module)]

        checks = []
        for owner, tag, name in needs:
            if name in merged.get(tag, set()) or (tag == "activity" and name in merged.get("activity-alias", set())):
                continue
            if tag == "uses-permission" and name in removed:
                detail = f'{owner} needs the permission "{name}", which "platforms.android.manifest.removedPermissions" of "haylen.json" removes, so the calls that need it answer "unsupported". Keep the permission to use them.'
                checks.append(VerificationCheck.warning("manifest", detail, AndroidVerifier.SOURCE))
                continue
            checks.append(VerificationCheck.failed("manifest", f'{owner} needs the {tag} "{name}", which the merged manifest of the built app lacks.', AndroidVerifier.SOURCE))
        if not any(check.outcome is VerificationCheck.Outcome.FAILED for check in checks):
            checks.append(VerificationCheck.passed("manifest", "The merged manifest holds what the engine and every plugin need.", AndroidVerifier.SOURCE))
        return checks

    def release(self, product: BuildProduct, apk: AndroidApk, entries: set[str]) -> list[VerificationCheck]:
        """Inspects the protected release that the APK holds, and checks that the APK and the bundle hold every shard of the asset packs of the project."""
        resolved = product.prepared.product
        source = ReleasePackage.of(resolved)
        checks = list(self.releases().inspection(source).checks(ReleaseEntry.of_archive(product.path), "assets/app"))
        compressed = apk.compressed(ProtectedRelease.SHARD_SUFFIX)
        if compressed:
            checks.append(VerificationCheck.failed("shards", f'The APK holds the shard "{compressed[0]}" compressed, while the runtime reads shards in place.', AndroidVerifier.SOURCE))

        project = product.prepared.project
        shipped = [
            project / AndroidNativeBuild.PACKAGE,
            *(project / AndroidAssetPacks.FOLDER / name / "src" / "main" / "assets" / "app" for name in AndroidGradleBuild.names(product.prepared, AndroidGradleBuild.INSTALL_PACKS)),
        ]
        shards = sorted(path.name for folder in shipped if folder.is_dir() for path in folder.iterdir() if path.name.endswith(ProtectedRelease.SHARD_SUFFIX))
        lacking = [name for name in shards if f"assets/app/{name}" not in entries]
        if lacking:
            checks.append(VerificationCheck.failed("shards", f'The APK lacks the shard "{lacking[0]}" of its protected release.', AndroidVerifier.SOURCE))
        elif not compressed:
            checks.append(VerificationCheck.passed("shards", f"The APK holds the {len(shards)} shards of its protected release uncompressed.", AndroidVerifier.SOURCE))

        delivered = AndroidGradleBuild.names(product.prepared, AndroidGradleBuild.DELIVERED_PACKS)
        if delivered:
            bundle = AndroidGradleBuild.bundle(product.prepared)
            bundled = AndroidApk(bundle).entries() if bundle.is_file() else set()
            lacking = [f"{pack}/assets/{path.name}" for pack in delivered for path in sorted((project / AndroidAssetPacks.FOLDER / pack / "src" / "main" / "assets").iterdir())]
            lacking = [entry for entry in lacking if entry not in bundled]
            if lacking:
                checks.append(VerificationCheck.failed("bundle", f'The bundle `{Repository.shown(bundle)}` lacks "{lacking[0]}" of the asset packs that Google Play delivers.', AndroidVerifier.SOURCE))
            else:
                checks.append(VerificationCheck.passed("bundle", f"The bundle holds the asset packs {', '.join(f'"{pack}"' for pack in delivered)}.", AndroidVerifier.SOURCE))
        return checks
