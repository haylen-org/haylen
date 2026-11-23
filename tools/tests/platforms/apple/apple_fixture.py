import json
import plistlib
import shutil
from pathlib import Path

import pytest

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.product_request import ProductRequest
from haylen.core.project_resolver import ProjectResolver
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.platforms.apple.apple_backend import AppleBackend
from haylen.platforms.apple.apple_engine_port import AppleEnginePort
from haylen.platforms.apple.apple_project import AppleProject
from haylen.platforms.apple.apple_sdk_files import AppleSdkFiles
from haylen.platforms.apple.apple_targets import AppleTargets
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_folders import ProductFolders
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_resolver import PluginResolver
from haylen.plugins.plugin_source import PluginSource
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_selector import SdkSelector
from haylen.toolchains.xcodegen import XcodeGen
from tests.support.recorded_commands import RecordedCommands
from tests.support.tooling_fixture import ToolingFixture


class AppleFixture:
    """Apps, plugins and an installed Apple SDK in a temporary folder for the tests of the Apple backend: an SDK with a slice of every Apple target, whose frameworks are empty folders, a registry with the Apple backend and its engine port, and a context whose native tools never run, so a preparation writes its project and records the commands of XcodeGen and CMake."""

    FRAMEWORKS = {
        "iOS": ["UIKit.framework", "Metal.framework"],
        "macCatalyst": ["UIKit.framework", "Metal.framework", "IOKit.framework"],
        "tvOS": ["UIKit.framework", "Metal.framework"],
        "visionOS": ["UIKit.framework", "Metal.framework"],
        "macOS": ["Cocoa.framework", "Metal.framework"],
    }
    XCODEGEN = Path("/tools/xcodegen")
    DEFINITIONS = ["IMGUI_DEFINE_MATH_OPERATORS", 'IMGUI_USER_CONFIG="haylen/ui/ImGuiConfig.hpp"']

    def __init__(self, folder: Path, monkeypatch: pytest.MonkeyPatch, environment: dict[str, str] | None = None) -> None:
        monkeypatch.setattr(Repository, "APPS", folder / "apps")
        monkeypatch.setattr(XcodeGen, "path", staticmethod(lambda context: AppleFixture.XCODEGEN))
        self.folder = folder
        self.tooling = ToolingFixture(folder)
        self.recorded = RecordedCommands(folder, environment)
        self.context = self.recorded.context
        self.context.process.run = self.run
        self.registry = BackendRegistry(self.context, [("apple", lambda: AppleBackend)], [("apple", lambda: AppleEnginePort)])
        self.backend = AppleBackend(self.context)
        self.sdk = self.apple_sdk()

    def run(self, command: list[object], **options: object) -> None:
        """Records a command, and writes the project that XcodeGen generates, so a preparation finds it the next time."""
        self.recorded.run(command, **options)
        parts = [str(part) for part in command]
        if parts[0] == str(AppleFixture.XCODEGEN):
            project = Path(parts[parts.index("--project") + 1]) / AppleProject.XCODE_PROJECT
            project.mkdir(parents=True, exist_ok=True)
            (project / "project.pbxproj").write_text("// The project.\n", encoding="utf-8")

    def apple_sdk(self) -> SdkManifest:
        folder = self.folder / "sdk" / "apple"
        for name in (AppleSdkFiles.FRAMEWORK_NAME, AppleSdkFiles.LUA_NAME, "include", "lib/cmake/haylen"):
            (folder / name).mkdir(parents=True)
        engine = Repository.ENGINE / "platform" / "apple"
        shutil.copy(engine / AppleSdkFiles.PRIVACY_NAME, folder / AppleSdkFiles.PRIVACY_NAME)
        shutil.copy(engine / AppleSdkFiles.SWIFT_HELPERS_NAME, folder / AppleSdkFiles.SWIFT_HELPERS_NAME)
        settings = {"frameworks": AppleFixture.FRAMEWORKS, "definitions": AppleFixture.DEFINITIONS}
        (folder / AppleSdkFiles.SETTINGS_NAME).write_text(json.dumps(settings), encoding="utf-8")
        slices = [
            {
                "system": target.system,
                "environment": target.environment.value,
                "architectures": list(target.architectures),
                "abi": target.abi,
                "toolchain": "xcode",
                "runtime": "libc++",
                "minimumSystemVersion": target.minimum_version,
                "graphicsApis": ["metal"],
                "capabilities": sorted(target.capabilities),
                "libraries": {
                    "haylen": f"{AppleSdkFiles.FRAMEWORK_NAME}/{target.slice}/{AppleSdkFiles.LIBRARY_NAME}",
                    "haylen-lua": f"{AppleSdkFiles.LUA_NAME}/{target.slice}/{AppleSdkFiles.LUA_LIBRARY_NAME}",
                },
            }
            for target in AppleTargets.ALL
        ]
        files = {
            AppleSdkFiles.FRAMEWORK: AppleSdkFiles.FRAMEWORK_NAME,
            AppleSdkFiles.LUA: AppleSdkFiles.LUA_NAME,
            AppleSdkFiles.SETTINGS: AppleSdkFiles.SETTINGS_NAME,
            AppleSdkFiles.PRIVACY: AppleSdkFiles.PRIVACY_NAME,
            AppleSdkFiles.SWIFT_HELPERS: AppleSdkFiles.SWIFT_HELPERS_NAME,
            AppleSdkFiles.INCLUDE: "include",
            AppleSdkFiles.CMAKE: "lib/cmake/haylen",
        }
        document = {"format": 1, "port": "apple", "portVersion": "1.0.0", "engineVersion": "0.0.1", "revision": ToolingFixture.REVISION, "configuration": "Release", "slices": slices, "files": files}
        (folder / SdkManifest.FILE).write_text(json.dumps(document), encoding="utf-8")
        return SdkManifest.load(folder)

    def app(self, document: dict, name: str = "app") -> Path:
        return self.tooling.app(document, name)

    def cpp_app(self, document: dict, name: str = "game") -> Path:
        folder = self.folder / name
        (folder / "src").mkdir(parents=True)
        (folder / "include").mkdir()
        (folder / "content").mkdir()
        (folder / "src" / "App.cpp").write_text("// The app.\n", encoding="utf-8")
        manifest = {
            "name": "Game",
            "identifier": "com.haylen.game",
            "version": "1.0.0",
            "category": "game",
            "kind": "cpp",
            "cpp": {"sources": ["src"], "includeDirectories": ["include"], "definitions": ["GAME=1"]},
            **document,
        }
        (folder / AppManifest.FILE).write_text(json.dumps(manifest, indent=4), encoding="utf-8")
        return folder

    def plugin(self, app: Path, identifier: str, document: dict) -> Path:
        return self.tooling.plugin(app, identifier, document)

    def resolve(self, app: Path, target: str = "ios-simulator", configuration: str = "Debug") -> ResolvedProduct:
        manifest = AppManifest.load(app)
        source = PluginSource(manifest.folder, PluginLock.load(manifest.folder), self.context.process, self.context.terminal)
        resolver = ProjectResolver(self.registry, PluginResolver(manifest, source), SdkSelector((self.sdk,), ToolingFixture.REVISION))
        return resolver.resolve(ProductRequest(app, target, Configuration(configuration), Configuration.RELEASE, None))

    def prepare(self, app: Path, target: str = "ios-simulator") -> PreparedProject:
        product = self.resolve(app, target)
        return self.backend.prepare(product, ProductFolders.of(app, product.identity))

    @staticmethod
    def spec(prepared: PreparedProject) -> dict:
        text = (prepared.folders.project / AppleProject.SPEC).read_text(encoding="utf-8")
        return json.loads(text[text.index("{") :])

    @staticmethod
    def plist(prepared: PreparedProject, name: str) -> dict:
        return plistlib.loads((prepared.folders.project / name).read_bytes())
