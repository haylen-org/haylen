from pathlib import Path

from haylen.core.configuration import Configuration
from haylen.core.repository import Repository
from haylen.platforms.desktop.desktop_app import DesktopApp
from haylen.platforms.desktop.desktop_layout import DesktopLayout
from haylen.platforms.desktop.windows_resources import WindowsResources
from haylen.platforms.verification_check import VerificationCheck


class DesktopVerifier:
    """The checks of an installed Windows or Linux app that only a desktop knows: the executable, the package and the native libraries where the engine finds them, the application manifest of a Windows executable with what the engine needs from it, and the desktop entry of a Linux app named after its identifier."""

    SOURCE = "desktop"
    # What the engine needs from the application manifest of a Windows executable, each with the text that shows it in the manifest.
    WINDOWS_MANIFEST_NEEDS = {
        "the Common Controls of version 6, which give the task dialogs and the controls that native libraries show the current look of Windows": "Microsoft.Windows.Common-Controls",
        "UTF-8 as the code page of the process, which the text of Haylen needs": ">UTF-8</activeCodePage>",
    }

    @staticmethod
    def checks(platform: str, app: DesktopApp, layout: DesktopLayout, configuration: Configuration, native: Path) -> list[VerificationCheck]:
        failed = []
        executable = layout.executable
        if not executable.is_file():
            failed.append(f'The built app lacks its executable "{layout.executable_path}". The project of the app builds and installs it, so prepare the app again.')
        if configuration is Configuration.DEBUG and not (layout.folder / layout.package / "haylen.json").is_file():
            failed.append(f'The built app lacks the package of the app in "{layout.package}", where the engine opens it.')
        for library in sorted(native.iterdir()) if native.is_dir() else []:
            if not (layout.libraries / library.name).exists():
                failed.append(f'The built app lacks the native library "{library.name}" in "{layout.libraries_path or "."}", where "native.load" finds it.')

        if platform == "windows" and executable.is_file():
            manifest = b"".join(WindowsResources.read(executable.read_bytes(), WindowsResources.MANIFEST)).decode("utf-8", "replace")
            for need, marker in DesktopVerifier.WINDOWS_MANIFEST_NEEDS.items():
                if marker not in manifest:
                    failed.append(f'The application manifest of the executable lacks {need}. Keep the manifest of the engine that "haylen_add_app" adds.')
        entry = layout.folder / "share" / "applications" / f"{app.identifier}.desktop"
        if platform == "linux" and not entry.is_file():
            failed.append(f'The built app lacks the desktop entry "share/applications/{app.identifier}.desktop", by which Linux desktops find the name and the icon of the window of the app.')

        if failed:
            return [VerificationCheck.failed("desktop", problem, DesktopVerifier.SOURCE) for problem in failed]
        return [
            VerificationCheck.passed(
                "desktop", f"The app `{Repository.shown(layout.folder)}` holds its executable, its package and its native libraries where the engine finds them.", DesktopVerifier.SOURCE
            )
        ]
