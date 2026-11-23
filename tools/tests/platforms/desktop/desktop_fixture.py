import json
from pathlib import Path

import pytest

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.product_request import ProductRequest
from haylen.core.project_resolver import ProjectResolver
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.desktop.desktop_backend import DesktopBackend
from haylen.platforms.desktop.desktop_system import DesktopSystem
from haylen.platforms.player.player_backend import PlayerBackend
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.web.web_backend import WebBackend
from haylen.platforms.web.web_port import WebPort
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_resolver import PluginResolver
from haylen.plugins.plugin_source import PluginSource
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_selector import SdkSelector
from tests.support.tooling_fixture import ToolingFixture


class DesktopFixture:
    """Apps, plugins and installed SDKs in a temporary folder for the tests of the desktop, player and web backends: a desktop SDK with a slice of every desktop system and a web SDK, whose players are files of the tests, and a registry with the three backends. The build folder of the apps moves into the temporary folder, so no test writes into the build folder of the checkout."""

    BACKENDS = (DesktopBackend, PlayerBackend, WebBackend)

    def __init__(self, folder: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        monkeypatch.setattr(Repository, "APPS", folder / "apps")
        self.folder = folder
        self.tooling = ToolingFixture(folder)
        self.context = self.tooling.context
        self.registry = BackendRegistry(self.context, [(backend.NAME, lambda backend=backend: backend) for backend in DesktopFixture.BACKENDS], [])
        self.sdks = (self.desktop_sdk(), self.web_sdk())

    def desktop_sdk(self) -> SdkManifest:
        folder = self.folder / "sdk" / "desktop"
        slices = []
        for system in DesktopSystem.all():
            for architecture, abi in system.abis.items():
                name = f"{system.name}-{architecture}"
                (folder / name / "lib" / "cmake" / "haylen").mkdir(parents=True)
                (folder / name / "bin").mkdir()
                (folder / name / "bin" / "haylen").write_text("player", encoding="utf-8")
                slices.append(
                    {
                        "system": system.name,
                        "environment": "host",
                        "architectures": [architecture],
                        "abi": abi,
                        "toolchain": system.toolchain,
                        "runtime": system.runtime,
                        "minimumSystemVersion": "1.0",
                        "graphicsApis": list(system.graphics_apis),
                        "capabilities": sorted(DesktopSystem.CAPABILITIES),
                        "libraries": {
                            "haylen": f"{name}/lib/libhaylen.a",
                            "haylen-runtime": f"{name}/lib/libhaylen-runtime.a",
                            "haylen-lua": f"{name}/lib/libhaylen-lua.a",
                            "haylen-player": f"{name}/lib/haylen-player.o",
                        },
                        "files": {"cmake": f"{name}/lib/cmake/haylen", "player": f"{name}/bin/haylen"},
                    }
                )
        return DesktopFixture.manifest(folder, "desktop", slices)

    def web_sdk(self) -> SdkManifest:
        folder = self.folder / "sdk" / "web"
        player = folder / WebPort.SLICE / "player"
        player.mkdir(parents=True)
        for name in WebPort.PLAYER_FILES:
            (player / name).write_text(f"// {name}\n", encoding="utf-8")
        platform = folder / WebPort.SLICE / "share" / "haylen" / "platform" / "web"
        platform.mkdir(parents=True)
        (platform / "haylen-logo.svg").write_text("<svg/>", encoding="utf-8")
        (folder / WebPort.SLICE / "lib" / "cmake" / "haylen").mkdir(parents=True)
        slices = [
            {
                "system": WebPort.SYSTEM,
                "environment": "device",
                "architectures": [WebPort.ARCHITECTURE],
                "abi": WebPort.ABIS[WebPort.ARCHITECTURE],
                "toolchain": WebPort.TOOLCHAIN,
                "runtime": WebPort.RUNTIME,
                "minimumSystemVersion": WebPort.MINIMUM,
                "graphicsApis": list(WebPort.GRAPHICS_APIS),
                "capabilities": [],
                "libraries": {"haylen": "lib/libhaylen.a", "haylen-runtime": "lib/libhaylen-runtime.a", "haylen-lua": "lib/libhaylen-lua.a", "haylen-player": "lib/haylen-player.o"},
                "files": {
                    "cmake": f"{WebPort.SLICE}/lib/cmake/haylen",
                    "player": f"{WebPort.SLICE}/player",
                    "platform": f"{WebPort.SLICE}/share/haylen/platform/web",
                },
            }
        ]
        return DesktopFixture.manifest(folder, "web", slices)

    @staticmethod
    def manifest(folder: Path, port: str, slices: list[dict]) -> SdkManifest:
        document = {"format": 1, "port": port, "portVersion": "1.0.0", "engineVersion": "0.0.1", "revision": ToolingFixture.REVISION, "configuration": "Release", "slices": slices}
        (folder / SdkManifest.FILE).write_text(json.dumps(document), encoding="utf-8")
        return SdkManifest.load(folder)

    def app(self, document: dict | None = None, name: str = "app") -> Path:
        return self.tooling.app(document or {}, name).resolve()

    def cpp_app(self, document: dict | None = None, name: str = "cpp-app") -> Path:
        folder = self.tooling.app({"kind": "cpp", "cpp": {"sources": ["src"]}, **(document or {})}, name).resolve()
        (folder / "src").mkdir()
        (folder / "src" / "App.cpp").write_text("// The app.\n", encoding="utf-8")
        return folder

    def plugin(self, app: Path, identifier: str, implementations: dict[str, str], contributions: list[dict], files: dict[str, str] | None = None, parameters: dict | None = None) -> Path:
        folder = self.tooling.plugin(app, identifier, {"implementations": implementations, "contributions": contributions, "parameters": parameters or {}})
        for relative, text in (files or {}).items():
            (folder / relative).parent.mkdir(parents=True, exist_ok=True)
            (folder / relative).write_text(text, encoding="utf-8")
        return folder

    def resolve(self, app: Path, target: str, configuration: Configuration = Configuration.DEBUG, architectures: tuple[str, ...] | None = None) -> ResolvedProduct:
        manifest = AppManifest.load(app)
        source = PluginSource(manifest.folder, PluginLock.load(manifest.folder), self.context.process, self.context.terminal)
        resolver = ProjectResolver(self.registry, PluginResolver(manifest, source), SdkSelector(self.sdks, ToolingFixture.REVISION))
        return resolver.resolve(ProductRequest(app, target, configuration, Configuration.RELEASE, architectures))

    @staticmethod
    def folders(product: ResolvedProduct) -> ProductFolders:
        return ProductFolders.of(product.app.folder, product.identity)

    @staticmethod
    def files(folder: Path) -> dict[str, bytes]:
        """Every file of a folder by its path, for comparing generated projects."""
        return {path.relative_to(folder).as_posix(): path.read_bytes() for path in sorted(folder.rglob("*")) if path.is_file()}
