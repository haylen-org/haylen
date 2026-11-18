from collections.abc import Mapping
from dataclasses import dataclass

from haylen.platforms.capability import Capability
from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.override_section import OverrideSection
from haylen.platforms.target_description import TargetDescription


@dataclass(frozen=True)
class PlatformDescription:
    """What a backend declares about itself before it does anything: its name, the version of the backend contract it implements, its own version, its targets, its capabilities, the host tools it needs, the contribution types of plugins and the sections of `haylen.json` it reads. The core picks a backend by the targets it declares and checks a request against the capabilities before it calls the backend."""

    name: str
    contract: int
    version: str
    targets: tuple[TargetDescription, ...]
    capabilities: frozenset[Capability]
    tools: tuple[str, ...]
    contribution_types: Mapping[str, ContributionType]
    override_sections: Mapping[str, OverrideSection]

    def target(self, name: str) -> TargetDescription | None:
        return next((target for target in self.targets if target.name == name), None)
