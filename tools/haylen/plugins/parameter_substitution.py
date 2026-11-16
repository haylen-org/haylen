import json
from collections.abc import Mapping

from haylen.core.frozen import Frozen
from haylen.plugins.plugin_manifest import PluginManifest


class ParameterSubstitution:
    """Replaces the placeholders `${name}` of the values of contributions with the values of the parameters. A text that is only a placeholder takes the value with its type, a placeholder inside longer text inserts the value as text, and a key or an item whose placeholders name a parameter without a value is left out."""

    MISSING = object()

    @staticmethod
    def apply(value: object, values: Mapping[str, object]) -> object:
        substituted = ParameterSubstitution.substitute(value, values)
        return None if substituted is ParameterSubstitution.MISSING else Frozen.freeze(substituted)

    @staticmethod
    def substitute(value: object, values: Mapping[str, object]) -> object:
        if isinstance(value, str):
            names = PluginManifest.PLACEHOLDER.findall(value)
            if not all(name in values for name in names):
                return ParameterSubstitution.MISSING
            whole = PluginManifest.PLACEHOLDER.fullmatch(value)
            if whole:
                return Frozen.thaw(values[whole[1]])
            return PluginManifest.PLACEHOLDER.sub(lambda match: ParameterSubstitution.text(values[match[1]]), value)

        if isinstance(value, Mapping):
            substituted = {}
            for key, item in value.items():
                new_key = ParameterSubstitution.substitute(key, values)
                new_item = ParameterSubstitution.substitute(item, values)
                if new_key is not ParameterSubstitution.MISSING and new_item is not ParameterSubstitution.MISSING:
                    substituted[str(new_key)] = new_item
            return substituted

        if isinstance(value, tuple | list):
            items = (ParameterSubstitution.substitute(item, values) for item in value)
            return [item for item in items if item is not ParameterSubstitution.MISSING]
        return value

    @staticmethod
    def text(value: object) -> str:
        return value if isinstance(value, str) else json.dumps(Frozen.thaw(value))
