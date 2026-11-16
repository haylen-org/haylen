from collections.abc import Mapping
from types import MappingProxyType


class Frozen:
    """Turns JSON values into values that nobody changes: objects into read-only mappings and arrays into tuples, so a resolved project exposes no list or dictionary that a backend could edit."""

    @staticmethod
    def freeze(value: object) -> object:
        if isinstance(value, Mapping):
            return MappingProxyType({key: Frozen.freeze(item) for key, item in value.items()})
        if isinstance(value, list | tuple):
            return tuple(Frozen.freeze(item) for item in value)
        return value

    @staticmethod
    def thaw(value: object) -> object:
        """Returns a frozen value as plain JSON values, such as for writing it into a file."""
        if isinstance(value, Mapping):
            return {key: Frozen.thaw(item) for key, item in value.items()}
        if isinstance(value, tuple):
            return [Frozen.thaw(item) for item in value]
        return value
