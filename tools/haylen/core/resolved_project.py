from collections.abc import Mapping
from dataclasses import dataclass

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.frozen import Frozen
from haylen.core.package_layout import PackageLayout
from haylen.core.resolved_value import ResolvedValue
from haylen.core.target_id import TargetId
from haylen.native.native_library import NativeLibrary
from haylen.plugins.merged_contribution import MergedContribution
from haylen.plugins.resolved_plugin import ResolvedPlugin
from haylen.sdk.selected_sdk import SelectedSdk


@dataclass(frozen=True)
class ResolvedProject:
    """Everything one effective target builds, resolved once from `haylen.json` and the plugins and never changed: the app, the target, the SDK slice, the plugins in load order with their parameters, the merged contributions, the settings of the app for the platform, the native libraries, the permissions and the oldest system version. Backends consume it and never resolve plugins again. Its paths name inputs that may change, so a backend detects changes before it reuses a preparation."""

    app: AppManifest
    package: PackageLayout
    target: TargetId
    target_name: str
    platform: str
    engine_configuration: Configuration
    sdk: SelectedSdk
    plugins: tuple[ResolvedPlugin, ...]
    contributions: tuple[MergedContribution, ...]
    settings: Mapping[str, Mapping[str, object]]
    native_libraries: tuple[NativeLibrary, ...]
    permissions: Mapping[str, str]
    minimum_system_version: ResolvedValue

    def of_type(self, name: str) -> tuple[MergedContribution, ...]:
        """Returns the merged contributions of a type, in the order their first owner gave them."""
        return tuple(contribution for contribution in self.contributions if contribution.type == name)

    def setting(self, section: str, key: str, default: object = None) -> object:
        """Returns a setting of a section of `platforms` in `haylen.json` that applies to this target."""
        return self.settings.get(section, {}).get(key, default)

    def setting_origins(self) -> dict[str, dict[str, dict[str, object]]]:
        """The settings of the sections of `platforms` that apply to this target, each with its value and the key of `haylen.json` it comes from, which `project inspect` shows."""
        return {
            section: {key: {"value": Frozen.thaw(value), "origin": f"{self.app.origin} at platforms.{section}.{key}"} for key, value in values.items()} for section, values in self.settings.items()
        }

    def describe(self) -> dict[str, object]:
        return {
            "target": self.target.describe(),
            "targetName": self.target_name,
            "platform": self.platform,
            "kind": self.app.kind.value,
            "engineConfiguration": self.engine_configuration.value,
            "sdk": self.sdk.describe(),
            "minimumSystemVersion": self.minimum_system_version.describe(),
            "plugins": [plugin.describe() for plugin in self.plugins],
            "contributions": [contribution.describe() for contribution in self.contributions],
            "settings": self.setting_origins(),
            "nativeLibraries": [{"name": library.name, "owner": library.owner, "static": library.static} for library in self.native_libraries],
            "permissions": dict(self.permissions),
        }
