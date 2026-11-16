import json
import plistlib
from collections.abc import Mapping, Sequence
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.frozen import Frozen
from haylen.core.repository import Repository


class PropertyLists:
    """Reads, joins and compares the property lists of Apple apps, such as the Info.plist, the entitlements and the privacy manifest. A join takes layers in order, each with the origin that a message names: objects join key by key, lists gain the items they lack and two different values of one key stop the preparation with both origins, so no value replaces another by the order of the layers."""

    @staticmethod
    def read(path: Path) -> dict:
        try:
            return plistlib.loads(path.read_bytes())
        except plistlib.InvalidFileException as error:
            raise BuildFailed(f"The file `{Repository.shown(path)}` is not a property list: {error}.") from error

    @staticmethod
    def dumps(values: Mapping[str, object]) -> bytes:
        return plistlib.dumps(Frozen.thaw(values), sort_keys=True)

    @staticmethod
    def join(layers: Sequence[tuple[str, Mapping[str, object]]], subject: str) -> dict:
        """Joins the layers of one property list, the first being the base, and fails with every conflict, each naming the key and both origins."""
        joined: dict = {}
        owners: dict[str, str] = {}
        conflicts: list[str] = []
        for origin, values in layers:
            PropertyLists.add(joined, Frozen.thaw(values), origin, "", owners, conflicts)
        if conflicts:
            lines = "\n".join(f"  {conflict}" for conflict in conflicts)
            raise InvalidContribution(f"The values of {subject} conflict:\n{lines}")
        return joined

    @staticmethod
    def add(joined: dict, values: Mapping, origin: str, place: str, owners: dict[str, str], conflicts: list[str]) -> None:
        for key, value in values.items():
            path = f"{place}.{key}" if place else key
            if key not in joined:
                joined[key] = value
                owners[path] = origin
                continue
            current = joined[key]
            if isinstance(current, dict) and isinstance(value, dict):
                PropertyLists.add(current, value, origin, path, owners, conflicts)
            elif isinstance(current, list) and isinstance(value, list):
                current.extend(item for item in value if item not in current)
            elif current != value:
                owner = owners.get(path) or next(owners[name] for name in sorted(owners, key=len, reverse=True) if path.startswith(f"{name}."))
                conflicts.append(f'The key "{path}" is {json.dumps(current)} for {owner} and {json.dumps(value)} for {origin}. Make them agree.')

    @staticmethod
    def holds(built: object, needed: object, item: bool = False) -> bool:
        """Tells whether a value of a built app holds a required one: objects hold every required key, lists hold an item for every required item, items of lists and booleans must match, and any other value only has to exist, so a developer may word a usage description in their own way."""
        if isinstance(needed, Mapping):
            return isinstance(built, Mapping) and all(key in built and PropertyLists.holds(built[key], value) for key, value in needed.items())
        if isinstance(needed, list | tuple):
            return isinstance(built, list | tuple) and all(any(PropertyLists.holds(entry, wanted, True) for entry in built) for wanted in needed)
        return built == needed if item or isinstance(needed, bool) else True
