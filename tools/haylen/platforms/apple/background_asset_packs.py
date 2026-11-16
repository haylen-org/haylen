import json
import os
from pathlib import Path

from haylen.content.protected_release import ProtectedRelease
from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.core.resolved_project import ResolvedProject
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.apple.apple_contributions import AppleContributions
from haylen.platforms.apple.apple_target import AppleTarget


class BackgroundAssetPacks:
    """Packs every named pack of the protected release of an app into an asset pack of Background Assets with `ba-package`, when a plugin delivers the packs from the App Store through its `assetPacks` contribution. The packs that it lists in `prefetch` download with the install and every update, the others when the app asks for them, and the shards of each lie in a folder named after the pack, which the plugin finds in the downloaded packs. The archives are for the upload of the app to App Store Connect."""

    SYSTEMS = {"ios": "iOS", "catalyst": "iOS", "tvos": "tvOS", "visionos": "visionOS", "macos": "macOS"}

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def package(self, project: ResolvedProject, target: AppleTarget, release: ProtectedRelease, output: Path) -> tuple[Path, ...]:
        Folders.remove(output)
        delivering = project.of_type(AppleContributions.ASSET_PACKS)
        if not delivering or not release.packs:
            return ()

        contribution = delivering[0]
        prefetch = [str(pack) for pack in contribution.field("prefetch", ())]
        unknown = [pack for pack in prefetch if pack not in release.packs]
        if unknown:
            names = ", ".join(f'"{pack}"' for pack in unknown)
            raise InvalidContribution(
                f'The plugin "{contribution.owners[0]}" downloads the packs {names} with the install, which "content.delivery" of `{project.app.origin}` does not name. List only packs of the app.'
            )

        output.mkdir(parents=True)
        archives = []
        for name, folder in sorted(release.packs.items()):
            policy = {"prefetch": {"installationEventTypes": ["firstInstallation", "subsequentUpdate"]}} if name in prefetch else {"onDemand": {}}
            manifest = output / f"{name}.json"
            # The tool takes the source folders relative to the source root, which is relative to the manifest.
            document = {
                "assetPackID": name,
                "downloadPolicy": policy,
                "fileSelectors": [{"directorySource": folder.name, "directoryDestination": name}],
                "platforms": [BackgroundAssetPacks.SYSTEMS[target.platform]],
                "sourceRoot": os.path.relpath(folder.parent, output),
            }
            manifest.write_text(json.dumps(document, indent=4) + "\n", encoding="utf-8")
            archive = output / f"{name}.aar"
            self.context.process.run(["xcrun", "ba-package", "package", manifest, "--output-path", archive])
            manifest.unlink()
            archives.append(archive)
        self.context.terminal.info(
            f'The plugin "{contribution.owners[0]}" delivers the named packs of the release from the App Store, in the asset packs of `{Repository.shown(output)}`, which the upload of the app takes.'
        )
        return tuple(archives)
