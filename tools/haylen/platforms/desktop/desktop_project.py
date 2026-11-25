import re
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
    WINDOWS_MANIFEST = "resources/app.manifest"
    EXECUTION_LEVEL = re.compile(r'(<requestedExecutionLevel\b[^>]*\blevel=")([A-Za-z]+)(")')
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
        app = DesktopApp.of_product(self.product)
        if platform == "linux" and app.executable in DesktopProject.LINUX_FOLDERS:
            raise InvalidManifest(
                f'The app folder `{Repository.shown(self.product.app.folder)}` gives the executable "{app.executable}", which is the name of a folder next to the executable of a Linux app. Name another executable in "platforms.linux.executableName", or rename the app folder.',
                origin=self.product.app.origin,
            )

        files = self.files(platform)
        schemes = DesktopLinks.schemes(self.product.primary)
        warnings: list[str] = []
        if platform == "windows":
            files[WindowsResources.HEADER] = (WindowsResources.header(app, self.product.app.origin).encode(), None)
            files[DesktopProject.WINDOWS_MANIFEST] = (self.windows_manifest(app, self.text(files, DesktopProject.WINDOWS_MANIFEST)).encode(), None)
            if schemes:
                files[DesktopLinks.SCRIPT] = (DesktopLinks.installer_script(app, schemes).encode(), None)
        else:
            entries = LinuxEntries()
            entry = entries.complete_entry(self.text(files, LinuxEntries.ENTRY), LinuxEntries.keys(app, schemes), self.shown(LinuxEntries.ENTRY))
            metainfo = entries.complete_metainfo(self.text(files, LinuxEntries.METAINFO), LinuxEntries.elements(app), self.shown(LinuxEntries.METAINFO))
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

    def windows_manifest(self, app: DesktopApp, text: str) -> str:
        """The application manifest of a Windows app with the execution level of `platforms.windows.executionLevel`, which a manifest of the app that `files` gives keeps unless the key names another level, which stops with both owners."""
        found = DesktopProject.EXECUTION_LEVEL.search(text)
        if found is None:
            raise InvalidManifest(
                f'The application manifest {self.shown(DesktopProject.WINDOWS_MANIFEST)} has no "requestedExecutionLevel", which sets the rights that the app asks Windows for. Keep the one of the template.',
                origin=self.product.app.origin,
            )
        own = DesktopProject.WINDOWS_MANIFEST in Frozen.thaw(self.product.primary.settings.get("windows", {}).get("files", {}))
        given = "executionLevel" not in self.product.primary.defaulted_settings.get("windows", frozenset())
        if own and not given:
            return text
        if own and found[2] != app.execution_level:
            raise InvalidManifest(
                f'The application manifest {self.shown(DesktopProject.WINDOWS_MANIFEST)} asks for the level "{found[2]}", while "platforms.windows.executionLevel" of `{self.product.app.origin}` gives "{app.execution_level}". Remove the key, or make them agree.',
                origin=self.product.app.origin,
            )
        return DesktopProject.EXECUTION_LEVEL.sub(lambda match: f"{match[1]}{app.execution_level}{match[3]}", text, count=1)

    def shown(self, relative: str) -> str:
        section = self.product.primary.settings.get(self.product.platform, {})
        own = Frozen.thaw(section.get("files", {})).get(relative)
        return Repository.shown(self.product.app.folder / own) if own else f"the template {relative}"

    def text(self, files: dict[str, tuple[bytes, int | None]], relative: str) -> str:
        platform = self.product.platform
        if relative not in files:
            raise InvalidManifest(
                f'The {platform.capitalize()} project has no "{relative}", which the tooling completes with the values of "haylen.json". Map it to a file of the app in the "files" of the section "{platform}", or leave it to the template.'
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
            values["HAYLEN_APP_MINIMUM_SYSTEM"] = str(product.primary.minimum_system_version.value)
        lines = [
            "# The values of haylen.json and the build of the executable of the app with the installed desktop SDK, which the tooling writes.",
            *(f"set({name} {CMakeApp.text(value)})" for name, value in values.items()),
            f"set(haylen_DIR {CMakeApp.text(product.primary.sdk.file('cmake').as_posix())})",
            "find_package(haylen CONFIG REQUIRED)",
        ]
        if installer_links:
            lines.append(f'set(CPACK_INNOSETUP_EXTRA_SCRIPTS "${{CMAKE_CURRENT_LIST_DIR}}/{DesktopLinks.SCRIPT}")')
        bootstrap = f'"${{CMAKE_CURRENT_LIST_DIR}}/{ProtectedReleases.BOOTSTRAP_FILE}"' if self.bootstrap is not None else None
        lines += CMakeApp.lines(product.app, "main", '"${HAYLEN_APP_CONTENT}"', bootstrap, console=app.console)
        return "\n".join([*lines, ""])
