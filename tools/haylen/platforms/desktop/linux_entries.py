import xml.etree.ElementTree as ElementTree

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.platforms.desktop.desktop_app import DesktopApp
from haylen.platforms.desktop.linux_categories import LinuxCategories


class LinuxEntries:
    """The desktop entry and the AppStream metainfo of a Linux app, which the backend completes from the files of the template, or the files of the app that the section `linux` puts in their place, with the name, the description, the categories, the executable, the icon, the window class, the publisher, the website and the URL schemes of the app. A key or an element that the file gives wins, with a warning when `haylen.json` gives another value."""

    ENTRY = "resources/app.desktop"
    METAINFO = "resources/app.metainfo.xml"

    def __init__(self) -> None:
        self.warnings: list[str] = []

    @staticmethod
    def entry_text(value: str) -> str:
        """Escapes a value of a desktop entry, whose strings take backslash escapes."""
        return value.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r")

    @staticmethod
    def keys(app: DesktopApp, schemes: tuple[str, ...]) -> dict[str, str]:
        """The keys of the desktop entry that come from `haylen.json`. An app without a category has no `Categories`, which the desktop entry specification leaves optional, and an app that links open hands each link to the executable after `--link`."""
        keys = {"Name": app.name, "Comment": app.description}
        categories = LinuxCategories.menu(app.category)
        if categories:
            keys["Categories"] = "".join(f"{category};" for category in categories)
        keys |= {"Exec": app.executable, "Icon": app.identifier, "StartupWMClass": app.identifier}
        if schemes:
            keys["Exec"] = f"{app.executable} --link %u"
            keys["MimeType"] = "".join(f"x-scheme-handler/{scheme};" for scheme in schemes)
        return keys

    def complete_entry(self, text: str, generated: dict[str, str], where: str) -> str:
        """Completes the `[Desktop Entry]` group of a desktop entry with the generated keys it lacks."""
        lines = text.splitlines()
        group = next((index for index, line in enumerate(lines) if line.strip() == "[Desktop Entry]"), None)
        if group is None:
            raise InvalidManifest(f'The desktop entry `{where}` has no "[Desktop Entry]" group. Give it one.', origin=where)
        end = next((index for index in range(group + 1, len(lines)) if lines[index].startswith("[")), len(lines))
        while end > group + 1 and not lines[end - 1].strip():
            end -= 1

        own = LinuxEntries.entry_keys(lines[group + 1 : end])
        added = []
        for key, value in generated.items():
            escaped = LinuxEntries.entry_text(value)
            if key not in own:
                added.append(f"{key}={escaped}")
            elif own[key] != escaped:
                self.warnings.append(f'The key "{key}" of `{where}` keeps its value "{own[key]}", while "haylen.json" gives "{escaped}".')
        return "\n".join([*lines[:end], *added, *lines[end:]]) + "\n"

    @staticmethod
    def entry_keys(lines: list[str]) -> dict[str, str]:
        keys = {}
        for line in lines:
            if "=" in line and not line.lstrip().startswith("#"):
                key, _, value = line.partition("=")
                keys[key.strip()] = value.strip()
        return keys

    @staticmethod
    def has_categories(text: str) -> bool:
        """Whether the `[Desktop Entry]` group of a completed desktop entry lists categories, which an AppImage needs."""
        lines = text.splitlines()
        group = next((index for index, line in enumerate(lines) if line.strip() == "[Desktop Entry]"), len(lines))
        end = next((index for index in range(group + 1, len(lines)) if lines[index].startswith("[")), len(lines))
        return bool(LinuxEntries.entry_keys(lines[group + 1 : end]).get("Categories"))

    @staticmethod
    def elements(app: DesktopApp) -> list[ElementTree.Element]:
        """The elements of the AppStream metainfo that come from `haylen.json`, in the order the specification lists them. The developer id is the identifier of the app without its last part, the domain of its publisher."""
        description = LinuxEntries.element("description")
        description.append(LinuxEntries.element("p", app.description))
        developer = LinuxEntries.element("developer", id=app.identifier.rsplit(".", 1)[0])
        developer.append(LinuxEntries.element("name", app.company))
        elements = [
            LinuxEntries.element("id", app.identifier),
            LinuxEntries.element("name", app.name),
            LinuxEntries.element("summary", app.description),
            description,
            developer,
            LinuxEntries.element("launchable", f"{app.identifier}.desktop", type="desktop-id"),
        ]
        categories = LinuxCategories.menu(app.category)
        if categories:
            listed = LinuxEntries.element("categories")
            for category in categories:
                listed.append(LinuxEntries.element("category", category))
            elements.append(listed)
        if app.website:
            elements.append(LinuxEntries.element("url", app.website, type="homepage"))
        return elements

    @staticmethod
    def element(tag: str, text: str | None = None, **attributes: str) -> ElementTree.Element:
        created = ElementTree.Element(tag, attributes)
        created.text = text
        return created

    def complete_metainfo(self, text: str, generated: list[ElementTree.Element], where: str) -> str:
        """Completes the `component` of an AppStream metainfo with the generated elements it lacks."""
        try:
            component = ElementTree.fromstring(text)
        except ElementTree.ParseError as error:
            line, column = error.position
            raise InvalidManifest(f"The AppStream metainfo `{where}` is not valid XML. Check line {line}, column {column + 1}.", origin=where) from error
        if component.tag != "component":
            raise InvalidManifest(f'The AppStream metainfo `{where}` must hold one "component" element.', origin=where)

        added = 0
        for element in generated:
            own = component.find(element.tag)
            if own is None:
                component.insert(added, element)
                added += 1
            elif own.text is not None and element.text is not None and own.text.strip() != element.text.strip():
                self.warnings.append(f'The element "{element.tag}" of `{where}` keeps its value "{own.text.strip()}", while "haylen.json" gives "{element.text.strip()}".')
        ElementTree.indent(component, "  ")
        return f'<?xml version="1.0" encoding="UTF-8"?>\n{ElementTree.tostring(component, encoding="unicode")}\n'
