from types import MappingProxyType

from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.merge_mode import MergeMode


class CoreContributions:
    """The contribution types that the core reads itself on every platform, apart from the types of the backends: the oldest system version that a plugin needs, which raises the minimum of the target only where the app allows it, and the native library that Lua loads, which a backend serves when it declares the type for its platforms."""

    MINIMUM_SYSTEM_VERSION = "minimumSystemVersion"
    NATIVE_LIBRARY = "nativeLibrary"

    @staticmethod
    def minimum_system_version(platforms: frozenset[str]) -> ContributionType:
        schema = {
            "type": "object",
            "additionalProperties": False,
            "required": ["version"],
            "properties": {"version": {"type": "string", "pattern": "^\\d+(\\.\\d+){0,2}$"}},
        }
        return ContributionType(
            CoreContributions.MINIMUM_SYSTEM_VERSION,
            platforms,
            MappingProxyType(schema),
            (),
            MergeMode.ACCUMULATE,
            "The oldest version of the system that the plugin runs on.",
        )

    @staticmethod
    def native_library(platforms: frozenset[str]) -> ContributionType:
        """The type of a native library, whose fields follow the entries of the `native` section of `haylen.json` with its paths relative to the plugin folder, and whose name is the id of the plugin."""
        schema = {
            "type": "object",
            "additionalProperties": False,
            "properties": {
                "files": {"type": "object", "additionalProperties": {"type": "string", "minLength": 1}},
                "cmake": {"type": "string", "minLength": 1},
                "link": {"enum": ["dynamic", "static"]},
                "symbols": {"type": "array", "items": {"type": "string"}, "uniqueItems": True},
            },
            "oneOf": [{"required": ["files"], "not": {"required": ["cmake"]}}, {"required": ["cmake"], "not": {"required": ["files"]}}],
        }
        return ContributionType(CoreContributions.NATIVE_LIBRARY, platforms, MappingProxyType(schema), (), MergeMode.ACCUMULATE, "A native library that Lua loads with native.load.")
