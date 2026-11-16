import re
import shutil
from pathlib import Path

from haylen.core.repository import Repository
from haylen.core.resolved_project import ResolvedProject
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.desktop.desktop_app import DesktopApp


class DesktopLinks:
    """The links of the URL schemes that open a Windows or Linux app, which the `urlScheme` contributions of its plugins declare. Windows and Linux start a new process of the app for every link, with the link after `--link`: the installer of Windows registers the schemes, the desktop entry of Linux names them, and a run of the installed app of the build folder registers them for the person, so links reach the app while it is being made."""

    CONTRIBUTION = "urlScheme"
    SCRIPT = "links.iss"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    @staticmethod
    def schemes(project: ResolvedProject) -> tuple[str, ...]:
        return tuple(str(contribution.field("scheme")) for contribution in project.of_type(DesktopLinks.CONTRIBUTION))

    @staticmethod
    def inno_text(value: str) -> str:
        """Escapes a value of an Inno Setup script, whose strings double their quotes and whose braces start constants."""
        return value.replace('"', '""').replace("{", "{{")

    @staticmethod
    def installer_script(app: DesktopApp, schemes: tuple[str, ...]) -> str:
        """Writes the `[Registry]` section of the installer of Windows, which registers the URL schemes among the classes of the registry of the person, or of every user for an install for them, and removes them with the app."""
        executable = f"{{app}}\\{DesktopLinks.inno_text(app.executable)}.exe"
        lines = ["; The URL schemes of the plugins of the app, which the installer registers with the app and the tooling writes.", "[Registry]"]
        for scheme in schemes:
            key = f"Software\\Classes\\{scheme}"
            lines += [
                f'Root: HKA; Subkey: "{key}"; ValueType: string; ValueName: ""; ValueData: "URL:{DesktopLinks.inno_text(app.name)}"; Flags: uninsdeletekey',
                f'Root: HKA; Subkey: "{key}"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""',
                f'Root: HKA; Subkey: "{key}\\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{executable},0"',
                f'Root: HKA; Subkey: "{key}\\shell\\open\\command"; ValueType: string; ValueName: ""; ValueData: """{executable}"" --link ""%1"""',
            ]
        return "\n".join(lines) + "\n"

    def register_windows(self, app: DesktopApp, schemes: tuple[str, ...], executable: Path) -> None:
        """Registers the URL schemes among the classes of the registry of the person, pointing at the installed executable as the installer does."""
        import winreg

        for scheme in schemes:
            self.context.terminal.step(f'Registering the links of "{scheme}" for `{Repository.shown(executable)}`')
            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, f"Software\\Classes\\{scheme}") as key:
                winreg.SetValueEx(key, "", 0, winreg.REG_SZ, f"URL:{app.name}")
                winreg.SetValueEx(key, "URL Protocol", 0, winreg.REG_SZ, "")
            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, f"Software\\Classes\\{scheme}\\shell\\open\\command") as key:
                winreg.SetValueEx(key, "", 0, winreg.REG_SZ, f'"{executable}" --link "%1"')

    def register_linux(self, app: DesktopApp, schemes: tuple[str, ...], installed: Path) -> None:
        """Registers the URL schemes with a desktop entry of the person in `~/.local/share/applications`, whose executable is the installed one, and makes the app the handler of each scheme with `xdg-mime`, as a package does once it installs."""
        if not schemes:
            return
        name = f"{app.identifier}.desktop"
        executable = installed / "bin" / app.executable
        entry = (installed / "share" / "applications" / name).read_text(encoding="utf-8")
        entry = re.sub(r"^Exec=.*$", lambda _: f"Exec={executable} --link %u", entry, flags=re.MULTILINE)
        applications = Path.home() / ".local" / "share" / "applications"
        applications.mkdir(parents=True, exist_ok=True)
        shown = ", ".join(f'"{scheme}"' for scheme in schemes)
        self.context.terminal.step(f"Registering the links of {shown} for `{Repository.shown(executable)}`")
        target = applications / name
        if not target.is_file() or target.read_text(encoding="utf-8") != entry:
            target.write_text(entry, encoding="utf-8")
        if shutil.which("xdg-mime") is None:
            self.context.terminal.warning('The links reach the app once the desktop reads its entry, since "xdg-mime" is missing to make it their handler.')
            return
        for scheme in schemes:
            self.context.process.run(["xdg-mime", "default", name, f"x-scheme-handler/{scheme}"])
