import shutil
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType

from haylen.content.protected_release import ProtectedRelease
from haylen.content.release_package import ReleasePackage
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext


class SteamDepots:
    """Lays a built desktop release out for SteamPipe, which delivers the app with its protected release and every named pack as a depot of its own, so a Steam app needs no content server: the depot of the platform under `content/<depot>/`, the shards of each pack under `content/<pack>/<place>/packs/`, where the runtime looks for them next to the executable or the bundle, and the build scripts of steamcmd under `scripts/`."""

    # The KeyValues text of the scripts of steamcmd ends a value at a double quote and reads these escapes inside it.
    ESCAPES = str.maketrans({"\\": "\\\\", '"': '\\"', "\n": "\\n", "\r": "\\r", "\t": "\\t"})
    APP_SCRIPT = "app-build.vdf"

    @dataclass(frozen=True)
    class Options:
        """The options of the format `steam`: the app ID of Steamworks, the numeric ID of every depot by its name and the branch that the build goes live on, if any."""

        app_id: str
        depots: Mapping[str, str]
        branch: str | None

        @staticmethod
        def parse(options: Mapping[str, object]) -> SteamDepots.Options:
            """Reads the options of a package request: `appId`, `depots` as a mapping, as `name=id` entries or as one such entry, which the command line gives for one depot, and `branch`."""
            unknown = sorted(set(options) - {"appId", "depots", "branch"})
            if unknown:
                names = ", ".join(f'"{name}"' for name in unknown)
                raise InvalidOption(f'The format "steam" takes no options {names}. It takes "appId", "depots" and "branch".')

            app_id = str(options.get("appId", ""))
            if not app_id.isdigit():
                raise InvalidOption('The format "steam" needs the numeric app ID of Steamworks in "appId".')

            given = options.get("depots", {})
            if isinstance(given, str):
                given = [given]
            entries = list(given.items()) if isinstance(given, Mapping) else [SteamDepots.Options.depot_entry(str(entry)) for entry in given]
            depots: dict[str, str] = {}
            for name, value in entries:
                if not str(value).isdigit():
                    raise InvalidOption(f'The depot "{name}" needs its numeric ID, such as "{name}=1001", which Steamworks shows.')
                depots[str(name)] = str(value)

            branch = options.get("branch")
            return SteamDepots.Options(app_id, MappingProxyType(depots), str(branch) if branch else None)

        @staticmethod
        def depot_entry(entry: str) -> tuple[str, str]:
            """Splits a depot entry of the command line, such as `windows=1001`, into its name and its ID."""
            name, _, identifier = entry.partition("=")
            return name, identifier

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def stage(self, source: ReleasePackage, release: ProtectedRelease, depot: str, product: Path, *, bundle: bool, packs_place: str, options: Options, output: Path) -> Path:
        """Copies a built product into the depot `depot`, as the depot folder itself or, with `bundle`, as a bundle inside it, puts the shards of every pack of the release into a depot of its own and writes the build scripts, and returns the app build script that steamcmd runs."""
        names = [depot, *release.packs]
        missing = [name for name in names if name not in options.depots]
        if missing:
            listed = ", ".join(f'"{name}"' for name in missing)
            raise InvalidOption(f'The depots {listed} have no ID. Give each one with "<name>=<id>" in the depots of the format "steam", the IDs that Steamworks shows.')

        content = output / "content"
        Folders.remove(content)
        Folders.remove(output / "scripts")
        destination = content / depot / product.name if bundle else content / depot
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(product, destination, symlinks=True)
        for pack, folder in release.packs.items():
            shutil.copytree(folder, content / pack / packs_place / "packs")

        script = SteamDepots.write_scripts(output, options.app_id, f"{source.app.name} {source.app.version}", {name: options.depots[name] for name in names}, options.branch)
        terminal = self.context.terminal
        terminal.success(f"Staged the Steam depots of `{Repository.shown(source.app.folder)}` in `{Repository.shown(output)}`.")
        terminal.info(f'Upload them with "steamcmd +login <account> +run_app_build {Repository.shown(script)} +quit".')
        return script

    @staticmethod
    def value(text: str) -> str:
        """Quotes a value for a script of steamcmd, escaping what would end the value or break its line, so a description, a branch or a pack name of any text stays one value."""
        escaped = text.translate(SteamDepots.ESCAPES)
        return f'"{escaped}"'

    @staticmethod
    def write_scripts(output: Path, app_id: str, description: str, depots: Mapping[str, str], branch: str | None) -> Path:
        """Writes the app build script and one depot build script per depot for `steamcmd +run_app_build`, each depot taking its whole folder under `content/`."""
        scripts = output / "scripts"
        scripts.mkdir(parents=True, exist_ok=True)
        value = SteamDepots.value
        entries = "".join(f"\t\t{value(depot)} {value(f'depot-build-{depot}.vdf')}\n" for depot in depots.values())
        live = f'\t"SetLive" {value(branch)}\n' if branch else ""
        app = f'"AppBuild"\n{{\n\t"AppID" {value(app_id)}\n\t"Desc" {value(description)}\n\t"ContentRoot" "../content/"\n\t"BuildOutput" "../output/"\n{live}\t"Depots"\n\t{{\n{entries}\t}}\n}}\n'
        (scripts / SteamDepots.APP_SCRIPT).write_text(app, encoding="utf-8")

        for name, depot in depots.items():
            mapping = '\t"FileMapping"\n\t{\n\t\t"LocalPath" "*"\n\t\t"DepotPath" "."\n\t\t"Recursive" "1"\n\t}\n'
            text = f'"DepotBuild"\n{{\n\t"DepotID" {value(depot)}\n\t"ContentRoot" {value(f"../content/{name}/")}\n{mapping}}}\n'
            (scripts / f"depot-build-{depot}.vdf").write_text(text, encoding="utf-8")
        return scripts / SteamDepots.APP_SCRIPT
