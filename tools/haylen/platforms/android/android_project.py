import json
import re
from dataclasses import dataclass
from pathlib import Path

from haylen.content.protected_release import ProtectedRelease
from haylen.content.protected_releases import ProtectedReleases
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.frozen import Frozen
from haylen.core.product_inputs import ProductInputs
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.native.cmake_tree import CMakeTree
from haylen.platforms.android.android_asset_packs import AndroidAssetPacks
from haylen.platforms.android.android_contributions import AndroidContributions
from haylen.platforms.android.android_manifest_file import AndroidManifestFile
from haylen.platforms.android.android_native_build import AndroidNativeBuild
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.android.android_plugin_parts import AndroidPluginParts
from haylen.platforms.android.android_splash import AndroidSplash
from haylen.platforms.android.android_template import AndroidTemplate
from haylen.platforms.android.java_properties import JavaProperties
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.product_folders import ProductFolders
from haylen.toolchains.android_ndk import AndroidNdk
from haylen.toolchains.gradle import Gradle


class AndroidProject:
    """The Gradle project of an Android app, which a preparation generates whole from the template, `haylen.json`, the plugins and the engine SDK, into the folder `project` of the product. The developer customizes it only through `platforms.android` of `haylen.json` and the files of the app that its `files` and `sources` name, so the project is disposable and the same inputs give the same files. The build outputs of Gradle and of its native build go to the folder `build` of the product, outside the project."""

    # The application id of Android, which also names the package of the app, takes letters, digits and underscores in each part.
    APPLICATION_ID = re.compile(r"[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+")
    PACKAGE_INDEX = "haylen-package-index.json"
    PLAYBACK_LIBRARY = "haylen-playback"
    LOCAL_PROPERTIES = "local.properties"

    @dataclass(frozen=True)
    class Release:
        """The protected release that a release build ships, with the bootstrap that its library compiles in and the files of its app domain, which stay in the base module."""

        release: ProtectedRelease
        bootstrap: bytes
        kept: tuple[str, ...]

    def __init__(self, product: ResolvedProduct, folders: ProductFolders, sdk: Path, cmake: Path) -> None:
        self.product = product
        self.folders = folders
        self.sdk = sdk
        self.cmake = cmake
        self.native = AndroidNativeBuild.of(product)
        self.plugins = AndroidPluginParts.of(product)

    def write(self, tree: GeneratedTree, release: AndroidProject.Release | None) -> AndroidAssetPacks:
        """Writes every file of the project into the tree and returns the asset packs it holds."""
        AndroidProject.check_identifier(self.product)
        files: dict[str, bytes | Path] = {}
        if release is None:
            packs = AndroidAssetPacks({}, {})
            package = self.product.package.files()
            files |= {f"{AndroidNativeBuild.PACKAGE}/{path}": source for path, source in package.items()}
            # Android cannot list asset folders recursively, so the runtime reads the files of the package from this index.
            files[f"{AndroidNativeBuild.PACKAGE}/{AndroidProject.PACKAGE_INDEX}"] = json.dumps(sorted(package)).encode()
        else:
            content_packs = tuple(sorted({str(rule["pack"]) for rule in self.product.app.section("content").get("delivery", ()) if "pack" in rule}))
            packs = AndroidAssetPacks.plan(release.release.shipped, release.kept, release.release.packs, self.plugins.pack_delivery, content_packs)
            files |= packs.files
            files[AndroidNativeBuild.BOOTSTRAP] = release.bootstrap

        for relative in AndroidTemplate.project_files():
            files[relative] = self.render(relative, packs).encode()
        files[AndroidProject.LOCAL_PROPERTIES] = self.local_properties().encode()
        files |= AndroidSplash.files(self.product.app)
        if self.native.targets:
            files[AndroidNativeBuild.CMAKE_LISTS] = self.native.cmake_lists(release is not None).encode()
        files |= self.native.prebuilt_files()
        for destination, source in self.plugins.files().items():
            if destination in files:
                raise InvalidContribution(f'A plugin places "{destination}" in the Android project, which the project generates itself. Place the file at another path.')
            files[destination] = source
        files |= self.overrides()

        for relative, content in sorted(files.items()):
            if relative == AndroidNativeBuild.BOOTSTRAP:
                tree.write_bytes(relative, bytes(content), ProtectedReleases.BOOTSTRAP_MODE)
            elif isinstance(content, Path):
                tree.copy(relative, content)
            else:
                tree.write_bytes(relative, content)
        return packs

    @staticmethod
    def check_identifier(product: ResolvedProduct) -> None:
        app = product.app
        if AndroidProject.APPLICATION_ID.fullmatch(app.identifier) is None:
            raise UnsupportedTarget(
                f'The identifier "{app.identifier}" of `{app.origin}` is no application id of Android, whose parts take letters, digits and underscores and start with a letter. Use an identifier such as "com.example.game".',
                origin=app.origin,
            )

    @property
    def minimum_sdk(self) -> int:
        value = str(self.product.primary.minimum_system_version.value)
        if not value.isdigit():
            raise InvalidManifest(
                f'The oldest Android of the app is "{value}", while Android names it by an API level, such as "27". It comes from {self.product.primary.minimum_system_version.origin}.'
            )
        return int(value)

    @property
    def maven(self) -> Path:
        """The local Maven repository of the Android libraries of the engine, which every ABI of the product takes from one SDK, since the `haylen` library carries the player of every ABI."""
        folders = {project.sdk.manifest.folder for project in self.product.projects}
        if len(folders) > 1:
            names = ", ".join(f"`{Repository.shown(folder)}`" for folder in sorted(folders))
            raise IncompatibleSdk(
                f'The ABIs of the app come from the SDKs {names}, while an Android app takes every ABI from one SDK. Build the ABIs into one SDK with "haylen sdk build --target android".'
            )
        return self.product.primary.sdk.manifest.file(AndroidPlatform.MAVEN_FILE)

    def render(self, relative: str, packs: AndroidAssetPacks) -> str:
        if relative == AndroidManifestFile.PATH:
            return AndroidManifestFile.render(self.product, self.native.library)
        return AndroidTemplate.render(AndroidTemplate.PROJECT / relative, self.tokens(packs))

    def tokens(self, packs: AndroidAssetPacks) -> dict[str, str]:
        app = self.product.app
        kotlin = AndroidTemplate.kotlin
        repositories = "\n".join(f"        maven {{ url = uri({kotlin(url)}) }}" for url in self.plugins.repositories)
        modules = [(identifier, f"{AndroidPluginParts.MODULES}/{identifier}") for identifier, _ in self.plugins.modules]
        modules += [(name, f"{AndroidAssetPacks.FOLDER}/{name}") for name in packs.deliveries]
        included = "\n".join(f"include({kotlin(':' + name)})\nproject({kotlin(':' + name)}).projectDir = file({kotlin(folder)})" for name, folder in modules)
        return {
            "ENGINE_REPOSITORY": kotlin(self.maven.as_posix()),
            "ENGINE_VERSION": kotlin(self.product.primary.sdk.manifest.engine_version),
            "BUILD_DIRECTORY": kotlin((self.folders.build / "gradle").as_posix()),
            "REPOSITORIES": repositories,
            "MODULES": included,
            "GRADLE_PLUGINS": "\n".join(f"        classpath({kotlin(f'{identifier}:{identifier}.gradle.plugin:{version}')})" for identifier, version in self.plugins.gradle_plugins),
            "KOTLIN_DIRECTORY": JavaProperties.escape((self.folders.build / "kotlin").as_posix()),
            "GRADLE_VERSION": Gradle.VERSION,
            "GRADLE_SHA256": Gradle.SHA256,
            "NAMESPACE": kotlin(app.identifier),
            "NDK_VERSION": kotlin(AndroidNdk.VERSION),
            "IDENTIFIER": kotlin(app.identifier),
            "MINIMUM_SDK": str(self.minimum_sdk),
            "VERSION_CODE": str(app.build),
            "VERSION_NAME": kotlin(app.version),
            "PLACEHOLDERS": self.placeholders(),
            "ABIS": ", ".join(kotlin(abi) for abi in self.native.abis),
            "NATIVE_ARGUMENTS": self.native_arguments(),
            "SOURCE_SETS": self.source_sets(),
            "NATIVE_BUILD": self.native_build(),
            "ASSET_PACKS": AndroidProject.asset_packs(packs),
            "PACKAGING": self.packaging(),
            "DEPENDENCIES": self.dependencies(),
            "APPLIED_PLUGINS": "".join(f"\napply(plugin = {kotlin(identifier)})" for identifier, _ in self.plugins.gradle_plugins),
        }

    def placeholders(self) -> str:
        if not self.plugins.placeholders:
            return ""
        entries = [f"            {AndroidTemplate.kotlin(name)} to {AndroidTemplate.kotlin(value)}," for name, value in self.plugins.placeholders]
        lines = ["        // The manifests of the plugin modules read their values from placeholders.", "        manifestPlaceholders += mapOf(", *entries, "        )"]
        return "\n".join(lines)

    def source_sets(self) -> str:
        folders = self.source_folders()
        if not folders:
            return ""
        added = [f"        {kind}.directories.add({AndroidTemplate.kotlin(folder.as_posix())})" for folder in folders for kind in ("java", "kotlin")]
        comment = "    // The Kotlin and Java folders of the app compile into the app module where they lie, the Java sources with the compiler of Java and the Kotlin sources, which see them, with the compiler of Kotlin."
        lines = ["", comment, '    sourceSets.getByName("main") {', *added, "    }"]
        return "\n".join(lines)

    @staticmethod
    def asset_packs(packs: AndroidAssetPacks) -> str:
        if not packs.deliveries:
            return ""
        names = ", ".join(AndroidTemplate.kotlin(":" + name) for name in packs.deliveries)
        lines = ["", "    // The asset packs of the protected release, which install with the app or arrive from Google Play after the install.", f"    assetPacks += listOf({names})"]
        return "\n".join(lines)

    def native_arguments(self) -> str:
        if not self.native.targets:
            return ""
        targets = ", ".join(AndroidTemplate.kotlin(target) for target in self.native.targets)
        options = ", ".join(AndroidTemplate.kotlin(option) for option in ("-DANDROID_STL=c++_static", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON", CMakeTree.WITHOUT_MODULE_SCAN))
        lines = [
            "        externalNativeBuild {",
            "            cmake {",
            f"                arguments += listOf({options})",
            f"                targets += listOf({targets})",
            "            }",
            "        }",
        ]
        return "\n".join(lines)

    def native_build(self) -> str:
        if not self.native.targets:
            return ""
        staging = AndroidTemplate.kotlin((self.folders.build / "cxx").as_posix())
        lines = [
            "",
            "    // The native code of the app compiles against the engine SDK of each ABI, with its build trees in the build folder of the app.",
            "    externalNativeBuild {",
            "        cmake {",
            f'            path = file("../{AndroidNativeBuild.CMAKE_LISTS}")',
            f"            buildStagingDirectory = file({staging})",
            "        }",
            "    }",
        ]
        return "\n".join(lines)

    def packaging(self) -> str:
        if not self.native.compiles_app:
            return ""
        lines = [
            "",
            "    // The library of the app brings the engine itself, so the Lua player of the `haylen` library stays out of the APK.",
            "    packaging {",
            "        jniLibs {",
            '            excludes += "lib/*/libhaylen.so"',
            "        }",
            "    }",
        ]
        return "\n".join(lines)

    def dependencies(self) -> str:
        version = self.product.primary.sdk.manifest.engine_version
        libraries = [AndroidNativeBuild.PLAYER_LIBRARY]
        if self.product.app.section("audio").get("background") is True:
            libraries.append(AndroidProject.PLAYBACK_LIBRARY)
        lines = [f'    implementation("dev.haylen:{library}:{version}")' for library in libraries]
        lines += [f"    implementation(project({AndroidTemplate.kotlin(':' + identifier)}))" for identifier, _ in self.plugins.modules]
        return "\n".join(lines)

    def source_folders(self) -> list[Path]:
        """The Kotlin and Java folders of the app that compile into the app module, such as the one of its application class, where they lie, so the build always takes their current files."""
        app = self.product.app
        folders = [app.folder / str(folder) for folder in self.product.primary.setting(AndroidContributions.SECTION, "sources", ())]
        for folder in folders:
            if not folder.is_dir():
                raise InvalidManifest(f'The key "platforms.android.sources" of `{app.origin}` names `{Repository.shown(folder)}`, which is no folder of the app.', origin=app.origin)
        return folders

    def local_properties(self) -> str:
        """The Android SDK and the CMake of this machine, which the native build of Gradle takes instead of an older CMake of the Android SDK."""
        return "\n".join([JavaProperties.line("sdk.dir", self.sdk.as_posix()), JavaProperties.line("cmake.dir", self.cmake.as_posix()), ""])

    def overrides(self) -> dict[str, Path]:
        """The files of the app that `platforms.android.files` of `haylen.json` names, which replace the files of the template and of the preparation at their paths."""
        files: dict[str, Path] = {}
        named = Frozen.thaw(self.product.primary.setting(AndroidContributions.SECTION, "files", {}))
        for destination, source in named.items():
            path = self.product.app.folder / source
            if path.is_file():
                files[destination] = path
                continue
            for found in sorted(ProductInputs.folder("", path).values()):
                files[f"{destination}/{found.relative_to(path).as_posix()}"] = found
        return files
