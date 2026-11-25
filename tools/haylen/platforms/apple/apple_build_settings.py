import json
from collections.abc import Mapping
from dataclasses import dataclass

from haylen.core.errors.invalid_manifest import InvalidManifest


class AppleBuildSettings:
    """Joins the build settings of the app target: the settings of the backend, and the `buildSettings` of the `apple` section of `haylen.json` over them. Every setting is the choice of the app, apart from the settings whose values another owner settles, such as the architectures of the command, the oldest system version or the identity of `haylen.json`. Such a setting takes a value of the app that repeats the value of its owner, and a value that contradicts it stops the preparation with both owners named. The flags of `OTHER_LDFLAGS` of the app join the flags of the backend, and the settings of every configuration hold in the Release configuration too, whose apps ship without symbols."""

    @dataclass(frozen=True)
    class Owned:
        """A setting that another owner settles: its value, the owner, and whether the values are lists of words whose order does not matter, separated by spaces or commas."""

        value: str
        owner: str
        words: bool = False

    KEY = "platforms.apple.buildSettings"
    JOINED = ("OTHER_LDFLAGS",)
    # The settings of the Release configuration: a release ships without symbols, which stay in a dSYM next to the app for crash reports, and strips the app as it builds, so a release that runs is the one that ships.
    RELEASE = {"DEBUG_INFORMATION_FORMAT": "dwarf-with-dsym", "DEPLOYMENT_POSTPROCESSING": "YES", "STRIP_INSTALLED_PRODUCT": "YES", "STRIP_STYLE": "all"}
    RELEASE_OWNER = "the Release configuration, whose apps ship without symbols and keep them in a dSYM,"

    @staticmethod
    def text(value: object) -> str:
        """The value of a setting as Xcode reads it, with `YES` and `NO` for booleans."""
        if isinstance(value, bool):
            return "YES" if value else "NO"
        return str(value)

    @staticmethod
    def agrees(given: object, owned: AppleBuildSettings.Owned) -> bool:
        text = AppleBuildSettings.text(given)
        if owned.words:
            return set(text.replace(",", " ").split()) == set(owned.value.replace(",", " ").split())
        return text == owned.value

    @staticmethod
    def join(base: Mapping[str, object], owned: Mapping[str, AppleBuildSettings.Owned], given: Mapping[str, object], origin: str) -> dict[str, object]:
        """Returns the settings of the app target, or fails with every setting of the app that contradicts its owner."""
        problems = []
        for name, value in given.items():
            settled = owned.get(name)
            if settled is not None and not AppleBuildSettings.agrees(value, settled):
                problems.append(f'The key "{AppleBuildSettings.KEY}.{name}" is {json.dumps(value)}, while {settled.owner} gives {json.dumps(settled.value)}.')
            required = AppleBuildSettings.RELEASE.get(name)
            if required is not None and AppleBuildSettings.text(value) != required:
                problems.append(f'The key "{AppleBuildSettings.KEY}.{name}" is {json.dumps(value)}, while {AppleBuildSettings.RELEASE_OWNER} gives {json.dumps(required)}.')
        if problems:
            lines = "\n".join(f"  {problem}" for problem in problems)
            raise InvalidManifest(f"The build settings of `{origin}` contradict the values of other owners. Remove the keys, or make them agree:\n{lines}", origin=origin)

        settings = {**base, **{name: value for name, value in given.items() if name not in AppleBuildSettings.JOINED}}
        for name in AppleBuildSettings.JOINED:
            if name in given:
                flags = given[name] if isinstance(given[name], list) else [given[name]]
                settings[name] = [*(base.get(name) or ["$(inherited)"]), *flags]
        return settings
