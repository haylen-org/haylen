import re
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from haylen.content.protected_release import ProtectedRelease
from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.platforms.android.android_template import AndroidTemplate
from haylen.plugins.merged_contribution import MergedContribution


@dataclass(frozen=True)
class AndroidAssetPacks:
    """The asset packs of the protected release of an Android app, by their names. Google Play takes a base module of at most 500 MB to download, which also carries the code and the native libraries of the app, so a release larger than the share of the base module that this leaves moves its content shards into install-time asset packs, which Android reads through the same assets as the base module, while the manifests and the shards of the app domain stay in the base module. The named packs of a release become asset packs that Play delivers after the install when a plugin of the app delivers them. The files map the paths of the project to the shards."""

    FOLDER = "asset-packs"
    RELEASE_ASSETS = "app/src/main/assets/app"
    BASE_RELEASE_LIMIT = 384 * 1024**2
    # A shard holds at most twice the shard target of the Android content profile and one record, so a shard always fits a pack of this size.
    PACK_LIMIT = 1024**3
    CONTENT_NAME = "haylencontent"
    INSTALL_TIME = "install-time"
    DELIVERIES = {"install-time": "with the app", "fast-follow": "right after the install", "on-demand": "when the app asks for it"}
    # Google Play takes asset packs that deliver after the install of at most 1.5 GB each, and at most 100 asset packs in an app.
    STORE_PACK_LIMIT = 1500 * 1000**2
    MAXIMUM_PACKS = 100

    deliveries: Mapping[str, str]
    files: Mapping[str, bytes | Path]

    @property
    def install_time(self) -> tuple[str, ...]:
        return tuple(name for name, delivery in self.deliveries.items() if delivery == AndroidAssetPacks.INSTALL_TIME)

    @property
    def delivered(self) -> tuple[str, ...]:
        """The packs that Google Play delivers after the install, which only a bundle holds."""
        return tuple(name for name, delivery in self.deliveries.items() if delivery != AndroidAssetPacks.INSTALL_TIME)

    @staticmethod
    def store_name(pack: str) -> str:
        """The name of the asset pack of Google Play that holds a named pack of the content, since Play names packs with letters, digits and underscores that start with a letter. The native part of a plugin that delivers the packs names them the same way."""
        name = re.sub(r"[-.]", "_", pack)
        return f"pack_{name}" if name[0].isdigit() else name

    @staticmethod
    def module(name: str, delivery: str) -> str:
        tokens = {"NAME": AndroidTemplate.kotlin(name), "DELIVERY": AndroidTemplate.kotlin(delivery), "DELIVERY_WORDS": AndroidAssetPacks.DELIVERIES[delivery]}
        return AndroidTemplate.render(AndroidTemplate.ASSET_PACK, tokens)

    @staticmethod
    def plan(shipped: Path, kept: tuple[str, ...], packs: Mapping[str, Path], delivery: MergedContribution | None, content_packs: tuple[str, ...]) -> AndroidAssetPacks:
        """Places the files of the shipped part of a release: in the base module, in install-time packs when they pass the share of the base module, with the files of `kept` always in the base module, and the named packs of a plugin that delivers them."""
        files: dict[str, bytes | Path] = {}
        deliveries: dict[str, str] = {}
        released = sorted(path for path in shipped.iterdir() if path.is_file())
        if ProtectedRelease.size(shipped) <= AndroidAssetPacks.BASE_RELEASE_LIMIT:
            files |= {f"{AndroidAssetPacks.RELEASE_ASSETS}/{path.name}": path for path in released}
        else:
            AndroidAssetPacks.fill(released, kept, files, deliveries)
        if delivery is not None:
            AndroidAssetPacks.deliver(packs, delivery, content_packs, files, deliveries)
        if len(deliveries) > AndroidAssetPacks.MAXIMUM_PACKS:
            raise UnsupportedTarget(
                f'The release needs {len(deliveries)} asset packs, more than the {AndroidAssetPacks.MAXIMUM_PACKS} that Google Play takes. Join named packs of "content.delivery" in "haylen.json", or deliver more content from the content server.'
            )
        return AndroidAssetPacks(deliveries, files)

    @staticmethod
    def fill(released: list[Path], kept: tuple[str, ...], files: dict[str, bytes | Path], deliveries: dict[str, str]) -> None:
        """Moves the content shards into install-time packs, filled in the order of their names, and keeps the rest in the base module. The shards in the packs keep the folder `app/` of the assets, where the runtime reads the release whichever module installed them."""
        filled = AndroidAssetPacks.PACK_LIMIT
        name = ""
        for path in released:
            if not path.name.endswith(ProtectedRelease.SHARD_SUFFIX) or path.name in kept:
                files[f"{AndroidAssetPacks.RELEASE_ASSETS}/{path.name}"] = path
                continue
            size = path.stat().st_size
            if filled + size > AndroidAssetPacks.PACK_LIMIT:
                name = f"{AndroidAssetPacks.CONTENT_NAME}{len(deliveries) + 1}"
                deliveries[name] = AndroidAssetPacks.INSTALL_TIME
                files[f"{AndroidAssetPacks.FOLDER}/{name}/build.gradle.kts"] = AndroidAssetPacks.module(name, AndroidAssetPacks.INSTALL_TIME).encode()
                filled = 0
            files[f"{AndroidAssetPacks.FOLDER}/{name}/src/main/assets/app/{path.name}"] = path
            filled += size

    @staticmethod
    def deliver(packs: Mapping[str, Path], delivery: MergedContribution, content_packs: tuple[str, ...], files: dict[str, bytes | Path], deliveries: dict[str, str]) -> None:
        """Writes an asset pack for every named pack of the release, which deliver right after the install when the plugin lists them in `fastFollow` and when the app asks for them otherwise. The shards of each sit at the root of its assets, the folder that Play gives the plugin once the pack is on the device."""
        owner = delivery.owners[0]
        fast = tuple(delivery.field("fastFollow", ()))
        unknown = [pack for pack in fast if pack not in content_packs]
        if unknown:
            names = ", ".join(f'"{pack}"' for pack in unknown)
            raise InvalidContribution(
                f'The plugin "{owner}" delivers right after the install the packs {names}, which "content.delivery" of "haylen.json" does not name. List only the packs of the app.',
                origin=delivery.origins[0],
            )

        names: dict[str, str] = {}
        for pack, folder in sorted(packs.items()):
            name = AndroidAssetPacks.store_name(pack)
            if name in names:
                raise InvalidContribution(
                    f'The packs "{names[name]}" and "{pack}" both become the asset pack "{name}" of Google Play, which names packs with letters, digits and underscores. Rename one of them in "content.delivery" of "haylen.json".'
                )
            if name in deliveries:
                raise InvalidContribution(
                    f'The pack "{pack}" becomes the asset pack "{name}" of Google Play, which the release already takes for its content shards. Rename the pack in "content.delivery" of "haylen.json".'
                )
            names[name] = pack
            shards = sorted(path for path in folder.iterdir() if path.is_file())
            size = sum(path.stat().st_size for path in shards)
            if size > AndroidAssetPacks.STORE_PACK_LIMIT:
                raise UnsupportedTarget(
                    f'The pack "{pack}" holds {size} bytes, more than the 1.5 GB that Google Play takes in an asset pack. Split its content into several named packs in "content.delivery" of "haylen.json".'
                )
            kind = "fast-follow" if pack in fast else "on-demand"
            deliveries[name] = kind
            files[f"{AndroidAssetPacks.FOLDER}/{name}/build.gradle.kts"] = AndroidAssetPacks.module(name, kind).encode()
            files |= {f"{AndroidAssetPacks.FOLDER}/{name}/src/main/assets/{path.name}": path for path in shards}
