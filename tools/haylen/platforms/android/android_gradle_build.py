import shutil
from pathlib import Path

from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.android.android_signing_key import AndroidSigningKey
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_component import ProductComponent
from haylen.toolchains.gradle import Gradle


class AndroidGradleBuild:
    """Builds a prepared Android project with the pinned Gradle, whose outputs, project cache and native build trees go to the build folder of the product. A release whose shards went into install-time asset packs builds the universal APK of the bundle, which holds the asset packs that install with the app as Google Play installs them, and with asset packs that Play delivers after the install also the bundle, which holds them. The products folder receives the APK and the bundle under the identifier of the app."""

    # The preparation records the asset packs of the project in its state, by their delivery.
    INSTALL_PACKS = "installTimePacks"
    DELIVERED_PACKS = "deliveredPacks"
    LIBRARY = "library"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    @staticmethod
    def names(prepared: PreparedProject, key: str) -> tuple[str, ...]:
        value = prepared.state.extra.get(key, "")
        return tuple(name for name in value.split(",") if name)

    def gradle(self, prepared: PreparedProject, tasks: list[str]) -> None:
        """Runs tasks of the project with the keys of the app in the environment of Gradle."""
        product = prepared.product
        keys = [AndroidSigningKey.release(product.app.folder, self.context.environment)] if product.configuration is Configuration.RELEASE else []
        debug = AndroidSigningKey.find(product.app.folder, "debug", self.context.environment)
        if debug is not None:
            keys.append(debug)
        environment = dict(self.context.environment)
        for key in keys:
            environment |= key.environment()
        command: list[object] = [Gradle.path(self.context), "-p", prepared.project, *tasks, f"--max-workers={self.context.jobs}", "--project-cache-dir", prepared.folders.build / "gradle-cache"]
        self.context.process.run(command, env=environment)

    def run(self, prepared: PreparedProject) -> BuildProduct:
        product = prepared.product
        variant = product.configuration.value
        packed = bool(AndroidGradleBuild.names(prepared, AndroidGradleBuild.INSTALL_PACKS))
        delivered = bool(AndroidGradleBuild.names(prepared, AndroidGradleBuild.DELIVERED_PACKS))
        tasks = [f":app:package{variant}UniversalApk" if packed else f":app:assemble{variant}"]
        if delivered:
            tasks.append(f":app:bundle{variant}")
        self.context.terminal.step(f'Building the Android app with Gradle in the "{variant}" configuration')
        self.gradle(prepared, tasks)

        outputs = AndroidGradleBuild.outputs(prepared)
        built = AndroidGradleBuild.newest(outputs / ("apk_from_bundle" if packed else "apk") / variant.lower(), "*.apk")
        products = prepared.folders.products
        Folders.remove(products)
        products.mkdir(parents=True)
        apk = products / f"{product.app.identifier}.apk"
        shutil.copy2(built, apk)
        if delivered:
            shutil.copy2(AndroidGradleBuild.newest(outputs / "bundle" / variant.lower(), "*.aab"), AndroidGradleBuild.bundle(prepared))

        components = tuple(ProductComponent(project.target, apk, "abi") for project in product.projects)
        symbols = AndroidGradleBuild.symbols(prepared) if product.configuration is Configuration.RELEASE else ()
        return BuildProduct(prepared, apk, "apk", components, symbols)

    def bundle_only(self, prepared: PreparedProject) -> Path:
        """Builds the bundle of a project whose build made only its APK, since Google Play delivers none of its asset packs after the install, and returns it."""
        variant = prepared.product.configuration.value
        self.context.terminal.step(f'Building the bundle of the Android app with Gradle in the "{variant}" configuration')
        self.gradle(prepared, [f":app:bundle{variant}"])
        bundle = AndroidGradleBuild.bundle(prepared)
        shutil.copy2(AndroidGradleBuild.newest(AndroidGradleBuild.outputs(prepared) / "bundle" / variant.lower(), "*.aab"), bundle)
        return bundle

    @staticmethod
    def outputs(prepared: PreparedProject) -> Path:
        return prepared.folders.build / "gradle" / "app" / "outputs"

    @staticmethod
    def bundle(prepared: PreparedProject) -> Path:
        return prepared.folders.products / f"{prepared.product.app.identifier}.aab"

    @staticmethod
    def newest(folder: Path, pattern: str) -> Path:
        found = sorted(folder.glob(pattern), key=lambda path: path.stat().st_mtime) if folder.is_dir() else []
        if not found:
            raise BuildFailed(f'Gradle built no "{pattern}" into `{Repository.shown(folder)}`, although it succeeded. Build the app again with "--verbose" to see the output of Gradle.')
        return found[-1]

    @staticmethod
    def symbols(prepared: PreparedProject) -> tuple[Path, ...]:
        """The symbol files of the app library of a release for each ABI, which stay in the native build trees for crash reports and never ship."""
        abis = {project.target.abi for project in prepared.product.projects}
        found = (prepared.folders.build / "cxx").glob("**/symbols/*/*.debug")
        return tuple(sorted(path for path in found if abis.intersection(path.parts)))
