import html
import json
import re
import tempfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from haylen.content.protected_releases import ProtectedReleases
from haylen.core.app_kind import AppKind
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.frozen import Frozen
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.native.cmake_app import CMakeApp
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.project_files import ProjectFiles
from haylen.platforms.web.web_splash import WebSplash


@dataclass(frozen=True)
class WebProject:
    """The project of the site of an app, which the backend generates whole: the page files of the template at its root, with the files that the section `web` of `haylen.json` puts in their place, the page scripts of the app under `scripts/`, the web modules of the plugins under `plugins/<id>/`, the splash logo and `config.json` without the sizes of the downloads, which the build adds. A Debug build of a Lua app plays the zipped package in `app.zip` with the prebuilt web player, and a Release build or a C++ app links a runtime of its own in the CMake project of `runtime/` with `haylen_add_app`, with the protected release and its bootstrap for a release."""

    TEMPLATE = Path(__file__).parent / "template"
    RUNTIME = "runtime"
    CONFIG = "config.json"
    ARCHIVE = "app.zip"
    SCRIPTS = "scripts"
    PLUGINS = "plugins"
    PLACEHOLDER = "{{SCRIPTS}}"
    # The token of the language of the page in "index.html", which a page that "files" gives may leave out.
    LANGUAGE = "{{LANGUAGE}}"
    DEFAULT_LANGUAGE = "en"
    LOADER = "loader.js"
    CONTRIBUTION = "webModule"

    product: ResolvedProduct
    content: Path | None
    bootstrap: bytes | None

    @staticmethod
    def links_runtime(product: ResolvedProduct) -> bool:
        """Whether the build links a runtime for the app, which a release and a C++ app do, instead of taking the prebuilt web player."""
        return product.configuration is Configuration.RELEASE or product.app.kind is AppKind.CPP

    def language(self) -> str:
        """The language of the page, the fallback language of the `localization` of `haylen.json` in the form of BCP 47 that the engine reads it in, such as `pt-BR` for `pt_br`, which screen readers and the translation of browsers read, or English for an app without one."""
        given = str(self.product.app.section("localization").get("fallback", WebProject.DEFAULT_LANGUAGE))
        parts = [part for part in re.split(r"[-_]", given) if part]
        subtags = [parts[0].lower()] if parts else [WebProject.DEFAULT_LANGUAGE]
        for part in parts[1:]:
            if len(part) == 4 and part.isalpha():
                subtags.append(part.capitalize())
            elif len(part) == 2 and part.isalpha():
                subtags.append(part.upper())
            else:
                subtags.append(part.lower())
        return "-".join(subtags)

    def write(self, tree: GeneratedTree) -> None:
        files = ProjectFiles.of(WebProject.TEMPLATE, "")
        section = self.product.primary.settings.get("web", {})
        for destination, source in Frozen.thaw(section.get("files", {})).items():
            files |= ProjectFiles.of(self.product.app.folder / source, destination)

        scripts = [str(script) for script in section.get("scripts", ())]
        for script in scripts:
            files[f"{WebProject.SCRIPTS}/{script}"] = self.script(script)
        index = files.get("index.html")
        if index is None or WebProject.PLACEHOLDER.encode() not in index[0]:
            raise InvalidManifest(
                f'The page "index.html" of the site has no "{WebProject.PLACEHOLDER}", where the tooling loads the loader and the page scripts. Keep it in the page that the "files" of "platforms.web" give.',
                origin=self.product.app.origin,
            )
        tags = "\n    ".join(f'<script src="{source}"></script>' for source in [WebProject.LOADER, *(f"{WebProject.SCRIPTS}/{script}" for script in scripts)])
        page = index[0].replace(WebProject.PLACEHOLDER.encode(), tags.encode()).replace(WebProject.LANGUAGE.encode(), html.escape(self.language()).encode())
        files["index.html"] = (page, index[1])

        splash = WebSplash.of(self.product)
        files[splash.file] = (splash.logo.read_bytes(), None)
        plugins, modules = self.plugins()
        files |= modules
        config = {"name": self.product.app.name, "identifier": self.product.app.identifier, "transparent": splash.transparent, "splash": splash.describe(), "plugins": plugins}
        files[WebProject.CONFIG] = ((json.dumps(config, indent=4) + "\n").encode(), None)

        if WebProject.links_runtime(self.product):
            files[f"{WebProject.RUNTIME}/CMakeLists.txt"] = (self.cmake().encode(), None)
        if self.product.configuration is Configuration.DEBUG:
            files[WebProject.ARCHIVE] = (WebProject.archive(self.product), None)
        if self.bootstrap is not None:
            files[f"{WebProject.RUNTIME}/{ProtectedReleases.BOOTSTRAP_FILE}"] = (self.bootstrap, ProtectedReleases.BOOTSTRAP_MODE)
        for relative, (content, mode) in sorted(files.items()):
            tree.write_bytes(relative, content, mode)

    def script(self, script: str) -> tuple[bytes, int | None]:
        path = self.product.app.folder / script
        relative = PurePosixPath(script)
        if relative.is_absolute() or ".." in relative.parts or not path.is_file():
            raise InvalidManifest(f'The script "{script}" of "platforms.web.scripts" in `{self.product.app.origin}` must be a file of the app folder.', origin=self.product.app.origin)
        return path.read_bytes(), None

    def plugins(self) -> tuple[list[dict[str, object]], dict[str, tuple[bytes, int | None]]]:
        """Returns the entries of `config.json` of the web modules of the plugins, in the load order of the plugins, with the values of their parameters, and the files of the folder that holds each module, which go to `plugins/<id>/`."""
        entries = []
        files: dict[str, tuple[bytes, int | None]] = {}
        modules = self.product.primary.of_type(WebProject.CONTRIBUTION)
        for plugin in self.product.plugins:
            for module in (contribution for contribution in modules if contribution.owners[0] == plugin.id):
                path = module.folders[0] / str(module.field("path"))
                if not path.is_file():
                    raise InvalidManifest(f'The web module `{Repository.shown(path)}` of the plugin "{plugin.id}" does not exist.', origin=module.origins[0])
                files |= ProjectFiles.of(path.parent, f"{WebProject.PLUGINS}/{plugin.id}")
                entry = {"id": plugin.id, "version": plugin.manifest.version.text, "module": f"{WebProject.PLUGINS}/{plugin.id}/{path.name}", "parameters": Frozen.thaw(plugin.parameters)}
                entries.append(entry)
        return entries, files

    def cmake(self) -> str:
        """Writes the CMake project of the runtime of the page, the target `haylen` that `haylen_add_app` builds against the web SDK."""
        sdk = self.product.primary.sdk.file("cmake")
        bootstrap = f'"${{CMAKE_CURRENT_LIST_DIR}}/{ProtectedReleases.BOOTSTRAP_FILE}"' if self.bootstrap is not None else None
        lines = [
            "# The runtime of the page of the app, which the tooling generates from haylen.json and builds against the web SDK with Emscripten.",
            "cmake_minimum_required(VERSION 3.28...4.4)",
            "project(App LANGUAGES C CXX)",
            "",
            f"set(haylen_DIR {CMakeApp.text(sdk.as_posix())})",
            "find_package(haylen CONFIG REQUIRED)",
            *CMakeApp.lines(self.product.app, "haylen", CMakeApp.text(self.content.as_posix()), bootstrap),
        ]
        return "\n".join([*lines, ""])

    @staticmethod
    def archive(product: ResolvedProduct) -> bytes:
        """The zipped package that the page of a Debug build downloads, which the same files zip into the same bytes."""
        with tempfile.TemporaryDirectory() as folder:
            archive = Path(folder) / WebProject.ARCHIVE
            product.package.zip(archive)
            return archive.read_bytes()
