import io
import json
from pathlib import Path

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.product_request import ProductRequest
from haylen.core.project_resolver import ProjectResolver
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.backend_registry import BackendRegistry
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_resolver import PluginResolver
from haylen.plugins.plugin_source import PluginSource
from haylen.process import Process
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_selector import SdkSelector
from haylen.terminal import Terminal
from tests.support.fake_backend import FakeBackend


class ToolingFixture:
    """Builds what the tests of the core need in a temporary folder: apps with their `haylen.json`, plugins with their `plugin.json`, installed SDK manifests and a registry with the fake backend, and resolves products through the same pipeline as the commands."""

    REVISION = "a" * 64

    def __init__(self, folder: Path) -> None:
        self.folder = folder
        self.output = io.StringIO()
        terminal = Terminal(self.output, self.output, {}, folder / "logs" / "test.log")
        self.context = ToolingContext(terminal, Process(terminal), 1, {})
        self.sdks: list[SdkManifest] = []

    def registry(self, backends: list[type] | None = None) -> BackendRegistry:
        classes = backends or [FakeBackend]
        return BackendRegistry(self.context, [(cls.__name__, lambda cls=cls: cls) for cls in classes], [])

    def app(self, document: dict, name: str = "app") -> Path:
        folder = self.folder / name
        (folder / "content").mkdir(parents=True, exist_ok=True)
        # Only an app that runs Lua has Lua modules.
        if document.get("kind") != "cpp" or document.get("cpp", {}).get("lua", False):
            (folder / "source").mkdir(exist_ok=True)
            (folder / "source" / "main.lua").write_text("print('hello')\n", encoding="utf-8")
        # A key whose value is None stays out of the manifest.
        defaults = {"name": "Test App", "identifier": "com.haylen.tests", "version": "1.0.0", **document}
        manifest = {key: value for key, value in defaults.items() if value is not None}
        (folder / AppManifest.FILE).write_text(json.dumps(manifest, indent=4), encoding="utf-8")
        return folder

    def plugin(self, app: Path, identifier: str, document: dict) -> Path:
        folder = app / "plugins" / identifier
        (folder / "source").mkdir(parents=True, exist_ok=True)
        (folder / "source" / "init.lua").write_text("return {}\n", encoding="utf-8")
        manifest = {
            "interface": 1,
            "id": identifier,
            "name": identifier.title(),
            "version": "1.0.0",
            "description": "A plugin of the tests.",
            "implementations": {"fake": "lua"},
            **document,
        }
        (folder / "plugin.json").write_text(json.dumps(manifest, indent=4), encoding="utf-8")
        return folder

    def sdk(
        self,
        configuration: str = "Release",
        revision: str = REVISION,
        minimum: str = "1.0",
        architectures: tuple[str, ...] = ("arm64", "x86_64"),
        capabilities: tuple[str, ...] = ("dynamicLibraries",),
    ) -> SdkManifest:
        folder = self.folder / "sdk" / f"sdk-{len(self.sdks)}"
        slices = [
            {
                "system": "fakeos",
                "environment": "device",
                "architectures": [architecture],
                "abi": f"fake-{architecture}",
                "toolchain": "fakecc",
                "runtime": "libfake",
                "minimumSystemVersion": minimum,
                "graphicsApis": ["fakeGl"],
                "capabilities": list(capabilities),
                "libraries": {"haylen": "lib/libhaylen.a", "haylen-runtime": "lib/libhaylen-runtime.a", "haylen-lua": "lib/libhaylen-lua.a", "haylen-player": "lib/haylen-player.o"},
            }
            for architecture in architectures
        ]
        document = {
            "format": 1,
            "port": "fake",
            "portVersion": "1.0.0",
            "engineVersion": "0.0.1",
            "revision": revision,
            "configuration": configuration,
            "slices": slices,
        }
        folder.mkdir(parents=True)
        (folder / SdkManifest.FILE).write_text(json.dumps(document), encoding="utf-8")
        manifest = SdkManifest.load(folder)
        self.sdks.append(manifest)
        return manifest

    def unusable_sdk(self, name: str, system: str = "fakeos", damaged: bool = False) -> Path:
        """Installs an SDK that this tooling cannot use: one whose slice lacks the libraries, as an older version of Haylen wrote it, or a damaged file."""
        folder = self.folder / "sdk" / name
        folder.mkdir(parents=True)
        slices = [
            {
                "system": system,
                "environment": "device",
                "architectures": ["arm64"],
                "abi": "fake-arm64",
                "toolchain": "fakecc",
                "runtime": "libfake",
                "minimumSystemVersion": "1.0",
                "graphicsApis": [],
                "capabilities": [],
            }
        ]
        document = {"format": 1, "port": "fake", "portVersion": "1.0.0", "engineVersion": "0.0.1", "revision": "c" * 64, "configuration": "Release", "slices": slices}
        text = '{"format": 1, "port":' if damaged else json.dumps(document)
        (folder / SdkManifest.FILE).write_text(text, encoding="utf-8")
        return folder

    def resolver(self, app: Path, registry: BackendRegistry | None = None) -> ProjectResolver:
        manifest = AppManifest.load(app)
        source = PluginSource(manifest.folder, PluginLock.load(manifest.folder), self.context.process, self.context.terminal)
        return ProjectResolver(registry or self.registry(), PluginResolver(manifest, source), SdkSelector(tuple(self.sdks), ToolingFixture.REVISION))

    def resolve(self, app: Path, target: str = "fake", configuration: str = "Debug", architectures: tuple[str, ...] | None = None) -> ResolvedProduct:
        request = ProductRequest(app, target, Configuration(configuration), Configuration.RELEASE, architectures)
        return self.resolver(app).resolve(request)
