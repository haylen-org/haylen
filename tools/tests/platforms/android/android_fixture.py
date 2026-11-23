import json
from pathlib import Path

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.product_request import ProductRequest
from haylen.core.project_resolver import ProjectResolver
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.android.android_backend import AndroidBackend
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.android.android_port import AndroidPort
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_folders import ProductFolders
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_resolver import PluginResolver
from haylen.plugins.plugin_source import PluginSource
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_selector import SdkSelector
from haylen.toolchains.android_ndk import AndroidNdk
from tests.support.native_binaries import NativeBinaries
from tests.support.tooling_fixture import ToolingFixture


class AndroidFixture:
    """Resolves and prepares Android apps of the tests in a temporary folder, with the Android backend, an Android SDK folder of its own and an installed engine SDK of every ABI whose files the generated projects name without reading them, besides its Android libraries, whose activity declares one native method of the engine next to a native method of Varn."""

    ENGINE_VERSION = "0.0.1"
    # The JNI names that a library of an app defines for the Android libraries of the fixture.
    NATIVES = ("JNI_OnLoad", "Java_dev_haylen_HaylenActivity_nativeTelevision")

    def __init__(self, folder: Path) -> None:
        self.tooling = ToolingFixture(folder)
        base = self.tooling.context
        (folder / "android-sdk").mkdir(parents=True)
        self.context = ToolingContext(base.terminal, base.process, 1, {"ANDROID_HOME": str(folder / "android-sdk"), "HOME": str(folder / "home")})
        self.backend = AndroidBackend(self.context)
        self.registry = BackendRegistry(self.context, [("android", lambda: AndroidBackend)], [])
        self.sdk = self.engine_sdk(folder / "sdk")

    @staticmethod
    def engine_sdk(folder: Path) -> SdkManifest:
        slices = [
            {
                "system": "android",
                "environment": "device",
                "architectures": [abi],
                "abi": abi,
                "toolchain": f"ndk-{AndroidNdk.VERSION}",
                "runtime": "c++_static",
                "minimumSystemVersion": "27",
                "graphicsApis": ["vulkan", "openGlEs"],
                "capabilities": ["dynamicLibraries", "executableMemory", "processes"],
                "libraries": {name: f"abis/{abi}/lib/{file}" for name, file in AndroidPort.SDK_LIBRARIES.items()},
                "files": {"cmake": f"abis/{abi}/lib/cmake/haylen", "player": f"jniLibs/{abi}/libhaylen.so"},
            }
            for abi in AndroidNdk.ABIS
        ]
        document = {
            "format": 1,
            "port": "android",
            "portVersion": "1.0.0",
            "engineVersion": AndroidFixture.ENGINE_VERSION,
            "revision": ToolingFixture.REVISION,
            "configuration": "Release",
            "slices": slices,
            "files": {"maven": "maven"},
        }
        folder.mkdir(parents=True)
        (folder / SdkManifest.FILE).write_text(json.dumps(document), encoding="utf-8")
        activity = NativeBinaries.class_file("dev/haylen/HaylenActivity", [("nativeTelevision", "(Z)V", True), ("onCreate", "(Landroid/os/Bundle;)V", False)])
        view = NativeBinaries.class_file("com/varn/VarnDataView", [("nativeAllocate", "(J)J", True)])
        for library in AndroidPlatform.LIBRARIES:
            classes = {"dev/haylen/HaylenActivity": activity, "com/varn/VarnDataView": view} if library == "haylen" else {}
            NativeBinaries.library(folder / "maven" / "dev" / "haylen" / library / AndroidFixture.ENGINE_VERSION / f"{library}-{AndroidFixture.ENGINE_VERSION}.aar", classes)
        return SdkManifest.load(folder)

    def app(self, document: dict, name: str = "app") -> Path:
        return self.tooling.app(document, name)

    def plugin(self, app: Path, identifier: str, document: dict) -> Path:
        """A plugin with a native Android part: its library module and the contribution that brings it, besides the contributions of `document`."""
        folder = self.tooling.plugin(app, identifier, {"implementations": {"android": "native"}, **document})
        module = folder / "android"
        (module / "src" / "main").mkdir(parents=True)
        (module / "build.gradle.kts").write_text(f'android {{\n    namespace = "com.haylen.plugins.{identifier}"\n}}\n', encoding="utf-8")
        (module / "src" / "main" / "AndroidManifest.xml").write_text("<manifest />\n", encoding="utf-8")
        manifest = json.loads((folder / "plugin.json").read_text(encoding="utf-8"))
        manifest["contributions"] = [{"type": "gradleModule", "platforms": ["android"], "path": "android"}, *manifest.get("contributions", [])]
        (folder / "plugin.json").write_text(json.dumps(manifest, indent=4), encoding="utf-8")
        return folder

    def resolve(self, app: Path, configuration: str = "Debug", architectures: tuple[str, ...] | None = None) -> ResolvedProduct:
        manifest = AppManifest.load(app)
        source = PluginSource(manifest.folder, PluginLock.load(manifest.folder), self.context.process, self.context.terminal)
        resolver = ProjectResolver(self.registry, PluginResolver(manifest, source), SdkSelector((self.sdk,), ToolingFixture.REVISION))
        return resolver.resolve(ProductRequest(app, "android", Configuration(configuration), Configuration.RELEASE, architectures))

    def prepare(self, product: ResolvedProduct, name: str = "product") -> PreparedProject:
        return self.backend.prepare(product, ProductFolders(self.tooling.folder / name))

    @staticmethod
    def files(project: Path) -> dict[str, bytes]:
        """Every file of a generated project by its path, without the record of the preparation."""
        return {path.relative_to(project).as_posix(): path.read_bytes() for path in sorted(project.rglob("*")) if path.is_file() and path.name != "haylen-preparation.json"}
