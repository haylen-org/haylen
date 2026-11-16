from collections.abc import Mapping
from dataclasses import dataclass

from haylen.platforms.merge_mode import MergeMode


@dataclass(frozen=True)
class ContributionType:
    """A type of contribution that a backend reads for some of its platforms, such as `swiftPackage` for the Apple platforms: the JSON Schema of its fields besides `type` and `platforms`, the fields that identify one contribution of the type, how contributions of several owners merge and, in the `unique` mode, the list fields that join when owners give one identity, such as the products of a Swift package, while every other field must agree."""

    name: str
    platforms: frozenset[str]
    schema: Mapping[str, object]
    identity: tuple[str, ...]
    merge: MergeMode
    description: str
    joined: tuple[str, ...] = ()
