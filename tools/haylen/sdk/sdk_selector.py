from dataclasses import dataclass

from haylen.core.configuration import Configuration
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.repository import Repository
from haylen.core.target_id import TargetId
from haylen.core.version import Version
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_slice import SdkSlice
from haylen.sdk.selected_sdk import SelectedSdk


@dataclass(frozen=True)
class SdkSelector:
    """Chooses the SDK slice of a target after the requirements of the plugins are known and before the project is generated. A slice serves a target only when its system, environment, architecture, binary interface, toolchain and configuration match, it was built from the current engine sources, it runs on the oldest system the app supports and it holds the graphics APIs the app asks for. The selector never builds: the commands that build apps build a missing SDK before they resolve, and the error says which SDK to build."""

    manifests: tuple[SdkManifest, ...]
    revision: str

    def select(self, target: TargetId, target_name: str, engine_configuration: Configuration, minimum_system_version: str | None, graphics_apis: tuple[str, ...]) -> SelectedSdk:
        build = f'Build it with "haylen sdk build --target {target_name} --config {engine_configuration.value}".'
        reasons: list[str] = []
        for manifest in self.manifests:
            for candidate in manifest.slices:
                if candidate.system != target.system or candidate.environment is not target.environment or target.architecture not in candidate.architectures:
                    continue
                reason = SdkSelector.mismatch(manifest, candidate, target, engine_configuration, minimum_system_version, graphics_apis, self.revision)
                if reason is None:
                    return SelectedSdk(manifest, candidate)
                reasons.append(f"The SDK `{Repository.shown(manifest.folder)}` {reason}")

        if not reasons:
            raise IncompatibleSdk(f'No installed SDK holds the engine for "{target_name}" on "{target.architecture}". {build}')
        lines = "\n".join(f"  {reason}" for reason in reasons)
        raise IncompatibleSdk(f'No installed SDK serves "{target_name}" on "{target.architecture}". {build}\n{lines}')

    def lacks(self, target: TargetId, engine_configuration: Configuration) -> bool:
        """Tells whether no installed SDK of the current engine sources in a configuration holds a slice of the system, environment, architecture, binary interface and toolchain of a target, which only a build of its SDK makes."""
        for manifest in self.manifests:
            if manifest.configuration is not engine_configuration or manifest.revision != self.revision:
                continue
            for candidate in manifest.slices:
                built = (candidate.system, candidate.environment, candidate.abi, candidate.toolchain) == (target.system, target.environment, target.abi, target.toolchain)
                if built and target.architecture in candidate.architectures:
                    return False
        return True

    @staticmethod
    def mismatch(
        manifest: SdkManifest,
        candidate: SdkSlice,
        target: TargetId,
        engine_configuration: Configuration,
        minimum_system_version: str | None,
        graphics_apis: tuple[str, ...],
        revision: str,
    ) -> str | None:
        """Returns why a slice does not serve a target, or nothing when it does."""
        if manifest.configuration is not engine_configuration:
            return f'is built in the "{manifest.configuration.value}" configuration, not in "{engine_configuration.value}".'
        if candidate.abi != target.abi or candidate.toolchain != target.toolchain:
            return f'is built for the interface "{candidate.abi}" with "{candidate.toolchain}", and the target needs "{target.abi}" with "{target.toolchain}".'
        if manifest.revision != revision:
            return "is built from other engine sources than the ones of this checkout."
        if minimum_system_version is not None and Version.parse(candidate.minimum_system_version) > Version.parse(minimum_system_version):
            return f'runs on "{target.system}" {candidate.minimum_system_version} and newer, while the app supports {minimum_system_version}.'
        missing = [api for api in graphics_apis if api not in candidate.graphics_apis]
        if missing:
            names = ", ".join(f'"{api}"' for api in missing)
            return f'does not hold the graphics APIs {names} that the app asks for in "window.graphicsApis".'
        return None
