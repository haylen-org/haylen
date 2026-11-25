from dataclasses import dataclass
from pathlib import Path

from haylen.content.protected_releases import ProtectedReleases
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.frozen import Frozen
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.native.cmake_app import CMakeApp
from haylen.platforms.desktop.desktop_app import DesktopApp
from haylen.platforms.desktop.desktop_links import DesktopLinks
from haylen.platforms.desktop.linux_categories import LinuxCategories
from haylen.platforms.desktop.linux_entries import LinuxEntries
from haylen.platforms.desktop.windows_resources import WindowsResources
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.project_files import ProjectFiles


@dataclass(frozen=True)
class DesktopProject:
    """The CMake project of a Windows or Linux app, which the backend generates whole from the template of its platform: the files of the template, the files that the section of the platform in `haylen.json` puts in their place, and the generated `app.cmake` with the values of the app, which finds the installed desktop SDK and builds the executable with `haylen_add_app`. Windows adds the values of its version resource and the script that registers the URL schemes in the installer, and Linux completes its desktop entry and AppStream metainfo. Lua and C++ apps share the project, and only the call of `haylen_add_app` differs."""

    TEMPLATE = Path(__file__).parent / "template"
    VALUES = "app.cmake"
    # The folders next to the executable of a Linux app, whose names an executable cannot take.
    LINUX_FOLDERS = frozenset({"app", "lib"})

    product: ResolvedProduct
    content: Path
    native: Path
    bootstrap: bytes | None

    @staticmethod
    def template(platform: str) -> Path:
        return DesktopProject.TEMPLATE / platform

    def write(self, tree: GeneratedTree) -> list[str]:
        """Writes the project into a generated tree and returns the warnings of the files of the app that keep their own values."""
        platform = self.product.platform
        app = DesktopApp.of(self.product.app)
        if platform == "linux" and app.executable in DesktopProject.LINUX_FOLDERS:
            raise InvalidManifest(
                f'The app folder `{Repository.shown(self.product.app.folder)}` gives the executable "{app.executable}", which is the name of a folder next to the executable of a Linux app. Rename the app folder.',
                origin=self.product.app.origin,
            )

        files = self.files(platform)
        schemes = DesktopLinks.schemes(self.product.primary)
        warnings: list[str] = []
        if platform == "windows":
            files[WindowsResources.HEADER] = (WindowsResources.header(app, self.product.app.origin).encode(), None)
            if schemes:
                files[DesktopLinks.SCRIPT] = (DesktopLinks.installer_script(app, schemes).encode(), None)
        else:
            entries = LinuxEntries()
            entry = entries.complete_entry(DesktopProject.text(files, LinuxEntries.ENTRY), LinuxEntries.keys(app, schemes), self.shown(LinuxEntries.ENTRY))
            metainfo = entries.complete_metainfo(DesktopProject.text(files, LinuxEntries.METAINFO), LinuxEntries.elements(app), self.shown(LinuxEntries.METAINFO))
            files[LinuxEntries.ENTRY] = (entry.encode(), None)
            files[LinuxEntries.METAINFO] = (metainfo.encode(), None)
            warnings += entries.warnings

        files[DesktopProject.VALUES] = (self.values(app, platform, platform == "windows" and bool(schemes)).encode(), None)
        if self.bootstrap is not None:
            files[ProtectedReleases.BOOTSTRAP_FILE] = (self.bootstrap, ProtectedReleases.BOOTSTRAP_MODE)
        for relative, (content, mode) in sorted(files.items()):
            tree.write_bytes(relative, content, mode)
        return warnings

    def files(self, platform: str) -> dict[str, tuple[bytes, int | None]]:
        """The files of the template of a platform, with the files of the app that the key `files` of its section puts in their place or adds."""
        template = DesktopProject.template(platform)
        files = ProjectFiles.of(template, "")
        section = self.product.primary.settings.get(platform, {})
        for destination, source in Frozen.thaw(section.get("files", {})).items():
            files |= ProjectFiles.of(self.product.app.folder / source, destination)
        return files

    def shown(self, relative: str) -> str:
        section = self.product.primary.settings.get(self.product.platform, {})
        own = Frozen.thaw(section.get("files", {})).get(relative)
        return Repository.shown(self.product.app.folder / own) if own else f"the template {relative}"

    @staticmethod
    def text(files: dict[str, tuple[bytes, int | None]], relative: str) -> str:
        if relative not in files:
            raise InvalidManifest(
                f'The Linux project has no "{relative}", which the tooling completes with the values of "haylen.json". Map it to a file of the app in the "files" of the section "linux", or leave it to the template.'
            )
        return files[relative][0].decode("utf-8")

    def values(self, app: DesktopApp, platform: str, installer_links: bool) -> str:
        """Writes `app.cmake`: the values of `haylen.json`, the content and the native libraries that the app ships, the installed SDK and the executable of the app."""
        product = self.product
        values = {
            "HAYLEN_APP_NAME": app.name,
            "HAYLEN_APP_IDENTIFIER": app.identifier,
            "HAYLEN_APP_VERSION": app.version,
            "HAYLEN_APP_DESCRIPTION": app.description,
            "HAYLEN_APP_COMPANY": app.company,
            "HAYLEN_APP_COPYRIGHT": app.copyright,
            "HAYLEN_APP_WEBSITE": app.website,
            "HAYLEN_APP_EXECUTABLE": app.executable,
            "HAYLEN_APP_CONTENT": self.content.as_posix(),
            "HAYLEN_APP_NATIVE": self.native.as_posix(),
        }
        if platform == "linux":
            values["HAYLEN_APP_DEBIAN_SECTION"] = LinuxCategories.debian_section(app.category)
        if platform == "windows":
            debug = product.request.engine_configuration is Configuration.DEBUG
            values["HAYLEN_MSVC_RUNTIME"] = "MultiThreadedDebugDLL" if debug else "MultiThreadedDLL"
        lines = [
            "# The values of haylen.json and the build of the executable of the app with the installed desktop SDK, which the tooling writes.",
            *(f"set({name} {CMakeApp.text(value)})" for name, value in values.items()),
            f"set(haylen_DIR {CMakeApp.text(product.primary.sdk.file('cmake').as_posix())})",
            "find_package(haylen CONFIG REQUIRED)",
        ]
        if installer_links:
            lines.append(f'set(CPACK_INNOSETUP_EXTRA_SCRIPTS "${{CMAKE_CURRENT_LIST_DIR}}/{DesktopLinks.SCRIPT}")')
        bootstrap = f'"${{CMAKE_CURRENT_LIST_DIR}}/{ProtectedReleases.BOOTSTRAP_FILE}"' if self.bootstrap is not None else None
        lines += CMakeApp.lines(product.app, "main", '"${HAYLEN_APP_CONTENT}"', bootstrap)
        return "\n".join([*lines, ""])
